#ifndef _gazdag_h
#define _gazdag_h

void gazdag_init (float eps1, int nt1, float dt1, 
		  int nz1, float dz1, float *vt1);
void gazdag_close ();
void gazdag (bool inv, float k2, float complex *p, float complex *q);

#endif
