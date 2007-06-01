/*******************************************************************************
*
* File su3_utils.c
*
* Functions to project to SU(3)
*
*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "utils.h"
#include "suN.h"
#include "representation.h"

static void normalize(suNg_vector *v)
{
   float fact=_vector_prod_re_g(*v,*v);
   fact=1.0f/(float)sqrt((double)(fact));
   _vector_mul_g(*v, fact, *v);
}


static void normalize_dble(suNg_vector_dble *v)
{
   double fact=_vector_prod_re_g(*v,*v);
   fact=1.0/sqrt(fact);
   _vector_mul_g(*v, fact, *v);
}

/*
#define _vector_cross_prod_g(v,w,z) \
   (v).c1.re= (w).c2.re*(z).c3.re-(w).c2.im*(z).c3.im  \
             -(w).c3.re*(z).c2.re+(w).c3.im*(z).c2.im; \
   (v).c1.im= (w).c3.re*(z).c2.im+(w).c3.im*(z).c2.re  \
             -(w).c2.re*(z).c3.im-(w).c2.im*(z).c3.re; \
   (v).c2.re= (w).c3.re*(z).c1.re-(w).c3.im*(z).c1.im  \
             -(w).c1.re*(z).c3.re+(w).c1.im*(z).c3.im; \
   (v).c2.im= (w).c1.re*(z).c3.im+(w).c1.im*(z).c3.re  \
             -(w).c3.re*(z).c1.im-(w).c3.im*(z).c1.re; \
   (v).c3.re= (w).c1.re*(z).c2.re-(w).c1.im*(z).c2.im  \
             -(w).c2.re*(z).c1.re+(w).c2.im*(z).c1.im; \
   (v).c3.im= (w).c2.re*(z).c1.im+(w).c2.im*(z).c1.re  \
             -(w).c1.re*(z).c2.im-(w).c1.im*(z).c2.re

void cross_prod(suNg_vector *v1,suNg_vector *v2,suNg_vector *v3)
{
   _vector_cross_prod_g(*v3,*v1,*v2);
}


void cross_prod_dble(suNg_vector_dble *v1,suNg_vector_dble *v2,suNg_vector_dble *v3)
{
   _vector_cross_prod_g(*v3,*v1,*v2);
}


void project_to_suNg(suNg *u)
{
   suNg_vector *v1,*v2,*v3;
   
   v1=(suNg_vector*)(u);
   v2=v1+1;
   v3=v1+2;
   
   normalize(v1);
   _vector_cross_prod_g(*v3,*v1,*v2);
   normalize(v3);
   _vector_cross_prod_g(*v2,*v3,*v1);   
}

void project_to_suNg_dble(suNg_dble *u)
{
   suNg_vector_dble *v1,*v2,*v3;
   
   v1=(suNg_vector_dble*)(u);
   v2=v1+1;
   v3=v1+2;
   
   normalize_dble(v1);
   _vector_cross_prod_g(*v3,*v1,*v2);
   normalize_dble(v3);
   _vector_cross_prod_g(*v2,*v3,*v1);
}

#undef _vector_cross_prod

*/



void project_to_suNg(suNg *u)
{
  int i,j;
  suNg_vector *v1,*v2;
  complex z;

  v1=(suNg_vector*)(u);
  v2=v1+1;
   
  normalize(v1);
  for (i=1; i<NG; ++i ) {
    for (j=i; j>0; --j) {
      z.re = _vector_prod_re_g(*v1, *v2);
      z.im = _vector_prod_im_g(*v1, *v2);
      _vector_project_g(*v2, z, *v1);
      ++v1;
    }
    normalize(v2);
    ++v2;
    v1=(suNg_vector*)(u);
  }

}

void project_to_suNg_dble(suNg_dble *u)
{
  int i,j;
  suNg_vector_dble *v1,*v2;
  complex_dble z;

  v1=(suNg_vector_dble*)(u);
  v2=v1+1;
   
  normalize_dble(v1);
  for (i=1; i<NG; ++i ) {
    for (j=i; j>0; --j) {
      z.re = _vector_prod_re_g(*v1, *v2);
      z.im = _vector_prod_im_g(*v1, *v2);
      _vector_project_g(*v2, z, *v1);
      ++v1;
    }
    normalize_dble(v2);
    ++v2;
    v1=(suNg_vector_dble*)(u);
  }

}

