#include <math.h>
#include <string.h>

#include <unistd.h>

#include <rsf.h>

static void check_compat (int esize, 
			  size_t nin, sf_file *in, int dim, const int *n);
static void add_float (bool collect, size_t nbuf, float* buf, float* bufi, 
		       char cmode, float scale, float add, 
		       bool abs_flag, bool log_flag, 
		       bool sqrt_flag, bool exp_flag);
static void add_int (bool collect, size_t nbuf, int* buf, int* bufi, 
		     char cmode, float scale, float add, 
		     bool abs_flag, bool log_flag, 
		     bool sqrt_flag, bool exp_flag);
static void add_complex (bool collect, size_t nbuf, 
			 float complex* buf, float complex* bufi, 
			 char cmode, float scale, float add, 
			 bool abs_flag, bool log_flag, 
			 bool sqrt_flag, bool exp_flag);

int main (int argc, char* argv[])
{
    int i, dim, n[SF_MAX_DIM], esize;
    size_t j, nin, nbuf = BUFSIZ, nsiz;
    sf_file *in, out;
    float *scale, *add;
    bool *sqrt_flag, *abs_flag, *log_flag, *exp_flag;
    char cmode, *mode, buf[BUFSIZ], bufi[BUFSIZ], *bi;
    sf_datatype type;

    sf_init (argc, argv);
    in = (sf_file*) sf_alloc ((size_t) argc,sizeof(sf_file));
    
    if (0 != isatty(fileno(stdin))) { /* no input file in stdin */
	nin=0;
    } else {
	in[0] = sf_input("in");
	nin=1;
    }

    for (i=1; i< argc; i++) { /* collect inputs */
	if (NULL != strchr(argv[i],'=')) continue; /* not a file */
	in[nin] = sf_input(argv[i]);
	nin++;
    }
    if (0==nin) sf_error ("no input");
    
    out = sf_output ("out");
    
    scale = sf_floatalloc (nin);
    add   = sf_floatalloc (nin);  

    sqrt_flag = sf_boolalloc (nin);
    abs_flag  = sf_boolalloc (nin);
    log_flag  = sf_boolalloc (nin);
    exp_flag  = sf_boolalloc (nin);
    
    for (j = 0; j < nin; j++) {
	scale[j] = 1.;
	add[j] = 0.;
	sqrt_flag[j] = abs_flag[j] = log_flag[j] = exp_flag[j] = false;
    }

    (void) sf_getfloats("scale",scale,nin);
    (void) sf_getfloats("add",add,nin);

    (void) sf_getbools("sqrt",sqrt_flag,nin);
    (void) sf_getbools("abs",abs_flag,nin);
    (void) sf_getbools("log",log_flag,nin);
    (void) sf_getbools("exp",exp_flag,nin);

    mode = sf_getstring("mode");
    cmode = (NULL==mode)? 'a':mode[0];

    dim = sf_filedims(in[0],n);
    for (nsiz=1, i=0; i < dim; i++) {
	nsiz *= n[i];
    }

    if (!sf_histint(in[0],"esize",&esize)) {
	esize=4;
    } else if (0>=esize) {
	sf_error("wrong esize=%d",esize);
    }
    check_compat(esize,nin,in,dim,n);

    sf_fileflush(out,in[0]);
    type = sf_gettype (in[0]);

    for (nbuf /= esize; nsiz > 0; nsiz -= nbuf) {
	if (nbuf > nsiz) nbuf=nsiz;

	for (j=0; j < nin; j++) {
	    bi = (j==0)? buf:bufi;
	    sf_read(bi,(size_t) esize,nbuf,in[j]);
	    switch(type) {
		case SF_FLOAT:
		    add_float(j != 0, nbuf,
			      (float*) buf,(float*) bi, 
			      cmode, scale[j], add[j], 
			      abs_flag[j], log_flag[j], 
			      sqrt_flag[j], exp_flag[j]);
		    break;
		case SF_COMPLEX:		    
		    add_complex(j != 0, nbuf,
				(float complex*) buf,(float complex*) bi, 
				cmode, scale[j], add[j], 
				abs_flag[j], log_flag[j], 
				sqrt_flag[j], exp_flag[j]);
		    break;
		case SF_INT:
		    add_int(j != 0, nbuf,
			    (int*) buf,(int*) bi, 
			    cmode, scale[j], add[j], 
			    abs_flag[j], log_flag[j], 
			    sqrt_flag[j], exp_flag[j]);
		    break;
		default:
		    sf_error("wrong type");
		    break;
	    }
	}
	sf_write(buf,(size_t) esize,nbuf,out);
    }
    
    exit (0);
}

static void add_float (bool collect, size_t nbuf, float* buf, float* bufi, 
		       char cmode, float scale, float add, 
		       bool abs_flag, bool log_flag, 
		       bool sqrt_flag, bool exp_flag)
{
    size_t j;
    float f;

    for (j=0; j < nbuf; j++) {
	f = bufi[j];
	if (abs_flag)    f = fabsf(f);
	f += add;
	if (log_flag)    f = logf(f);
	if (sqrt_flag)   f = sqrtf(f);
	if (1. != scale) f *= scale;
	if (exp_flag)    f = expf(f);
	if (collect) {
	    switch (cmode) {
		case 'p':
		case 'm':
		    buf[j] *= f;
		    break;
		case 'd':
		    buf[j] /= f;
		    break;
		default:
		    buf[j] += f;
		    break;
	    }
	}
    }
}

static void add_int (bool collect, size_t nbuf, int* buf, int* bufi, 
		     char cmode, float scale, float add, 
		     bool abs_flag, bool log_flag, 
		     bool sqrt_flag, bool exp_flag)
{
    size_t j;
    float f;

    for (j=0; j < nbuf; j++) {
	f = (float) bufi[j];
	if (abs_flag)    f = fabsf(f);
	f += add;
	if (log_flag)    f = logf(f);
	if (sqrt_flag)   f = sqrtf(f);
	if (1. != scale) f *= scale;
	if (exp_flag)    f = expf(f);
	if (collect) {
	    switch (cmode) {
		case 'p':
		case 'm':
		    buf[j] *= f;
		    break;
		case 'd':
		    buf[j] /= f;
		    break;
		default:
		    buf[j] += f;
		    break;
	    }
	}
    }
}

static void add_complex (bool collect, size_t nbuf, 
			 float complex* buf, float complex* bufi, 
			 char cmode, float scale, float add, 
			 bool abs_flag, bool log_flag, 
			 bool sqrt_flag, bool exp_flag)
{
    size_t j;
    float complex c;

    for (j=0; j < nbuf; j++) {
	c = bufi[j];
	if (abs_flag)    c = cabsf(c);
	c += add;
	if (log_flag)    c = clogf(c);
	if (sqrt_flag)   c = csqrtf(c);
	if (1. != scale) c *= scale;
	if (exp_flag)    c = cexpf(c);
	if (collect) {
	    switch (cmode) {
		case 'p':
		case 'm':
		    buf[j] *= c;
		    break;
		case 'd':
		    buf[j] /= c;
		    break;
		default:
		    buf[j] += c;
		    break;
	    }
	}
    }
}

static void check_compat (int esize, 
			  size_t nin, sf_file *in, int dim, const int *n) 
{
    int ni, id;
    size_t i;
    float f, fi;
    char key[3];
    const float tol=1.e-5;
    
    for (i=1; i < nin; i++) {
	if (!sf_histint(in[i],"esize",&ni) || ni != esize)
	    sf_error ("esize mismatch: need %d",esize);
	for (id=1; id <= dim; id++) {
	    (void) snprintf(key,3,"n%d",id);
	    if (!sf_histint(in[i],key,&ni) || ni != n[id-1])
		sf_error("%s mismatch: need %d",key,n[id-1]);
	    (void) snprintf(key,3,"d%d",id);
	    if (sf_histfloat(in[0],key,&f) && 
		(!sf_histfloat(in[i],key,&fi) || (fabs(fi-f) > tol*fabs(f))))
		sf_warning("%s mismatch: need %g",key,f);
	    (void) snprintf(key,3,"o%d",id);
	    if (sf_histfloat(in[0],key,&f) && 
		(!sf_histfloat(in[i],key,&fi) || (fabs(fi-f) > tol*fabs(f))))
		sf_warning("%s mismatch: need %g",key,f);
	}
    }
}
