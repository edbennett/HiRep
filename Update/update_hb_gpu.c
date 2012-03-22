/***************************************************************************\
* Copyright (c) 2008, Claudio Pica                                          *   
\***************************************************************************/

/*******************************************************************************
*
* File update.c
*
* Update programs
*
*******************************************************************************/
#ifdef WITH_GPU
#define PROJECT_INTERVAL 10

#include "suN.h"
#include "utils.h"
#include "global.h"
#include "update.h"
#include "communications.h"
#include "gpu.h"


#define _suNg_read_gpu(stride,v,in,iy,x)\
iw=(iy)+((x)*4)*(stride);\
(v).c[0]=((double*)(in))[iw]; iw+=(stride); \
(v).c[1]=((double*)(in))[iw]; iw+=(stride);\
(v).c[2]=((double*)(in))[iw]; iw+=(stride);\
(v).c[3]=((double*)(in))[iw]

#define _suNg_write_gpu(stride,v,in,iy,x)\
iw=(iy)+((x)*4)*(stride);\
((double*)(in))[iw]=(v).c[0]; iw+=(stride);\
((double*)(in))[iw]=(v).c[1]; iw+=(stride);\
((double*)(in))[iw]=(v).c[2]; iw+=(stride);\
((double*)(in))[iw]=(v).c[3]

__global__ void project_gauge_field_gpu(suNg* gauge, int N){ //Only for quaternions
  int ix = blockIdx.x*BLOCK_SIZE+ threadIdx.x;
  int iw,i;
  double norm;
  suNg u;
  ix = min(ix,N);
  if (ix>=N/2) {
    gauge+=2*N;
    ix -= N/2;
  }
  for (i=0;i<4;++i){
    _suNg_read_gpu(N/2,u,gauge,ix,i);
    _suNg_sqnorm(norm,u);
    norm = 1./sqrt(0.5*norm);
    _suNg_mul(u,norm,u);
    _suNg_write_gpu(N/2,u,gauge,ix,i);
  }

}

void project_gauge_field(void){
  int N = u_gauge->type->master_end[0] -  u_gauge->type->master_start[0] + 1;
  int grid = N/BLOCK_SIZE + ((N % BLOCK_SIZE == 0) ? 0 : 1);
  project_gauge_field_gpu<<<grid,BLOCK_SIZE>>>(u_gauge->gpu_ptr,N);
  //  start_gf_sendrecv(u_gauge);
}


#undef _suNg_read_gpu
#undef _suNg_write_gpu
#endif
