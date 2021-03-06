/******************************************************************************
*
* Test of modules
*
******************************************************************************/

#define MAIN_PROGRAM

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "io.h"
#include "random.h"
#include "error.h"
#include "geometry.h"
#include "memory.h"
#include "statistics.h"
#include "update.h"
#include "global.h"
#include "observables.h"
#include "suN.h"
#include "suN_types.h"
#include "dirac.h"
#include "linear_algebra.h"
#include "inverters.h"
#include "representation.h"
#include "utils.h"
#include "logger.h"
#include "communications.h"

int nhb,nor,nit,nth,nms,level,seed;
double beta;

static double hmass=0.1;


void D(spinor_field *out, spinor_field *in){
  Dphi(hmass,out,in);
}

void H(spinor_field *out, spinor_field *in){
  g5Dphi(hmass,out,in);
}

static spinor_field *tmp;
void M(spinor_field *out, spinor_field *in){
  g5Dphi(-hmass,tmp,in); 
  g5Dphi(-hmass,out,tmp);
}


int main(int argc,char *argv[])
{
  char pame[256];
  int i;
  double tau1, tau2;
  spinor_field *s1, *s2, *s3;
  spinor_field *res, *res_trunc;

  mshift_par par;

  int cgiters;
  /* setup process id and communications */
  setup_process(&argc,&argv);
  
  /* logger setup */

  logger_setlevel(0,100); /* log all */
  if (PID!=0) { 
    logger_disable();}
  else{
    sprintf(pame,">out_%d",PID); logger_stdout(pame);
    sprintf(pame,"err_%d",PID); freopen(pame,"w",stderr);
  }
     
  lprintf("MAIN",0,"PId =  %d [world_size: %d]\n\n",PID,WORLD_SIZE); 
  
  /* read input file */
  read_input(glb_var.read,"test_input");

  /* setup communication geometry */
  if (geometry_init() == 1) {
    finalize_process();
    return 0;
  }
  
   
   lprintf("MAIN",0,"Gauge group: SU(%d)\n",NG);
   lprintf("MAIN",0,"Fermion representation: " REPR_NAME " [dim=%d]\n",NF);
   lprintf("MAIN",0,"global size is %dx%dx%dx%d\n",GLB_T,GLB_X,GLB_Y,GLB_Z);
   lprintf("MAIN",0,"proc grid is %dx%dx%dx%d\n",NP_T,NP_X,NP_Y,NP_Z);
   
   /* setup lattice geometry */
   geometry_mpi_eo();
   /* test_geometry_mpi_eo(); */
    
    
    /* setup random numbers */
    read_input(rlx_var.read,"test_input");
    lprintf("MAIN",0,"RLXD [%d,%d]\n",rlx_var.rlxd_level,rlx_var.rlxd_seed+MPI_PID);
    rlxd_init(rlx_var.rlxd_level,rlx_var.rlxd_seed+MPI_PID); /* use unique MPI_PID to shift seeds */

   
   u_gauge=alloc_gfield(&glattice);
#ifndef REPR_FUNDAMENTAL
   u_gauge_f=alloc_gfield_f(&glattice);
#endif
  represent_gauge_field();

  lprintf("MAIN",0,"Generating a random gauge field... ");
  lprintf("MAIN",0,"done.\n");

  random_u(u_gauge);

  start_gf_sendrecv(u_gauge);
  complete_gf_sendrecv(u_gauge);

  represent_gauge_field();
   
  par.n = 6;
  par.shift=(double*)malloc(sizeof(double)*(par.n));
  par.err2=1.e-28;
  par.max_iter=0;
  res=alloc_spinor_field_f(par.n*2+3,&glattice);
  res_trunc=res+par.n;
  s1=res_trunc+par.n;
  s2=s1+1;
  s3=s2+1;
  tmp=s3+1;


  par.shift[0]=+0.1;
  par.shift[1]=-0.21;
  par.shift[2]=+0.05;
  par.shift[3]=-0.01;
  par.shift[4]=-0.15;
  par.shift[5]=-0.05;
   

  gaussian_spinor_field(s1); 
  tau1=spinor_field_sqnorm_f(s1);
  lprintf("QMR TEST",0,"Norma iniziale: %e\n",tau1);

  /* TEST g5QMR_M */

  lprintf("QMR TEST",0,"\n");
  lprintf("QMR TEST",0,"Testing g5QMR multishift\n");
  lprintf("QMR TEST",0,"------------------------\n");

  cgiters=g5QMR_mshift_trunc(&par, 20, &D, s1, res_trunc, res);
  lprintf("QMR TEST",0,"Converged in %d iterations\n",cgiters);

  for(i=0;i<par.n;++i){
    D(s2,&res[i]);
    D(s3,&res_trunc[i]);
    spinor_field_mul_add_assign_f(s2,-par.shift[i],&res[i]);
    spinor_field_mul_add_assign_f(s3,-par.shift[i],&res_trunc[i]);
    spinor_field_sub_assign_f(s2,s1);
    spinor_field_sub_assign_f(s3,s1);
    tau1=spinor_field_sqnorm_f(s2)/spinor_field_sqnorm_f(s1);
    tau2=spinor_field_sqnorm_f(s3)/spinor_field_sqnorm_f(s1);
    lprintf("QMR TEST",0," g5QMR[%d] = %e, trunc = %e (req. %e)\n",i,tau1,tau2,par.err2);
  }
	 
  free_spinor_field_f(res);
  free(par.shift);
  finalize_process();

  return 0;
}
