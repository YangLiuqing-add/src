#include <rsf.h>

#include "eno3.h"
#include "eno2.h"
#include "eno.h"

/* concrete data type */
struct Eno3 {
    int order, ng, n1, n2, n3;
    eno **ent;
    eno2 jnt;
    float **f, **f1;
};

/* 
   Function: eno3_init
   -------------------
   Initialize interpolation object
   order      - interpolation order
   n1, n2, n3 - data dimensions
*/
eno3 eno3_init (int order, int n1, int n2, int n3)
{
    eno3 pnt;
    int i2, i3;
    
    pnt = (eno3) sf_alloc(1,sizeof(*pnt));
    pnt->order = order; 
    pnt->n1 = n1; 
    pnt->n2 = n2;
    pnt->n3 = n3;
    pnt->ng = 2*order-2;
    if (pnt->ng > n2 || pnt->ng > n3) 
	sf_error("%s: ng=%d is too big",__FILE__,pnt->ng);
    pnt->jnt = eno2_init (order, pnt->ng, pnt->ng);
    pnt->f  = sf_floatalloc2(pnt->ng,pnt->ng);
    pnt->f1 = sf_floatalloc2(pnt->ng,pnt->ng);
    pnt->ent = (eno**) sf_alloc(n3,sizeof(eno*));
    for (i3 = 0; i3 < n3; i3++) {
	pnt->ent[i3] = (eno*) sf_alloc(n2,sizeof(eno));
	for (i2 = 0; i2 < n2; i2++) {
	    pnt->ent[i3][i2] = eno_init (order, n1);
	}
    }

    return pnt;
}

/* 
   Function: eno3_set
   ------------------
   Set the interpolation table
   pnt           - ENO object
   c[n3][n2][n1] - data
   Note: c can be changed or freed afterwords
*/
void eno3_set (eno3 pnt, float*** c)
{
    int i2, i3;
    
    for (i3 = 0; i3 < pnt->n3; i3++) {
	for (i2 = 0; i2 < pnt->n2; i2++) {
	    eno_set (pnt->ent[i3][i2], c[i3][i2]);
	}
    }
}

/* 
   Function: eno3_set1
   ------------------_
   Set the interpolation table
   pnt           - ENO object
   c[n3*n2*n1] - data
   Note: c can be changed or freed afterwords
*/
void eno3_set1 (eno3 pnt, float* c)
{
    int i2, i3;
    
    for (i3 = 0; i3 < pnt->n3; i3++) {
	for (i2 = 0; i2 < pnt->n2; i2++) {
	    eno_set (pnt->ent[i3][i2], c+(pnt->n1)*(i2+(pnt->n2)*i3));
	}
    }
}

/* 
   Function: eno3_close
   --------------------
   Free internal storage
*/
void eno3_close (eno3 pnt)
{
    int i2, i3;
    
    eno2_close (pnt->jnt);
    for (i3 = 0; i3 < pnt->n3; i3++) {
	for (i2 = 0; i2 < pnt->n2; i2++) {
	    eno_close (pnt->ent[i3][i2]);
	}
	free (pnt->ent[i3]);
    }
    free (pnt->ent);
    free (pnt->f[0]);
    free (pnt->f);
    free (pnt->f1[0]);
    free (pnt->f1);
    free (pnt);
}

/* 
   Function: eno3_apply
   --------------------
   Apply interpolation
   pnt   - ENO object
   i,j,k - grid location
   x,y,z - offsets from grid
   f     - data value (output)
   f1[3] - derivative value (output)
   what  - flag of what to compute: 
            FUNC - function value
	    DER  - derivative value
	    BOTH - both
*/
void eno3_apply (eno3 pnt, 
		 int i, int j, int k, 
		 float x, float y, float z,
		 float* f, float* f1, der what)
{
    int i2, i3, b2, b3;
    float g;
    
    if (j-pnt->order+2 < 0) {
	b2 = 0;
    } else if (j+pnt->order-1 > pnt->n2-1) {
	b2 = pnt->n2 - pnt->ng;
    } else {
	b2 = j-pnt->order+2;
    }
    
    j -= b2;
    
    
    if (k-pnt->order+2 < 0) {
	b3 = 0;
    } else if (k+pnt->order-1 > pnt->n3-1) {
	b3 = pnt->n3 - pnt->ng;
    } else {
	b3 = k-pnt->order+2;
    }
    
    k -= b3;
    
    for (i3 = 0; i3 < pnt->ng; i3++) {
	for (i2 = 0; i2 < pnt->ng; i2++) {
	    eno_apply (pnt->ent[b3+i3][b2+i2],i,x,
		       &(pnt->f[i3][i2]),
		       &(pnt->f1[i3][i2]),
		       (what==FUNC? FUNC: BOTH));
	}
    }
    
    eno2_set (pnt->jnt,pnt->f);
    eno2_apply (pnt->jnt,j,k,y,z,f,f1+1,what);
    
    if (what != FUNC) {
	eno2_set (pnt->jnt,pnt->f1);
	eno2_apply(pnt->jnt,j,k,y,z,f1,&g,FUNC);
    }
}
