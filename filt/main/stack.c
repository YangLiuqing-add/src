#include <string.h>
#include <stdio.h>

#include <rsf.h>

int main(int argc, char* argv[])
{
    int j, n1, n2, n3, i2, i3, ni, *fold = NULL;
    size_t i, n;
    sf_file in, out;
    char key1[7], key2[7], *val;
    bool norm;
    float *trace, *sum;
    sf_datatype type;

    sf_init (argc, argv);
    in = sf_input ("in");
    out = sf_output ("out");

    if (!sf_histint(in,"n1",&n1)) sf_error("No n1= in input");
    n = (size_t) n1;

    type = sf_gettype (in);
    if (SF_FLOAT != type) {
	if (SF_COMPLEX == sf_gettype (in)) {
	    n *= 2; 
/* possibly incorrect norm for complex data */
	} else {
	    sf_error("Incorrect data type in input");
	}
    }

    if (!sf_histint(in,"n2",&n2)) sf_error("No n2= in input");

    n3 = 1;
    for (j=2; j < SF_MAX_DIM; j++) {
	sprintf(key1,"n%d",j+1);
	sprintf(key2,"n%d",j);
	if (!sf_histint(in,key1,&ni)) break;
	sf_putint(out,key2,ni);
	n3 *= ni;
	
	sprintf(key1,"o%d",j+1);
	sprintf(key2,"o%d",j);
	if (NULL != (val = sf_histstring(in,key1))) 
	    sf_putstring(out,key2,val);

	sprintf(key1,"d%d",j+1);
	sprintf(key2,"d%d",j);
	if (NULL != (val = sf_histstring(in,key1))) 
	    sf_putstring(out,key2,val);

	sprintf(key1,"label%d",j+1);
	sprintf(key2,"label%d",j);
	if (NULL != (val = sf_histstring(in,key1))) 
	    sf_putstring(out,key2,val);
    }

    if (!sf_getbool("norm",&norm)) norm = true;
		
    if (norm) fold = sf_intalloc (n);
    trace = sf_floatalloc (n);
    sum   = sf_floatalloc (n);
    
    for (i3=0; i3 < n3; i3++) {
	memset (sum,0,n*sizeof(float));
	if (norm) memset (fold,0,n*sizeof(int));
	
	for (i2=0; i2 < n2; i2++) {
	    sf_read (trace, sizeof(float), n, in);
	    for (i=0; i < n; i++) {
		sum[i] += trace[i];
		if (norm && (0.0 != trace[i])) fold[i]++; 
	    }
	}
	if (norm) {
	    for (i=0; i < n; i++) {
		if (fold[i] > 0) sum[i] /= fold[i];
	    }
	}
	sf_write(sum, sizeof(float), n, out); 
    }
    
    exit (0);
}
