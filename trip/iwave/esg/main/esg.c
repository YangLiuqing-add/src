/* driver for iwave with elastic staggered grid model
   esg.c
*/
/*============================================================================
 *                            BEGIN INCLUDES 
 * ============================================================================*/

#include <iwave.h>
#include <esgn.h>
#include <trace_term.h>
#include <pointsrc.h>
#include <sampler.h>
#include <parser.h>
#include <esg_selfdoc.h>
#include <esg_movie.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#define NSTR 128

/* #define IWAVE_EXTEND_MODEL */

/* uncomment to write to the rk-dep output stream at every major step 
   #define VERBOSE
*/
/* uncomment to write to the rk-dep output stream at every time step 
   #define VERBOSE_STEP
*/
/*============================================================================
 *                             END INCLUDES 
 * ============================================================================*/

int main(int argc, char ** argv) {

  int err=0;               /* error flag         */
  FILE * stream;           /* output stream      */
  PARARRAY pars;           /* parameter array    */
  IWAVE state;             /* model state        */
  SAMPLER trace;           /* trace sampler      */
  POINTSRC * ptsrc=NULL;   /* acoustic pt src    */
  SAMPLER * arrsrc=NULL;   /* source array       */
  int towed=0;             /* fixed vs. towed    */
  RPNT d;                  /* workspace for i/o  */
  RPNT o;                  /* workspace for i/o  */
  MOVIE mt;                /* movie writer       */
  char * srctype;          /* string length      */
  char hdrkey[NSTR];       /* key = hdrfile      */
  char datakey[NSTR];      /* key = datafile     */
  char srckey[NSTR];       /* key = source       */
  IPNT sindex;             /* source array index */
  RPNT mult  ;             /* multiplier array   */
  RPNT scoord;             /* source cell loc    */
  int dump_term=0;         /* trace info dump    */
  int istart=0;            /* start index        */
  int ts;                  /* thread support lvl */
  int rk;                  /* process rank       */

  /* int i; */

  /*******************************************************************
   ****************** INITIALIZE PARALLEL ENVIRONMENT ****************
   * initialize parallel environment, output stream, and param table *
   *******************************************************************/

  xargc=argc;
  xargv=argv;

  ts=0;
#ifdef IWAVE_USE_MPI
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &ts);
#endif

  /* initparallel(ts); */
  initparallel_global(ts);

  /* self-doc if no args */
  rk=retrieveGlobalRank();
  if (rk==0) requestdoc(1);

#ifdef VERBOSE
#ifdef IWAVE_USE_MPI
  if (rk==0) {
#endif
    fprintf(stderr,"Global MPI_Comm_size = %d\n",retrieveGlobalSize());
    fprintf(stderr,"initoutstream\n");
#ifdef IWAVE_USE_MPI
  }
#endif
#endif

  err=initoutstream(&stream,retrieveGlobalRank(),retrieveGlobalSize());
  if (err) {
    fprintf(stderr,"ERROR: main from initoutstream. ABORT\n");
    abortexit(err,&pars,&stream);
  }

#ifdef VERBOSE
  fprintf(stream,"readinput\n");
  fflush(stream);
#endif
  readinput(&pars,stream,argc,argv);

#ifdef VERBOSE
  fprintf(stream,"paramtable:\n");
  ps_printall(pars,stream);
  fflush(stream);
#endif

#ifdef VERBOSE
  fprintf(stream,"initparallel_local \n");
  fflush(stream);
#endif
  initparallel_local(pars,stream);

  /*******************************************************************
   ****************** INITIALIZE PARALLEL ENVIRONMENT ****************
   *                         finished                                *
   *******************************************************************/

  /* check comm status - skip the rest if no comm */
#ifdef IWAVE_USE_MPI
  if (retrieveGroupID() == MPI_UNDEFINED) {
    fprintf(stream,"NOTE: finalize MPI, cleanup, exit\n");
  }
  else {
#endif

    xargc=argc;
    /* assign key strings */
    strcpy(hdrkey,"hdrfile");
    strcpy(datakey,"datafile");
    strcpy(srckey,"source");

    /* assign default sampler params */
    IASN(sindex,D_S);
    RASN(mult,RPNT_0);
    mult[0]=1;
    RASN(scoord,RPNT_0);
  

    /* extract trace sampler verbosity flag */
    ps_ffint(pars,"dump_term",&dump_term);

    /* extract source key - default = "point" */
    if (ps_ffcstring(pars,"srctype",&srctype)) {
      srctype=(char *)malloc(6*sizeof(char));
      strcpy(srctype,"point");
    }

    /* construct iwave object */
#ifdef VERBOSE
    fprintf(stream,"iwave_construct\n");
    fflush(stream);
#endif

    err=iwave_construct(&state,&pars,stream,&esg_modelinit);
    if (err) {
      fprintf(stream,"ERROR: main from iwave_construct. ABORT\n");
      iwave_destroy(&state);
      abortexit(err,&pars,&stream);
    }

#ifdef VERBOSE
    fprintf(stream,"iwave_printf\n");
    fflush(stream);
#endif

    iwave_printf(&state,&pars,stream);

#ifdef VERBOSE
    fprintf(stream,"data sampler\n");
    fflush(stream);
#endif

    /* construct traceterm object */
    sindex[0] = D_P0;
    err=sampler_construct(&trace,&pars,sindex,mult,scoord,0,hdrkey,datakey,stream);
    if (err) {
      fprintf(stream,"ERROR: main from sampler_construct. ABORT\n");
      abortexit(err,&pars,&stream);
    }
    /* initial reassignment of source coordinates */
    RASN(scoord,trace.t.tg.src[trace.t.tg.xrec]);

#ifdef VERBOSE
    fprintf(stream,"movie sampler\n");
    fflush(stream);
#endif
    /* construct movie object */
    err = esg_movie_construct(&mt, stream);
    if (err) {
      fprintf(stream,"ERROR: main from movie_construct. ABORT\n");
      abortexit(err,&pars,&stream);
    }

    /* postconditon: tracegeom trace.tg now equipped with 
     * - number of shot records
     * - shot coordinates for each record
     * - file offset for each record
     */

    /* NOTE: pointsrc object has trivial default construction */

#ifdef VERBOSE
    fprintf(stream,"loop\n");
    fflush(stream);
#endif

    /* simulation loop
     * roles of "while" conditions:
     * tg.xrec = index of next source to be computed
     * tg.irec = index of current source being computed
     * tg.nrec = index of final source 
     * sampler_init updates these via call to init_tracegeom
     * sampler_init precondition (before sim loop): xrec=irec=first
     * (index of first record in current group)
     * sampler_init postcondition: irec=xrec; xrec++;
     * so at bottom of step i (=0,1,2...) through sim loop,
     * irec=i, xrec=i+1.
     * - test xrec against last: for versions which permit incoherent 
     *   models, must preceed call to iwave_static_init,
     *   even though it is tested also in traceterm_init, because
     *   latter preceeds former.
     * - iwave_static_init: uses source coordinates to read in proper
     *   coefficient arrays, which may depend on source. computes
     *   time step satisfying stability conditions for scheme.
     * - sampler_init: reads trace geometry for shot, allocates and 
     *   initializes sampling-related arrays
     */
    while ( (trace.t.tg.xrec < trace.t.tg.last+1)) {

      /* iwave_static_init should succeed in record range [0,nrec) */
#ifndef IWAVE_EXTEND_MODEL
      if (trace.t.tg.xrec == trace.t.tg.first) 
	err = iwave_static_init(&state,&pars,stream,0);
#else
      err = iwave_static_init(&state,&pars,stream,trace.t.tg.xrec);
#endif
      if(err){  
	fprintf(stream,"ERROR: main from iwave_static_init, xrec = %d, err = %d. ABORT\n", trace.t.tg.xrec, err);
	abortexit(err,&pars,&stream);
      }
      
      /* sampler_init should succeed in record range [first,last) */

      /*      fprint_grid(stderr,(state.model).g); */
      if (sampler_init(&trace,&(state.model),&pars,stream)) {
	fprintf(stream,"ERROR: main from sampler_init. ABORT\n");
	abortexit(err,&pars,&stream);
      }

      fprintf(stderr,"IWAVE::esg rkw=%d rk=%d isrc=%d\n",
              retrieveGlobalRank(),retrieveRank(),trace.t.tg.irec);


      if (dump_term) sampler_fprint(&trace, stream);
    
      /* pointsrc_init: initializes source wavelet with proper
       * calibration, determines start time of simulation 
       */
#ifdef VERBOSE
      fprintf(stream,"initialize source\n");
      fflush(stream);
#endif	
      if (!strcmp(srctype,"point")) {
	if (!(ptsrc=(POINTSRC *)malloc(sizeof(POINTSRC)))) {
	  fprintf(stream,"ERROR: failed to allocate point source. ABORT\n");
	  abortexit(err,&pars,&stream);
	}
	err=pointsrc_init(ptsrc,&(state.model),&pars,&(trace.t.tg),stream);
	if (err) {
	  fprintf(stream,"ERROR: main from pointsrc_init. ABORT\n");
	  abortexit(err,&pars,&stream);
	}
	if (dump_term) pointsrc_fprint(ptsrc,stream);
	istart=ptsrc->istart;
      }
      else if (!strcmp(srctype,"array")) {
	if (!(arrsrc=(SAMPLER *)malloc(sizeof(SAMPLER)))) {
	  fprintf(stream,"ERROR: failed to allocate array source. ABORT\n");
	  abortexit(err,&pars,&stream);
	}
	ps_ffint(pars,"towed",&towed);
	if (!towed) {
	  RASN(scoord,RPNT_0);
	}
	/* NOTE THIS PROBABLY ISN'T ANY INTERESTING SOURCE */
        IASN(sindex,D_P);
	err=sampler_construct(arrsrc,&pars,sindex,mult,scoord,1,NULL,srckey,stream);
	if (err) {
	  fprintf(stream,"ERROR: main from sampler_construct. ABORT\n");
	  abortexit(err,&pars,&stream);
	}
	err=sampler_init(arrsrc,&(state.model),&pars,stream);
	if (err) {
	  fprintf(stream,"ERROR: main from sampler_construct. ABORT\n");
	  abortexit(err,&pars,&stream);
	}
	istart=(arrsrc->t).istart;
	if (dump_term) sampler_fprint(arrsrc,stream);
      }
      else {
	if (srctype) fprintf(stream,"ERROR: unknown source option = %s\n",srctype);
	else fprintf(stream,"ERROR: unknown source option\n");
	abortexit(err,&pars,&stream);
      }
      
#ifdef VERBOSE
      fprintf(stream,"initialize movie\n");
      fflush(stream);
#endif
      err=movie_init(&mt, &(state.model), &pars, &(trace.t.tg), stream);
      if (err) {
	fprintf(stream,"ERROR: main from movie_init. ABORT\n");
	abortexit(err,&pars,&stream);
      }
      if (dump_term) movie_fprint(&mt, stream);

#ifdef VERBOSE
      fprintf(stream,"initialize state\n");
      fflush(stream);
#endif

      /* initialize dynamic fields */
      iwave_dynamic_init(&state,istart);
      esg_dynamic_init(&(state.model));
#ifdef VERBOSE
      fprintf(stream,"time loop\n"); 
      fflush(stream);
#endif

      /*
	preptime(state.stat_times, state.stat_calls, &state);
      */

      /* time stepping */
      while (state.model.tsind.it <= trace.t.istop) {

#ifdef VERBOSE_STEP
	fprintf(stream,"step - update dynamical fields\n");
	fflush(stream);
#endif
	err=iwave_run(&state,stream);
	if (err) {
	  fprintf(stream,"ERROR: main from iwave_run. ABORT\n");
	  abortexit(err,&pars,&stream);
	}

#ifdef VERBOSE_STEP
	fprintf(stream,"poststep - source\n");
	fflush(stream);
#endif
        /** point source added on every normal stress field */
	if (ptsrc) {
	  err=pointsrc_run(ptsrc,&(state.model));
	  if (err) {
	    fprintf(stream,"ERROR: main from pointsrc_run. ABORT\n");
	    abortexit(err,&pars,&stream);
	  }
	}
	if (arrsrc) {
	  err=sampler_run(arrsrc,&(state.model));
	  if (err) {
	    fprintf(stream,"ERROR: main from pointsrc_run. ABORT\n");
	    abortexit(err,&pars,&stream);
	  }
	}

#ifdef VERBOSE_STEP
	fprintf(stream,"poststep - traces\n");
	fflush(stream);
#endif
      
	err=sampler_run(&trace,&(state.model));
	if (err) {
	  fprintf(stream,"ERROR: main from sampler_run. ABORT\n");
	  abortexit(err,&pars,&stream);
	}

#ifdef VERBOSE_STEP
	fprintf(stream,"poststep - movie\n");
	fflush(stream);
#endif

#ifndef IWAVE_EXTEND_MODEL
	err=movie_run(&mt,&(state.model),stream,0);
#else
	err=movie_run(&mt,&(state.model),stream, trace.t.tg.irec);
#endif
	if (err) {
	  fprintf(stream,"ERROR: main from movie_run. ABORT\n");
	  abortexit(err,&pars,&stream);
	}

#ifdef VERBOSE_STEP
	fprintf(stream,"update time step info\n");
	fflush(stream);
#endif
	next_step(&(state.model.tsind),state.model.tsinfo);
      
      }

      /*
	printtime(state.stat_times, state.stat_calls, &state, stream);
      */
      /* after time step loop, write data - this block of code should be
	 part of the operator() method of the FO built by the output
	 policy class in TSOpt.
      */
    
      get_d(d, state.model.g);
      get_o(o, state.model.g);

#ifdef VERBOSE
      fprintf(stream,"writetraces\n");
      fflush(stream);
#endif

      err=writetraces(&(trace.t.tg),d,o,stream);

      if (err) {
	fprintf(stream,"ERROR: main from tracegeom::writetraces. ABORT\n");
	abortexit(err,&pars,&stream);
      }

#ifdef VERBOSE
      fprintf(stream,"destroy source objects\n");
      fflush(stream);
#endif
      if (ptsrc) free(ptsrc);
      if (arrsrc) free(arrsrc);
    }

    if (srctype) free(srctype);

    /* destroy static objects and exit */

#ifdef VERBOSE
    fprintf(stream,"movie_destroy\n");
    fflush(stream);
#endif

    movie_destroy(&mt);

#ifdef VERBOSE
    fprintf(stream,"sampler_destroy\n");
    fflush(stream);
#endif

    sampler_destroy(&trace);

#ifdef VERBOSE
    fprintf(stream,"iwave_destroy\n");
    fflush(stream);
#endif

    iwave_destroy(&state);

#ifdef IWAVE_USE_MPI

  } /* end nontriv comm branch */

#ifdef VERBOSE
  fprintf(stream,"MPI_Finalize\n");
  fflush(stream);
#endif
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
#endif

#ifdef VERBOSE
  fprintf(stream,"quietexit\n");
  fflush(stream);
#endif

  quietexit(&pars,&stream);

  exit(0);
}
