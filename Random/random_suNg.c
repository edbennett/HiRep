/***************************************************************************\
* Copyright (c) 2008, Claudio Pica                                          *   
* All rights reserved.                                                      * 
\***************************************************************************/

/*******************************************************************************
*
* File random_suNg.c
*
* Generation of uniformly distributed SU(Ng) matrices
*
*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "suN.h"
#include "random.h"
#include "utils.h"

/* static variables for random_suNg */
static double s[4];
static suNg_vector *pu1,*pu2;


extern void random_su2(double rho,double s[]);


void random_suNg_unit_vector(suNg_vector *v)
{
  double norm=0.0,fact;
  
  while ((1.0+norm)==1.0)   {
    gauss((double*)v,(sizeof(suNg_vector)/sizeof(double)));
    _vector_prod_re_g(norm,*v,*v);
    norm=sqrt(norm);
  }
  
  fact=1.0/norm;
  _vector_mul_g(*v,fact,*v);
}

void gaussian_suNg_vector(suNg_vector *v)
{
   gauss((double*)v,(sizeof(suNg_vector)/sizeof(double)));
}


/* generates a random SU(N) matrix via SU(2) rotations */
static void rotate(void) /* same as in cabmar */
{
	  int i;
	  complex z1,z2;
	  complex *cu1, *cu2;
				  
	  cu1 = &((*pu1).c[0]);
	  cu2 = &((*pu2).c[0]);
						  
	  for (i=0; i<NG; ++i) {
		    z1.re=s[0]*(*cu1).re-s[1]*(*cu2).im+s[2]*(*cu2).re-s[3]*(*cu1).im;
		    z1.im=s[0]*(*cu1).im+s[1]*(*cu2).re+s[2]*(*cu2).im+s[3]*(*cu1).re;
		    z2.re=s[0]*(*cu2).re-s[1]*(*cu1).im-s[2]*(*cu1).re+s[3]*(*cu2).im;
		    z2.im=s[0]*(*cu2).im+s[1]*(*cu1).re-s[2]*(*cu1).im-s[3]*(*cu2).re;
		    (*cu1) = z1; 
		    (*cu2) = z2; 
		    ++cu1;
		    ++cu2;
	  }
}

#ifndef GAUGE_SON
void random_suNg(suNg *u) {
  int i,j;

	_suNg_unit(*u);
  pu1=(suNg_vector*)(u);
				  
  for (i=0; i<NG; ++i) {
    pu2 = pu1 + 1;
    for (j=i+1; j<NG; ++j) {
		  random_su2(0.0,s);
      rotate();
      ++pu2; 
    } 
	  ++pu1; 
  }

}
#else
void random_suNg(suNg *u) {
  suNg tmp;
  double gr[NG*NG];
  int i;
  do {
    gauss(gr,NG*NG);
    for (i=0;i<NG*NG;i++){
      tmp.c[i]=gr[i];
    }
  } while (!project_to_suNg_real(u,&tmp));
}

#endif

