/***************************************************************************\
* Copyright (c) 2008, Claudio Pica                                          *   
* All rights reserved.                                                      * 
\***************************************************************************/

/*******************************************************************************
*
* File Dphi_flt_gpu.c
*
* Action of the Wilson-Dirac operator D and hermitian g5D on a given 
* single-precision spinor field
*
*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "suN.h"
#include "global.h"
#include "error.h"
#include "dirac.h"
#include "linear_algebra.h"
#include "spinor_field.h"
#include "geometry.h"
#include "communications.h"
#include "memory.h"
#include "gpu.h"

#ifdef WITH_GPU

#ifdef ROTATED_SF
#include "update.h"
extern rhmc_par _update_par; /* Update/update_rhmc.c */
#endif /* ROTATED_SF */

/*
 * the following variable is used to keep trace of
 * matrix-vector multiplication.
 * we count how many time the function Dphi_ is called
 */
static unsigned long int MVMcounter=0;

unsigned long int getMVM_flt() {
	unsigned long int res=MVMcounter>>1; /* divide by two */
	MVMcounter=0; /* reset counter */

	return res;
}

/*
 * This function defines the massless Dirac operator
 * It can act on spinors defined on the whole lattice 
 * or on spinors with definite parity
 */

typedef struct _suNf_hspinor_flt
{
  suNf_vector_flt c[2];
} suNf_hspinor_flt;
  
/* v = suNf_vector ; in = input ; iy = site
   x = 0..3 spinor component 
 */
#define _suNf_read_spinor_flt_gpu_old(stride,v,in,iy,x)\
	(v).c[0]=((complex_flt*)(in))[(iy)+((x)*3)*(stride)]; \
  (v).c[1]=((complex_flt*)(in))[(iy)+((x)*3+1)*(stride)]; \
  (v).c[2]=((complex_flt*)(in))[(iy)+((x)*3+2)*(stride)]

#define _suNf_read_spinor_flt_gpu(stride,v,in,iy,x)\
iz=(iy)+((x)*3)*(stride);\
(v).c[0]=((complex_flt*)(in))[iz]; iz+=(stride); \
(v).c[1]=((complex_flt*)(in))[iz]; iz+=(stride);\
(v).c[2]=((complex_flt*)(in))[iz]


/* v = suNf_vector ; out = output ; iy = site
 x = 0..3 spinor component 
 */
#define _suNf_write_spinor_flt_gpu_old(stride,v,out,iy,x)\
((complex_flt*)(out))[(iy)+((x)*3)*(stride)]=(v).c[0]; \
((complex_flt*)(out))[(iy)+((x)*3+1)*(stride)]=(v).c[1]; \
((complex_flt*)(out))[(iy)+((x)*3+2)*(stride)]=(v).c[2]

#define _suNf_write_spinor_flt_gpu(stride,v,out,iy,x)\
iz=(iy)+((x)*3)*(stride);\
((complex_flt*)(out))[iz]=(v).c[0]; iz+=(stride); \
((complex_flt*)(out))[iz]=(v).c[1]; iz+=(stride);\
((complex_flt*)(out))[iz]=(v).c[2]


#define _suNf_flt_read_gpu_old(stride,v,in,iy,x)\
(v).c[0]=((float*)(in))[(iy)+((x)*4)*(stride)]; \
(v).c[1]=((float*)(in))[(iy)+((x)*4+1)*(stride)];\
(v).c[2]=((float*)(in))[(iy)+((x)*4+2)*(stride)];\
(v).c[3]=((float*)(in))[(iy)+((x)*4+3)*(stride)]

#define _suNf_flt_read_gpu(stride,v,in,iy,x)\
iz=(iy)+((x)*4)*(stride);\
(v).c[0]=((float*)(in))[iz]; iz+=(stride); \
(v).c[1]=((float*)(in))[iz]; iz+=(stride);\
(v).c[2]=((float*)(in))[iz]; iz+=(stride);\
(v).c[3]=((float*)(in))[iz]


/* Takes an even input spinor and returns an odd spinor */
__global__ void Dphi_flt_gpu_oe(suNf_spinor_flt* out, suNf_spinor_flt* in, 
                             const suNf_flt* gauge, const int *iup, const int *idn, 
                             const int vol4h, const int stride)
{
  
  suNf_spinor_flt r;
  suNf_hspinor_flt sn;
  suNf_flt u;				
  
  int iy, iz;
  int ix = blockIdx.x*BLOCK_SIZE + threadIdx.x;
  ix = min(ix,vol4h-1);
  
#ifdef UPDATE_EO
   
#else 
  
#endif //UPDATE_EO                   

  
  /******************************* direction +0 *********************************/
  iy=iup(ix+vol4h,0);
	
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_add_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge+4*vol4h,ix,0);
  _suNf_multiply(r.c[0],u,sn.c[0]);
  
  r.c[2]=r.c[0];
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_add_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_multiply(r.c[1],u,sn.c[0]);
  
  r.c[3]=r.c[1];
  
  /******************************* direction -0 *********************************/
  iy=idn(ix+vol4h,0);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_sub_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge,iy,0);
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_sub_assign_f(r.c[2],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_sub_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_sub_assign_f(r.c[3],sn.c[1]);       
  
  /******************************* direction +1 *********************************/
  iy=iup(ix+vol4h,1);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_i_add_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge+4*vol4h,ix,1);
  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_i_sub_assign_f(r.c[3],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_i_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_i_sub_assign_f(r.c[2],sn.c[1]);
  
  /******************************* direction -1 *********************************/
  iy=idn(ix+vol4h,1);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_i_sub_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge,iy,1);
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_i_add_assign_f(r.c[3],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
	_vector_i_sub_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_i_add_assign_f(r.c[2],sn.c[1]);
  
  /******************************* direction +2 *********************************/
  iy= iup(ix+vol4h,2);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_flt_read_gpu(stride,u,gauge+4*vol4h,ix,2);
  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_add_assign_f(r.c[3],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_sub_assign_f(sn.c[0],sn.c[1]);

  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_sub_assign_f(r.c[2],sn.c[1]);
  
  /******************************* direction -2 *********************************/
  iy = idn(ix+vol4h,2);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_sub_assign_f(sn.c[0],sn.c[1]);

  _suNf_flt_read_gpu(stride,u,gauge,iy,2);
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_sub_assign_f(r.c[3],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_add_assign_f(r.c[2],sn.c[1]);
  
  /******************************* direction +3 *********************************/
  iy = iup(ix+vol4h,3);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_i_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_flt_read_gpu(stride,u,gauge+4*vol4h,ix,3);
  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_i_sub_assign_f(r.c[2],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_i_sub_assign_f(sn.c[0],sn.c[1]);

  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_i_add_assign_f(r.c[3],sn.c[1]);
  
  /******************************* direction -3 *********************************/
  iy = idn(ix+vol4h,3);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_i_sub_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge,iy,3);
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_i_add_assign_f(r.c[2],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_i_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_i_sub_assign_f(r.c[3],sn.c[1]);
  
  /******************************** end of directions *********************************/ 
  //out[ix]=r;
  _spinor_mul_f(r,-0.5f,r);
  
  _suNf_write_spinor_flt_gpu(stride,r.c[0],out,ix,0);
  _suNf_write_spinor_flt_gpu(stride,r.c[1],out,ix,1);
  _suNf_write_spinor_flt_gpu(stride,r.c[2],out,ix,2);
  _suNf_write_spinor_flt_gpu(stride,r.c[3],out,ix,3);
  
}

/* Takes an odd input spinor and returns an even spinor */
__global__ void Dphi_flt_gpu_eo(suNf_spinor_flt* out, suNf_spinor_flt* in, 
                                const suNf_flt* gauge, const int *iup, const int *idn, 
                                const int vol4h, const int stride)
{
  
  suNf_spinor_flt r;
  suNf_hspinor_flt sn;
  suNf_flt u;				
  
  int iy, iz;
  int ix = blockIdx.x*BLOCK_SIZE + threadIdx.x;
  ix = min(ix,vol4h-1);
  
#ifdef UPDATE_EO
   
#else 

#endif //UPDATE_EO                   
  
  
  /******************************* direction +0 *********************************/
  iy=iup(ix,0)-vol4h;
	
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_add_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge,ix,0);
  _suNf_multiply(r.c[0],u,sn.c[0]);
  
  r.c[2]=r.c[0];
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_multiply(r.c[1],u,sn.c[0]);
  
  r.c[3]=r.c[1];
  
  /******************************* direction -0 *********************************/
  iy=idn(ix,0)-vol4h;
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_sub_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge+4*vol4h,iy,0);
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_sub_assign_f(r.c[2],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_sub_assign_f(sn.c[0],sn.c[1]);

  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_sub_assign_f(r.c[3],sn.c[1]);       
  
  /******************************* direction +1 *********************************/
  iy=iup(ix,1)-vol4h;
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_i_add_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge,ix,1);
  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_i_sub_assign_f(r.c[3],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_i_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_i_sub_assign_f(r.c[2],sn.c[1]);
  
  /******************************* direction -1 *********************************/
  iy=idn(ix,1)-vol4h;
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_i_sub_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge+4*vol4h,iy,1);
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_i_add_assign_f(r.c[3],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_i_sub_assign_f(sn.c[0],sn.c[1]);

  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_i_add_assign_f(r.c[2],sn.c[1]);
  
  /******************************* direction +2 *********************************/
  iy= iup(ix,2)-vol4h;
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_flt_read_gpu(stride,u,gauge,ix,2);
  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_add_assign_f(r.c[3],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_sub_assign_f(sn.c[0],sn.c[1]);

  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_sub_assign_f(r.c[2],sn.c[1]);
  
  /******************************* direction -2 *********************************/
  iy = idn(ix,2)-vol4h;
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_sub_assign_f(sn.c[0],sn.c[1]);

  _suNf_flt_read_gpu(stride,u,gauge+4*vol4h,iy,2);
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_sub_assign_f(r.c[3],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_add_assign_f(r.c[2],sn.c[1]);
  
  /******************************* direction +3 *********************************/
  iy = iup(ix,3)-vol4h;
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_i_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_flt_read_gpu(stride,u,gauge,ix,3);
  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_i_sub_assign_f(r.c[2],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_i_sub_assign_f(sn.c[0],sn.c[1]);

  _suNf_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_i_add_assign_f(r.c[3],sn.c[1]);
  
  /******************************* direction -3 *********************************/
  iy = idn(ix,3)-vol4h;
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,0);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,2);
  _vector_i_sub_assign_f(sn.c[0],sn.c[1]);
  
  _suNf_flt_read_gpu(stride,u,gauge+4*vol4h,iy,3);
  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[0],sn.c[1]);
  _vector_i_add_assign_f(r.c[2],sn.c[1]);
  
  _suNf_read_spinor_flt_gpu(stride,sn.c[0],in,iy,1);
  _suNf_read_spinor_flt_gpu(stride,sn.c[1],in,iy,3);
  _vector_i_add_assign_f(sn.c[0],sn.c[1]);

  _suNf_inverse_multiply(sn.c[1],u,sn.c[0]);
  
  _vector_add_assign_f(r.c[1],sn.c[1]);
  _vector_i_sub_assign_f(r.c[3],sn.c[1]);
  
  /******************************** end of directions *********************************/ 
  //out[ix]=r;
  _spinor_mul_f(r,-0.5f,r);
  
  _suNf_write_spinor_flt_gpu(stride,r.c[0],out,ix,0);
  _suNf_write_spinor_flt_gpu(stride,r.c[1],out,ix,1);
  _suNf_write_spinor_flt_gpu(stride,r.c[2],out,ix,2);
  _suNf_write_spinor_flt_gpu(stride,r.c[3],out,ix,3);
  
}



void Dphi_flt_(spinor_field_flt *out, spinor_field_flt *in)
{
   int N,grid;
  const int vol4h=T*X*Y*Z/2;
#ifdef UPDATE_EO
#else
#endif
  
   error((in==NULL)||(out==NULL),1,"Dphi_flt [Dphi_flt_gpu.c]",
         "Attempt to access unallocated memory space");
   
   error(in==out,1,"Dphi_flt [Dphi_flt_gpu.c]",
         "Input and output fields must be different");

#ifndef CHECK_SPINOR_MATCHING
   error(out->type==&glat_even && in->type!=&glat_odd,1,"Dphi_flt [Dphi_flt_gpu.c]", "Spinors don't match! (1)");
   error(out->type==&glat_odd && in->type!=&glat_even,1,"Dphi_flt [Dphi_flt_gpu.c]", "Spinors don't match! (2)");
   error(out->type==&glattice && in->type!=&glattice,1,"Dphi_flt [Dphi_flt_gpu.c]", "Spinors don't match! (3)");
#endif
   
   //N = out->type->master_end[0] - out->type->master_start[0] + 1 ;
  N=vol4h; 
  grid = N/BLOCK_SIZE + ((N % BLOCK_SIZE == 0) ? 0 : 1);
  
  if(in->type==&glat_odd) {
    getMVM_flt();
    Dphi_flt_gpu_eo<<<grid,BLOCK_SIZE>>>(START_SP_ADDRESS_GPU(out),START_SP_ADDRESS_GPU(in), u_gauge_f_flt->gpu_ptr,iup_gpu,idn_gpu,vol4h, vol4h);
      CudaCheckError();
  } else if (in->type==&glat_even) {
    getMVM_flt();
    Dphi_flt_gpu_oe<<<grid,BLOCK_SIZE>>>(START_SP_ADDRESS_GPU(out),START_SP_ADDRESS_GPU(in), u_gauge_f_flt->gpu_ptr,iup_gpu,idn_gpu,vol4h, vol4h);
      CudaCheckError();
  } else if (in->type==&glattice) {
      in->type=&glat_even;
      out->type=&glat_odd;
    getMVM_flt();
    Dphi_flt_gpu_oe<<<grid,BLOCK_SIZE>>>(START_SP_ADDRESS_GPU(out),START_SP_ADDRESS_GPU(in), u_gauge_f_flt->gpu_ptr,iup_gpu,idn_gpu,vol4h, vol4h);
      CudaCheckError();
      in->type=&glat_odd;
      out->type=&glat_even;
    getMVM_flt();
    Dphi_flt_gpu_eo<<<grid,BLOCK_SIZE>>>(START_SP_ADDRESS_GPU(out),START_SP_ADDRESS_GPU(in), u_gauge_f_flt->gpu_ptr,iup_gpu,idn_gpu,vol4h, vol4h);
      CudaCheckError();
      
      in->type=&glattice;
      out->type=&glattice;      
  } else {
      error(1,1,"Dphi_flt_ [Dphi_flt_gpu.c]", "Wrong input spinor geometry!");
  }
  
}

/*
 * this function takes 2 spinors defined on the whole lattice
 */
 void Dphi_flt(double m0, spinor_field_flt *out, spinor_field_flt *in)				///		I have left m0 in double precision
{
   double rho;

   error((in==NULL)||(out==NULL),1,"Dphi_flt [Dphi_flt_gpu.c]",
         "Attempt to access unallocated memory space");
   
   error(in==out,1,"Dphi_flt [Dphi_flt_gpu.c]",
         "Input and output fields must be different");


#ifdef CHECK_SPINOR_MATCHING
   error(out->type!=&glattice || in->type!=&glattice,1,"Dphi_flt [Dphi_flt_gpu.c]", "Spinors are not defined on all the lattice!");
#endif /* CHECK_SPINOR_MATCHING */
   
  Dphi_flt_(out,in); 
  
   rho=4.+m0;
   spinor_field_mul_add_assign_f_flt(out,rho,in);
   
}

void g5Dphi_flt(double m0, spinor_field_flt *out, spinor_field_flt *in)
{
  double rho;

  error((in==NULL)||(out==NULL),1,"g5Dphi_flt [Dphi_flt_gpu.c]",
	"Attempt to access unallocated memory space");

  error(in==out,1,"g5Dphi_flt [Dphi_flt_gpu.c]",
	"Input and output fields must be different");

#ifdef CHECK_SPINOR_MATCHING
   error(out->type!=&glattice || in->type!=&glattice,1,"g5Dphi_flt [Dphi_flt_gpu.c]", "Spinors are not defined on all the lattice!");
#endif /* CHECK_SPINOR_MATCHING */

  Dphi_flt_(out,in); 

   rho=4.+m0;
   spinor_field_mul_add_assign_f_flt(out,rho,in);
   spinor_field_g5_assign_f_flt(out);
}

static int init=1;
static spinor_field_flt *gtmp=NULL;
static spinor_field_flt *etmp=NULL;
static spinor_field_flt *otmp=NULL;

static void free_mem() {
  if (gtmp!=NULL) { 
    free_spinor_field_f_flt(gtmp);					// This does
    etmp=NULL; 
  }
  if (etmp!=NULL) { 
    free_spinor_field_f_flt(etmp); 
    etmp=NULL; 
  }
  if (otmp!=NULL) { 
    free_spinor_field_f_flt(otmp); 
    otmp=NULL; 
  }
  init=1;
}

static void init_Dirac_flt() {
  if (init) {
    alloc_mem_t=GPU_MEM;
    
    gtmp=alloc_spinor_field_f_flt(1,&glattice);
    etmp=alloc_spinor_field_f_flt(1,&glat_even);
    otmp=alloc_spinor_field_f_flt(1,&glat_odd);

    alloc_mem_t=std_mem_t;
    
    atexit(&free_mem);
    init=0;
  }
}

/* Even/Odd preconditioned dirac operator
 * this function takes 2 spinors defined on the even lattice
 * Dphi in = (4+m0)^2*in - D_EO D_OE in
 *
 */
void Dphi_eopre_flt(double m0, spinor_field_flt *out, spinor_field_flt *in)
{
  double rho;
  
  error((in==NULL)||(out==NULL),1,"Dphi_eopre_flt [Dphi_flt_gpu.c]",
	"Attempt to access unallocated memory space");
  
  error(in==out,1,"Dphi_eopre_flt [Dphi_flt_gpu.c]",
	"Input and output fields must be different");
  
#ifdef CHECK_SPINOR_MATCHING
  error(out->type!=&glat_even || in->type!=&glat_even,1,"Dphi_eopre__flt " __FILE__, "Spinors are not defined on even lattice!");
#endif /* CHECK_SPINOR_MATCHING */

  /* alloc memory for temporary spinor field */
  if (init) { init_Dirac_flt(); }
  
  Dphi_flt_(otmp, in);
  Dphi_flt_(out, otmp);
  
  rho=4.0+m0;
  rho*=-rho; /* this minus sign is taken into account below */
  
  spinor_field_mul_add_assign_f_flt(out,rho,in);
  spinor_field_minus_f_flt(out,out);
}


/* Even/Odd preconditioned dirac operator
 * this function takes 2 spinors defined on the odd lattice
 * Dphi in = (4+m0)^2*in - D_OE D_EO in
 *
 */
void Dphi_oepre_flt(double m0, spinor_field_flt *out, spinor_field_flt *in)
{
  double rho;
  
  error((in==NULL)||(out==NULL),1,"Dphi_oepre_flt [Dphi_flt_gpu.c]",
	"Attempt to access unallocated memory space");
  
  error(in==out,1,"Dphi_oepre_flt [Dphi_flt_gpu.c]",
	"Input and output fields must be different");
  
#ifdef CHECK_SPINOR_MATCHING
  error(out->type!=&glat_odd || in->type!=&glat_odd,1,"Dphi_oepre_flt " __FILE__, "Spinors are not defined on odd lattice!");
#endif /* CHECK_SPINOR_MATCHING */


  /* alloc memory for temporary spinor field */
  if (init) { init_Dirac_flt();}
  
  Dphi_flt_(etmp, in);
  Dphi_flt_(out, etmp);

  rho=4.0+m0;
  rho*=-rho; /* this minus sign is taken into account below */
  
  spinor_field_mul_add_assign_f_flt(out,rho,in);
  spinor_field_minus_f_flt(out,out);


}



void g5Dphi_eopre_flt(double m0, spinor_field_flt *out, spinor_field_flt *in)
{
  double rho;

  error((in==NULL)||(out==NULL),1,"g5Dphi_eopre_flt [Dphi_flt_gpu.c]",
	"Attempt to access unallocated memory space");
  
  error(in==out,1,"g5Dphi_eopre_flt [Dphi_flt_gpu.c]",
	"Input and output fields must be different");
  
#ifdef CHECK_SPINOR_MATCHING
  error(out->type!=&glat_even || in->type!=&glat_even,1,"g5Dphi_eopre_flt " __FILE__, "Spinors are not defined on even lattice!");
#endif /* CHECK_SPINOR_MATCHING */

#if defined(BASIC_SF) || defined(ROTATED_SF)
  SF_spinor_bcs_flt(in);
#endif /* defined(BASIC_SF) || defined(ROTATED_SF) */

  /* alloc memory for temporary spinor field */
  if (init) { init_Dirac_flt();}
  
  Dphi_flt_(otmp, in);
  Dphi_flt_(out, otmp);

  rho=4.0+m0;
  rho*=-rho; /* this minus sign is taken into account below */
  
  spinor_field_mul_add_assign_f_flt(out,rho,in);
  spinor_field_minus_f_flt(out,out);
  spinor_field_g5_assign_f_flt(out);

#if defined(BASIC_SF) || defined(ROTATED_SF)
  SF_spinor_bcs_flt(out);
#endif /* defined(BASIC_SF) || defined(ROTATED_SF) */
  
}

/* g5Dphi_eopre ^2 */
void g5Dphi_eopre_sq_flt(double m0, spinor_field_flt *out, spinor_field_flt *in) {
  /* alloc memory for temporary spinor field */
  if (init) { init_Dirac_flt(); }

  g5Dphi_eopre_flt(m0, etmp, in);
  g5Dphi_eopre_flt(m0, out, etmp);
  
}

/* g5Dhi ^2 */
void g5Dphi_sq_flt(double m0, spinor_field_flt *out, spinor_field_flt *in) {
  /* alloc memory for temporary spinor field */
  if (init) { init_Dirac_flt();  }
  

  g5Dphi_flt(m0, gtmp, in);
  g5Dphi_flt(m0, out, gtmp);

}

#endif