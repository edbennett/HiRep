#include "global.h"
#include "linear_algebra.h"
#include "inverters.h"
#include "suN.h"
#include "observables.h"
#include "dirac.h"
#include "utils.h"
#include "update.h"
#include "error.h"
#include "logger.h"
#include "memory.h"
#include <malloc.h>
#include <math.h>
#include <assert.h>
#include <string.h>


#define SOUCE_SINK_INDEX(i,j) ( (i)*n_sources + (j) )
#define SPIN_2D_INDEX(i,j) ( (i)*4 + (j) )
#define NOISY_INDEX(r,d) ( (r)*n_dilution_slices + (d) )


static double hmass;

enum{ n_eigenvalues = 100 }; /* N_{ev} */
enum{ n_hp_eigenvalues = 50 };

enum{ n_global_noisy_sources_per_point = 1 }; /* N_r */
enum{ n_points = 2 };
enum{ n_global_noisy_sources = n_global_noisy_sources_per_point * n_points }; /* N_{gns} */
enum{ n_dilution_slices = T }; /* N_d */
enum{ n_diluted_noisy_sources = n_global_noisy_sources*n_dilution_slices };

enum{ n_sources = n_eigenvalues + n_diluted_noisy_sources };

enum{ n_gamma_matrices = 9 };
enum{  n_correlators = 9 };

static double d[n_eigenvalues];
static suNf_spinor *ev[n_eigenvalues];
static suNf_spinor *ws[2];

static suNf_spinor *noisy_sources[n_diluted_noisy_sources];
static suNf_spinor **noisy_sinks[n_diluted_noisy_sources];

static complex meson[n_gamma_matrices][n_sources*n_sources][T];



static void H(suNf_spinor *out, suNf_spinor *in);
static void D(suNf_spinor *out, suNf_spinor *in);
static void all_to_all_quark_propagator_init(int n_masses);
static void z2_spinor(suNf_spinor *source);
static void get_time_diluted_sources(suNf_spinor **source);
static void get_sinks(suNf_spinor *source, suNf_spinor **sink, int n_masses, double *mass, double acc);
static void source_sink_contraction(complex out[][16], suNf_spinor *source, suNf_spinor *sink, double z);
static void triplet_correlator_re(double* out, complex A[][T], complex B[][T], double x);
static void triplet_correlator_im(double* out, complex A[][T], complex B[][T], double x);
static void hairpin_re(double* out, complex A[][T], complex B[][T], double x);
static void hairpin_im(double* out, complex A[][T], complex B[][T], double x);


static void id_trace_H(complex* out, complex* smat); static int ID = -1;
static void g0_trace_H(complex* out, complex* smat); static int G0 = -1;
static void g5_trace_H(complex* out, complex* smat); static int G5 = -1;
static void g0g5_trace_H(complex* out, complex* smat); static int G0G5 = -1;
static void g1_trace_H(complex* out, complex* smat); static int G1 = -1;
static void g2_trace_H(complex* out, complex* smat); static int G2 = -1;
static void g3_trace_H(complex* out, complex* smat); static int G3 = -1;
static void g0g1_trace_H(complex* out, complex* smat); static int G0G1 = -1;
static void g0g2_trace_H(complex* out, complex* smat); static int G0G2 = -1;
static void g0g3_trace_H(complex* out, complex* smat); static int G0G3 = -1;
static void g5g1_trace_H(complex* out, complex* smat); static int G5G1 = -1;
static void g5g2_trace_H(complex* out, complex* smat); static int G5G2 = -1;
static void g5g3_trace_H(complex* out, complex* smat); static int G5G3 = -1;
static void g0g5g1_trace_H(complex* out, complex* smat); static int G0G5G1 = -1;
static void g0g5g2_trace_H(complex* out, complex* smat); static int G0G5G2 = -1;
static void g0g5g3_trace_H(complex* out, complex* smat); static int G0G5G3 = -1;


void dublin_meson_correlators(double** correlator[], char corr_name[][256], int n_corr, int n_masses, double *mass, double acc) {
	int i, j, k, t, counter, maxh2iter;

	int ie, status;
	float omega1, omega2;
	double ubnd;
	
	suNf_spinor *source;
	suNf_spinor *sink;
	
	complex ss[T][16];
	double tmpcorr[T];

	maxh2iter = max_H2(&ubnd,mass[0]);
	ubnd = sqrt(ubnd);
	omega1 = 1.0e-6f;
	omega2 = 1.0e-2f;

	error(n_corr < n_correlators,1,"meson_correlators [all_to_all_quark_propagator.c]","Bad dimension for correlator[][]");


	/* allocate memory */

	set_spinor_len(VOLUME);
	all_to_all_quark_propagator_init(n_masses);


	/* compute the lowest n_eigenvalues eigenvalues/vectors */

	ie = eva(VOLUME,n_hp_eigenvalues,n_eigenvalues,0,100,20,ubnd,omega1,omega2,&H,ws,ev,d,&status);
	error(ie!=0,1,"all_to_all_quark_propagator","Failed to compute Dirac eigenvectors");


	/* generate random sources & sinks */

	for(i = 0; i < n_global_noisy_sources; i++) {
		get_time_diluted_sources(noisy_sources + NOISY_INDEX(i,0));
		for(j = 0; j < n_dilution_slices; j++)
			get_sinks(noisy_sources[NOISY_INDEX(i,j)], noisy_sinks[NOISY_INDEX(i,j)], n_masses, mass, acc);
	}


	/* two-point functions */

	for(i = 0; i < n_masses; i++) {
		
		for(j = 0; j < n_sources; j++) {
			
			if(j < n_eigenvalues) source = ev[j];
			else source = noisy_sources[j-n_eigenvalues];
			
			for(k = 0; k < n_sources; k++) {
				
				if(k < n_eigenvalues) {
					sink = ev[k];
					source_sink_contraction(ss, source, sink, 1.0f/d[k]);
				} else {
					sink = noisy_sinks[j-n_eigenvalues][i];
					source_sink_contraction(ss, source, sink, 2.0f/n_global_noisy_sources);
				}
				
				for(t = 0; t < T; t++) {
					counter = 0;
					
					id_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					ID = counter;
					counter++;
/*
					g0_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G0 = counter;
					counter++;
*/
					g5_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G5 = counter;
					counter++;

					g0g5_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G0G5 = counter;
					counter++;

					g1_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G1 = counter;
					counter++;

					g2_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G2 = counter;
					counter++;

					g3_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G3 = counter;
					counter++;
/*
					g0g1_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G0G1 = counter;
					counter++;

					g0g2_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G0G2 = counter;
					counter++;

					g0g3_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G0G3 = counter;
					counter++;

					g5g1_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G5G1 = counter;
					counter++;

					g5g2_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G5G2 = counter;
					counter++;

					g5g3_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G5G3 = counter;
					counter++;
*/
					g0g5g1_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G0G5G1 = counter;
					counter++;

					g0g5g2_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G0G5G2 = counter;
					counter++;

					g0g5g3_trace_H(meson[counter][SOUCE_SINK_INDEX(j,k)]+t, ss[t]);
					G0G5G3 = counter;
					counter++;

				}
				
			}
		}
		
		counter = 0;
		
		
		/* scalar C=+1; isotriplet=a isosinglet=f */
		
		if(i == 0) strcpy(corr_name[counter],"a");
		triplet_correlator_re(correlator[counter][i], meson[ID], meson[ID], 1.0);
		counter++;

		if(i == 0) strcpy(corr_name[counter],"f");
		hairpin_re(correlator[counter][i], meson[ID], meson[ID], 1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] = correlator[counter-1][i][t] - correlator[counter][i][t];
		counter++;

		
		/* pseudoscalar C=+1; isotriplet=pi isosinglet=eta */
		
		if(i == 0) strcpy(corr_name[counter],"pi");
		triplet_correlator_re(correlator[counter][i], meson[G5], meson[G5], 1.0);
		counter++;

		if(i == 0) strcpy(corr_name[counter],"eta");
		hairpin_re(correlator[counter][i], meson[G5], meson[G5], 1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] = correlator[counter-1][i][t] - correlator[counter][i][t];
		counter++;
		
		
		/* vector C=-1; isotriplet=rho isosinglet=phi */
		
		if(i == 0) strcpy(corr_name[counter],"rho");
		triplet_correlator_re(correlator[counter][i], meson[G1], meson[G1], 1.0);
		triplet_correlator_re(tmpcorr, meson[G2], meson[G2], 1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] += tmpcorr[t];
		triplet_correlator_re(tmpcorr, meson[G3], meson[G3], 1.0);
		for(t = 0; t < T; t++) {
			correlator[counter][i][t] += tmpcorr[t];
			correlator[counter][i][t] /= 3.0;
		}
		counter++;
		
		if(i == 0) strcpy(corr_name[counter],"phi");
		hairpin_re(correlator[counter][i], meson[G1], meson[G1], 1.0);
		hairpin_re(tmpcorr, meson[G2], meson[G2], 1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] += tmpcorr[t];
		hairpin_re(tmpcorr, meson[G3], meson[G3], 1.0);
		for(t = 0; t < T; t++) {
			correlator[counter][i][t] += tmpcorr[t];
			correlator[counter][i][t] /= 3.0;
			correlator[counter][i][t] = correlator[counter-1][i][t] - correlator[counter][i][t];
		}		
		counter++;
		
		
		/* pseudovector C=-1; isotriplet=b isosinglet=h */
		
		if(i == 0) strcpy(corr_name[counter],"b");
		triplet_correlator_re(correlator[counter][i], meson[G0G5G1], meson[G0G5G1], 1.0);
		triplet_correlator_re(tmpcorr, meson[G0G5G2], meson[G0G5G2], 1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] += tmpcorr[t];
		triplet_correlator_re(tmpcorr, meson[G0G5G3], meson[G0G5G3], 1.0);
		for(t = 0; t < T; t++) {
			correlator[counter][i][t] += tmpcorr[t];
			correlator[counter][i][t] /= 3.0;
		}
		counter++;
		
		if(i == 0) strcpy(corr_name[counter],"h");
		hairpin_re(correlator[counter][i], meson[G0G5G1], meson[G0G5G1], 1.0);
		hairpin_re(tmpcorr, meson[G0G5G2], meson[G0G5G2], 1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] += tmpcorr[t];
		hairpin_re(tmpcorr, meson[G0G5G3], meson[G0G5G3], 1.0);
		for(t = 0; t < T; t++) {
			correlator[counter][i][t] += tmpcorr[t];
			correlator[counter][i][t] /= 3.0;
			correlator[counter][i][t] = correlator[counter-1][i][t] - correlator[counter][i][t];
		}		
		counter++;

		
		/* another pseudoscalar C=+1; isotriplet=pi isosinglet=eta */
		/*
		if(i == 0) strcpy(corr_name[counter],"pi2");
		triplet_correlator_re(correlator[counter][i], meson[G0G5], meson[G0G5], -1.0);
		counter++;

		if(i == 0) strcpy(corr_name[counter],"eta2");
		hairpin_re(correlator[counter][i], meson[G0G5], meson[G0G5], -1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] = correlator[counter-1][i][t] - correlator[counter][i][t];
		counter++;
		*/
		
		/* another vector C=-1; isotriplet=rho isosinglet=phi */
		/*
		if(i == 0) strcpy(corr_name[counter],"rho2");
		triplet_correlator_re(correlator[counter][i], meson[G0G1], meson[G0G1], -1.0);
		triplet_correlator_re(tmpcorr, meson[G0G2], meson[G0G2], -1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] += tmpcorr[t];
		triplet_correlator_re(tmpcorr, meson[G0G3], meson[G0G3], -1.0);
		for(t = 0; t < T; t++) {
			correlator[counter][i][t] += tmpcorr[t];
			correlator[counter][i][t] /= 3.0;
		}
		counter++;
		
		if(i == 0) strcpy(corr_name[counter],"phi2");
		hairpin_re(correlator[counter][i], meson[G0G1], meson[G0G1], -1.0);
		hairpin_re(tmpcorr, meson[G0G2], meson[G0G2], -1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] += tmpcorr[t];
		hairpin_re(tmpcorr, meson[G0G3], meson[G0G3], -1.0);
		for(t = 0; t < T; t++) {
			correlator[counter][i][t] += tmpcorr[t];
			correlator[counter][i][t] /= 3.0;
			correlator[counter][i][t] = correlator[counter-1][i][t] - correlator[counter][i][t];
		}		
		counter++;
		*/
		
		/* scalar C=-1; forbidden Minkowski mesons */
		/*
		if(i == 0) strcpy(corr_name[counter],"forbidden triplet 0+-");
		triplet_correlator_re(correlator[counter][i], meson[G0], meson[G0], -1.0);
		counter++;

		if(i == 0) strcpy(corr_name[counter],"forbidden singlet 0+-");
		hairpin_re(correlator[counter][i], meson[G0], meson[G0], -1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] = correlator[counter-1][i][t] - correlator[counter][i][t];
		counter++;
		*/
		
		/* pseudovector C=+1; forbidden Minkowski mesons */
		/*
		if(i == 0) strcpy(corr_name[counter],"forbidden triplet 1++");
		triplet_correlator_re(correlator[counter][i], meson[G5G1], meson[G5G1], 1.0);
		triplet_correlator_re(tmpcorr, meson[G5G2], meson[G5G2], 1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] += tmpcorr[t];
		triplet_correlator_re(tmpcorr, meson[G5G3], meson[G5G3], 1.0);
		for(t = 0; t < T; t++) {
			correlator[counter][i][t] += tmpcorr[t];
			correlator[counter][i][t] /= 3.0;
		}
		counter++;
		
		if(i == 0) strcpy(corr_name[counter],"forbidden singlet 1++");
		if(i == 0) strcpy(corr_name[counter],"a");
		hairpin_re(correlator[counter][i], meson[G5G1], meson[G5G1], 1.0);
		hairpin_re(tmpcorr, meson[G5G2], meson[G5G2], 1.0);
		for(t = 0; t < T; t++)
			correlator[counter][i][t] += tmpcorr[t];
		hairpin_re(tmpcorr, meson[G5G3], meson[G5G3], 1.0);
		for(t = 0; t < T; t++) {
			correlator[counter][i][t] += tmpcorr[t];
			correlator[counter][i][t] /= 3.0;
			correlator[counter][i][t] = correlator[counter-1][i][t] - correlator[counter][i][t];
		}		
		counter++;
		*/
		
		/* pion decay amplitude */
		
		if(i == 0) strcpy(corr_name[counter],"pion decay amplitude");
		triplet_correlator_im(correlator[counter][i], meson[G5], meson[G0G5], 1.0);
		counter++;
		
		
	}
}






static void H(suNf_spinor *out, suNf_spinor *in){
	g5Dphi(hmass,out,in);
}



static void D(suNf_spinor *out, suNf_spinor *in){
	Dphi(hmass,out,in);
}



static suNf_spinor *h2tmp;
void H2(suNf_spinor *out, suNf_spinor *in){
  g5Dphi(hmass, h2tmp, in);
  g5Dphi(hmass, out, h2tmp);
}



static void all_to_all_quark_propagator_init(int n_masses) {
	int i, j;
	static int init_flag = 0;
	double requiredmemory = 0.0;
	
	if(init_flag != 0) return;

	set_spinor_len(VOLUME);

	/* static complex meson[n_gamma_matrices][n_sources*n_sources][T]; */
	requiredmemory += sizeof(complex)*n_gamma_matrices*n_sources*n_sources*T;
	
	for(i = 0; i < 2; i++)
		ws[i] = alloc_spinor_field_f();
	requiredmemory += sizeof(suNf_spinor)*VOLUME*2;
	
	for(i = 0; i < n_eigenvalues; i++)
		ev[i] = alloc_spinor_field_f();
	requiredmemory += sizeof(suNf_spinor)*VOLUME*n_eigenvalues;
	
	for(i = 0; i < n_diluted_noisy_sources; i++)
		noisy_sources[i] = alloc_spinor_field_f();
	requiredmemory += sizeof(suNf_spinor)*VOLUME*n_diluted_noisy_sources;
	
	for(i = 0; i < n_diluted_noisy_sources; i++) {
		noisy_sinks[i] = (suNf_spinor**)malloc(sizeof(suNf_spinor*)*n_masses);
		for(j = 0; j < n_masses; j++)
			noisy_sinks[i][j] = alloc_spinor_field_f();
	}
	requiredmemory += sizeof(suNf_spinor)*VOLUME*n_diluted_noisy_sources*n_masses;
	
	requiredmemory /= 1024.0*1024.0;
	lprintf("ALL_TO_ALL_QUARK_PROPAGATOR_INIT",0,"Required memory = %f Mb\n",requiredmemory);
	
	init_flag = 1;
}



/*
suNf_spinor* get_stored_source(int random_index, int dilution_index) {
	return noisy_sources[random_index*T+dilution_index];
}



suNf_spinor** get_stored_sinks(int random_index, int dilution_index) {
	return noisy_sinks[random_index*T+dilution_index];
}
*/



static void z2_spinor(suNf_spinor *source) {	
	double plus = sqrt(.5);
	double minus = -sqrt(.5);
	unsigned int len, i;
	double *sf = (double*)source;
	get_spinor_len(&len);
	for(i = 0; i < (sizeof(suNf_spinor)/sizeof(double))*len; i++)
		sf[i] = ((rand() & 1) == 0) ? plus : minus;
}



static void get_time_diluted_sources(suNf_spinor **source) {
	int i, t, x, index;
	static complex alpha[n_eigenvalues];
	
	for(t = 0; t < T; t++) {
		for(i = 0; i < n_eigenvalues; i++)
			alpha[i].re = alpha[i].im = 0.0;

		spinor_field_zero_f(source[t]);
		for(x = 0; x < VOL3; x++) {
			index = ipt_4d[t][x];
			z2_spinor(source[t] + index);
			for(i = 0; i < n_eigenvalues; i++) {
				_spinor_prod_assign_f(alpha[i],ev[i][index],source[t][index]);
			}
		}
		
		for(i = 0; i < n_eigenvalues; i++) {
			for(index = 0; index < VOLUME; index++) {
				_spinor_project_f(source[t][index],alpha[i],ev[i][index]);
			}
		}
	}
}



static void get_sinks(suNf_spinor *source, suNf_spinor **sink, int n_masses, double *mass, double acc) {
	static MINRES_par MINRESpar;
	int i, cgiter;
	
	hmass=mass[0];

	MINRESpar.err2 = acc;
	MINRESpar.max_iter = 0;

	cgiter = MINRES(&MINRESpar, &H, source, sink[0], 0);
	for(i = 1; i < n_masses; i++) {
		hmass = mass[i];
		cgiter += MINRES(&MINRESpar, &H, source, sink[i], sink[i-1]);
	}
	
	lprintf("GET_SINKS",10,"MINRES MVM = %d",cgiter);
}



static void source_sink_contraction(complex out[][16], suNf_spinor *source, suNf_spinor *sink, double z) {
	int i, j, t, x, index;
	suNf_vector *eta, *csi;
	
	for(t = 0; t < T; t++) {
		memset(out[t], '\0', sizeof(complex)*16);
		
		for(x = 0; x < VOL3; x++) {
			index = ipt_4d[t][x];
			
			for(i = 0; i < 4; i++) {
				csi = (suNf_vector*)(sink+index) + i;
				for(j = 0; j < 4; j++) {
					eta = (suNf_vector*)(source+index) + i;
					_vector_prod_assign_f(out[t][SPIN_2D_INDEX(i,j)], *eta, *csi);
				}
			}
			
		}
		
		for(i = 0; i < 16; i++) {
			out[t][i].re *= z/VOL3;
			out[t][i].im *= z/VOL3;
		}
	}

}



static void triplet_correlator_re(double* out, complex A[][T], complex B[][T], double x) {
	int j, k, rj, dj, rk, dk, t, dt, t1;
	const float z = (0.5f*n_sources)/(n_sources-1);
	complex *a;
	complex *b;
	
	for(dt = 0; dt < T; dt++)
		out[dt] = 0.0;
	
	for(j = 0; j < n_eigenvalues; j++) {
	
		for(k = 0; k < n_eigenvalues; k++) {
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,k)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,j)]+t1;
					out[dt] += a->re*b->re - a->im*b->im;
				}
			}
		}
		
		for(rk = 0; rk < n_global_noisy_sources/2; rk++)
		for(dk = 0; dk < n_dilution_slices; dk++) {
			k = n_eigenvalues + NOISY_INDEX(rk,dk);
			
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,k)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,j)]+t1;
					out[dt] += a->re*b->re - a->im*b->im;
				}
			}
		}
		
	}
	
	for(rj = n_global_noisy_sources/2+1; rj < n_global_noisy_sources; rj++)
	for(dj = 0; dj < n_dilution_slices; dj++) {
		j = n_eigenvalues + NOISY_INDEX(rj,dj);
		
		for(k = 0; k < n_eigenvalues; k++) {
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,k)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,j)]+t1;
					out[dt] += a->re*b->re - a->im*b->im;
				}
			}
		}
		
	}
	
	
	for(rj = 0; rj < n_global_noisy_sources; rj++)
	for(dj = 0; dj < n_dilution_slices; dj++) {
		j = n_eigenvalues + NOISY_INDEX(rj,dj);
		
		for(rk = rj+1; rk < n_global_noisy_sources; rk++)
		for(dk = 0; dk < n_dilution_slices; dk++) {
			k = n_eigenvalues + NOISY_INDEX(rk,dk);
			
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,k)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,j)]+t1;
					out[dt] += z*(a->re*b->re - a->im*b->im);
				}
			}
		}
		
	}
	
	for(dt = 0; dt < T; dt++)
		out[dt] *= z/T;
	
}



static void triplet_correlator_im(double* out, complex A[][T], complex B[][T], double x) {
	int j, k, rj, dj, rk, dk, t, dt, t1;
	const float z = (0.5f*n_sources)/(n_sources-1);
	complex *a;
	complex *b;
	
	for(dt = 0; dt < T; dt++)
		out[dt] = 0.0;
	
	for(j = 0; j < n_eigenvalues; j++) {
	
		for(k = 0; k < n_eigenvalues; k++) {
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,k)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,j)]+t1;
					out[dt] += a->re*b->im + a->im*b->re;
				}
			}
		}
		
		for(rk = 0; rk < n_global_noisy_sources/2; rk++)
		for(dk = 0; dk < n_dilution_slices; dk++) {
			k = n_eigenvalues + NOISY_INDEX(rk,dk);
			
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,k)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,j)]+t1;
					out[dt] += a->re*b->im + a->im*b->re;
				}
			}
		}
		
	}
	
	for(rj = n_global_noisy_sources/2+1; rj < n_global_noisy_sources; rj++)
	for(dj = 0; dj < n_dilution_slices; dj++) {
		j = n_eigenvalues + NOISY_INDEX(rj,dj);
		
		for(k = 0; k < n_eigenvalues; k++) {
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,k)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,j)]+t1;
					out[dt] += a->re*b->im + a->im*b->re;
				}
			}
		}
		
	}
	
	
	for(rj = 0; rj < n_global_noisy_sources; rj++)
	for(dj = 0; dj < n_dilution_slices; dj++) {
		j = n_eigenvalues + NOISY_INDEX(rj,dj);
		
		for(rk = rj+1; rk < n_global_noisy_sources; rk++)
		for(dk = 0; dk < n_dilution_slices; dk++) {
			k = n_eigenvalues + NOISY_INDEX(rk,dk);
			
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,k)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,j)]+t1;
					out[dt] += z*(a->re*b->im + a->im*b->re);
				}
			}
		}
		
	}
	
	for(dt = 0; dt < T; dt++)
		out[dt] *= x/T;
	
}



static void hairpin_re(double* out, complex A[][T], complex B[][T], double x) {
	int j, k, rj, dj, rk, dk, t, dt, t1;
	const float z = (2.0f*n_sources)/(n_sources-1);
	complex *a;
	complex *b;
	
	for(dt = 0; dt < T; dt++)
		out[dt] = 0.0;
	
	for(j = 0; j < n_eigenvalues; j++) {
	
		for(k = 0; k < n_eigenvalues; k++) {
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,j)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,k)]+t1;
					out[dt] += a->re*b->re - a->im*b->im;
				}
			}
		}
		
		for(rk = 0; rk < n_global_noisy_sources/2; rk++)
		for(dk = 0; dk < n_dilution_slices; dk++) {
			k = n_eigenvalues + NOISY_INDEX(rk,dk);
			
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,j)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,k)]+t1;
					out[dt] += a->re*b->re - a->im*b->im;
				}
			}
		}
		
	}
	
	for(rj = n_global_noisy_sources/2+1; rj < n_global_noisy_sources; rj++)
	for(dj = 0; dj < n_dilution_slices; dj++) {
		j = n_eigenvalues + NOISY_INDEX(rj,dj);
		
		for(k = 0; k < n_eigenvalues; k++) {
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,j)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,k)]+t1;
					out[dt] += a->re*b->re - a->im*b->im;
				}
			}
		}
		
	}
	
	
	for(rj = 0; rj < n_global_noisy_sources; rj++)
	for(dj = 0; dj < n_dilution_slices; dj++) {
		j = n_eigenvalues + NOISY_INDEX(rj,dj);
		
		for(rk = rj+1; rk < n_global_noisy_sources; rk++)
		for(dk = 0; dk < n_dilution_slices; dk++) {
			k = n_eigenvalues + NOISY_INDEX(rk,dk);
			
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,j)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,k)]+t1;
					out[dt] += z*(a->re*b->re - a->im*b->im);
				}
			}
		}
		
	}
	
	for(dt = 0; dt < T; dt++)
		out[dt] *= x/T;
	
}



static void hairpin_im(double* out, complex A[][T], complex B[][T], double x) {
	int j, k, rj, dj, rk, dk, t, dt, t1;
	const float z = (2.0f*n_sources)/(n_sources-1);
	complex *a;
	complex *b;
	
	for(dt = 0; dt < T; dt++)
		out[dt] = 0.0;
	
	for(j = 0; j < n_eigenvalues; j++) {
	
		for(k = 0; k < n_eigenvalues; k++) {
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,j)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,k)]+t1;
					out[dt] += a->re*b->im + a->im*b->re;
				}
			}
		}
		
		for(rk = 0; rk < n_global_noisy_sources/2; rk++)
		for(dk = 0; dk < n_dilution_slices; dk++) {
			k = n_eigenvalues + NOISY_INDEX(rk,dk);
			
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,j)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,k)]+t1;
					out[dt] += a->re*b->im + a->im*b->re;
				}
			}
		}
		
	}
	
	for(rj = n_global_noisy_sources/2+1; rj < n_global_noisy_sources; rj++)
	for(dj = 0; dj < n_dilution_slices; dj++) {
		j = n_eigenvalues + NOISY_INDEX(rj,dj);
		
		for(k = 0; k < n_eigenvalues; k++) {
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,j)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,k)]+t1;
					out[dt] += a->re*b->im + a->im*b->re;
				}
			}
		}
		
	}
	
	
	for(rj = 0; rj < n_global_noisy_sources; rj++)
	for(dj = 0; dj < n_dilution_slices; dj++) {
		j = n_eigenvalues + NOISY_INDEX(rj,dj);
		
		for(rk = rj+1; rk < n_global_noisy_sources; rk++)
		for(dk = 0; dk < n_dilution_slices; dk++) {
			k = n_eigenvalues + NOISY_INDEX(rk,dk);
			
			for(t = 0; t < T; t++) {
				a = A[SOUCE_SINK_INDEX(j,j)]+t;
				for(dt = 0; dt < T; dt++) {
					t1 = (t+dt)%T;
					b = B[SOUCE_SINK_INDEX(k,k)]+t1;
					out[dt] += z*(a->re*b->im + a->im*b->re);
				}
			}
		}
		
	}
	
	for(dt = 0; dt < T; dt++)
		out[dt] *= x/T;
	
}



#define GAMMA_TRACE_RE_DEFINITION \
static void NAME(complex* out, complex* smat) { \
	out->re = _S1_*smat[SPIN_2D_INDEX(1,_C1_)].re \
		+ _S2_*smat[SPIN_2D_INDEX(1,_C2_)].re \
		+ _S3_*smat[SPIN_2D_INDEX(1,_C3_)].re \
		+ _S4_*smat[SPIN_2D_INDEX(1,_C4_)].re; \
	out->im = _S1_*smat[SPIN_2D_INDEX(1,_C1_)].im \
		+ _S2_*smat[SPIN_2D_INDEX(1,_C2_)].im \
		+ _S3_*smat[SPIN_2D_INDEX(1,_C3_)].im \
		+ _S4_*smat[SPIN_2D_INDEX(1,_C4_)].im; \
}



#define GAMMA_TRACE_IM_DEFINITION \
static void NAME(complex* out, complex* smat) { \
	out->im = _S1_*smat[SPIN_2D_INDEX(1,_C1_)].re \
		+ _S2_*smat[SPIN_2D_INDEX(1,_C2_)].re \
		+ _S3_*smat[SPIN_2D_INDEX(1,_C3_)].re \
		+ _S4_*smat[SPIN_2D_INDEX(1,_C4_)].re; \
	out->re = - _S1_*smat[SPIN_2D_INDEX(1,_C1_)].im \
		- _S2_*smat[SPIN_2D_INDEX(1,_C2_)].im \
		- _S3_*smat[SPIN_2D_INDEX(1,_C3_)].im \
		- _S4_*smat[SPIN_2D_INDEX(1,_C4_)].im; \
}



#define NAME id_trace_H

#define _C1_ 1
#define _C2_ 2
#define _C3_ 3
#define _C4_ 4

#define _S1_ 1
#define _S2_ 1
#define _S3_ -1
#define _S4_ -1

GAMMA_TRACE_RE_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g0_trace_H

#define _C1_ 3
#define _C2_ 4
#define _C3_ 1
#define _C4_ 2

#define _S1_ -1
#define _S2_ -1
#define _S3_ 1
#define _S4_ 1

GAMMA_TRACE_RE_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g5_trace_H

#define _C1_ 1
#define _C2_ 2
#define _C3_ 3
#define _C4_ 4

#define _S1_ 1
#define _S2_ 1
#define _S3_ 1
#define _S4_ 1

GAMMA_TRACE_RE_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g0g5_trace_H

#define _C1_ 3
#define _C2_ 4
#define _C3_ 1
#define _C4_ 2

#define _S1_ -1
#define _S2_ -1
#define _S3_ -1
#define _S4_ -1

GAMMA_TRACE_RE_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g1_trace_H

#define _C1_ 4
#define _C2_ 3
#define _C3_ 2
#define _C4_ 1

#define _S1_ 1
#define _S2_ 1
#define _S3_ 1
#define _S4_ 1

GAMMA_TRACE_IM_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g2_trace_H

#define _C1_ 4
#define _C2_ 3
#define _C3_ 2
#define _C4_ 1

#define _S1_ -1
#define _S2_ 1
#define _S3_ -1
#define _S4_ 1

GAMMA_TRACE_RE_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g3_trace_H

#define _C1_ 3
#define _C2_ 4
#define _C3_ 1
#define _C4_ 2

#define _S1_ -1
#define _S2_ 1
#define _S3_ -1
#define _S4_ 1

GAMMA_TRACE_IM_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g0g1_trace_H

#define _C1_ 2
#define _C2_ 1
#define _C3_ 4
#define _C4_ 3

#define _S1_ -1
#define _S2_ -1
#define _S3_ -1
#define _S4_ -1

GAMMA_TRACE_IM_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g0g2_trace_H

#define _C1_ 2
#define _C2_ 1
#define _C3_ 4
#define _C4_ 3

#define _S1_ 1
#define _S2_ -1
#define _S3_ 1
#define _S4_ -1

GAMMA_TRACE_RE_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g0g3_trace_H

#define _C1_ 1
#define _C2_ 2
#define _C3_ 3
#define _C4_ 4

#define _S1_ 1
#define _S2_ -1
#define _S3_ 1
#define _S4_ -1

GAMMA_TRACE_IM_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g5g1_trace_H

#define _C1_ 4
#define _C2_ 3
#define _C3_ 2
#define _C4_ 1

#define _S1_ -1
#define _S2_ -1
#define _S3_ 1
#define _S4_ 1

GAMMA_TRACE_IM_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g5g2_trace_H

#define _C1_ 4
#define _C2_ 3
#define _C3_ 2
#define _C4_ 1

#define _S1_ 1
#define _S2_ -1
#define _S3_ -1
#define _S4_ 1

GAMMA_TRACE_RE_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g5g3_trace_H

#define _C1_ 3
#define _C2_ 4
#define _C3_ 1
#define _C4_ 2

#define _S1_ 1
#define _S2_ -1
#define _S3_ -1
#define _S4_ 1

GAMMA_TRACE_IM_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g0g5g1_trace_H

#define _C1_ 2
#define _C2_ 1
#define _C3_ 4
#define _C4_ 3

#define _S1_ 1
#define _S2_ 1
#define _S3_ -1
#define _S4_ -1

GAMMA_TRACE_IM_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g0g5g2_trace_H

#define _C1_ 2
#define _C2_ 1
#define _C3_ 4
#define _C4_ 3

#define _S1_ -1
#define _S2_ 1
#define _S3_ 1
#define _S4_ -1

GAMMA_TRACE_RE_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_



#define NAME g0g5g3_trace_H

#define _C1_ 1
#define _C2_ 2
#define _C3_ 3
#define _C4_ 4

#define _S1_ -1
#define _S2_ 1
#define _S3_ 1
#define _S4_ -1

GAMMA_TRACE_IM_DEFINITION

#undef NAME
#undef _C1_
#undef _C2_
#undef _C3_
#undef _C4_
#undef _S1_
#undef _S2_
#undef _S3_
#undef _S4_
