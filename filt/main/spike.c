#include <rsf.h>

int main(int argc, char* argv[])
{ 
    int i, is, dim, n[SF_MAX_DIM], ii[SF_MAX_DIM];
    int nsp, **k, n1, n2, i1, i2, kk;
    char key[7], *label;
    float f, *trace, *mag;
    sf_file spike;

    sf_init (argc,argv);
    spike = sf_output("out");
    sf_setformat(spike,"native_float");

    /* dimensions */
    for (i=0; i < SF_MAX_DIM; i++) {
	snprintf(key,3,"n%d",i+1);
	if (!sf_getint(key,n+i)) break;
	sf_putint(spike,key,n[i]);
    }

    if (0==i) sf_error("Need n1=");
    dim=i;
    
    /* basic parameters */
    for (i=0; i < dim; i++) {
	snprintf(key,3,"o%d",i+1);
	if (!sf_getfloat(key,&f)) f=0.;
	sf_putfloat(spike,key,f);

	snprintf(key,3,"d%d",i+1);
	if (!sf_getfloat(key,&f)) f = (i==0)? 0.004: 0.1;
	sf_putfloat(spike,key,f);

	snprintf(key,7,"label%d",i+1);
	if (NULL == (label = sf_getstring(key)))
	    label = (i==0)? "Time (sec)":"Distance (km)";
	sf_putstring(spike,key,label);
    }
	
    if (!sf_getint("nsp",&nsp)) nsp=1;
    mag = sf_floatalloc (nsp);
    k = sf_intalloc2 (nsp,dim);

    for (i=0; i < dim; i++) {
	snprintf(key,3,"k%d",i+1);
	if (!sf_getints(key,k[i],nsp)) {
	    for (is=0; is < nsp; is++) {
		k[i][is]=-1;
	    }
	} else {
	    for (is=0; is < nsp; is++) {
		if (k[i][is] > n[i]) 
		    sf_error("Invalid k%d[%d]=%d > n%d=%d",
			     i+1,is+1,k[i][is],i+1,n[i]);
		k[i][is]--; /* C notation */
	    }
	}
    }

    if (!sf_getfloats("mag",mag,nsp)) {
	for (is=0; is < nsp; is++) {
	    mag[is]=1.;
	}
    }

    n1 = n[0];
    n2 = sf_filesize(spike)/n1;

    trace = sf_floatalloc (n[0]);

    for (i2=0; i2 < n2; i2++) {
	sf_line2cart(dim-1, n+1, i2, ii);
	/* zero trace */
	for (i1=0; i1 < n1; i1++) trace[i1]=0.;
	/* put spikes in it */
	for (is=0; is < nsp; is++) {
	    for (i=1; i < dim; i++) {
		kk = k[i][is];
		if ((kk < -1) || (kk >= 0 && kk != ii[i])) break;	    
	    }
	    if (i < dim) continue;
	    kk = k[0][is];
	    if (kk >= 0) { /* one spike per trace */
		trace[kk] += mag[is];
	    } else {
		for (i1=0; i1 < n1; i1++) {
		    trace[i1] += mag[is];
		}
	    }
	}
	sf_write(trace,sizeof(float),n1,spike);
    }

    exit (0);
}

