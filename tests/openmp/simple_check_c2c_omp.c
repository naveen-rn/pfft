#include <complex.h>
#include <pfft.h>
#include <omp.h>

#define NNN 256
/* The size of the transformation will be NNN^3*/

/* USAGE:
 *   -pfft_omp_threads nthreads
 *   -pfft_runs runs
 * both as integer parameters */


int main(int argc, char **argv)
{
  int nthreads=1; /*number of threads to initialize openmp with*/
  int runs=1; /*number of runs for testing*/
  int np[2];
  ptrdiff_t n[3];
  ptrdiff_t alloc_local;
  ptrdiff_t local_ni[3], local_i_start[3];
  ptrdiff_t local_no[3], local_o_start[3];
  double err;
  pfft_complex *in, *out;
  pfft_plan plan_forw=NULL, plan_back=NULL;
  MPI_Comm comm_cart_2d;

  /* Read command line arguments */
  pfft_get_args(argc,argv,"-pfft_omp_threads",1,PFFT_INT,&nthreads);
  pfft_get_args(argc,argv,"-pfft_runs",1,PFFT_INT,&runs);
  pfft_printf(MPI_COMM_WORLD, "# %4d threads will be used for openmp (default is 1)\n", nthreads);

  /* Set size of FFT and process mesh */
  n[0] = NNN;n[1] =NNN; n[2] =NNN;
  np[0] = 1; np[1] = 1;
  
  /* Initialize MPI and PFFT (including OpenMP) */
  MPI_Init(&argc, &argv);
  pfft_init();

  /* Init OpenMP with specified number of threads */
  pfft_plan_with_nthreads(nthreads);

 /* Create two-dimensional process grid of size np[0] x np[1], if possible */
  if( pfft_create_procmesh_2d(MPI_COMM_WORLD, np[0], np[1], &comm_cart_2d) ){
    pfft_fprintf(MPI_COMM_WORLD, stderr, "Error: This test file only works with %d processes.\n", np[0]*np[1]);
    MPI_Finalize();
    return 1;
  }

  /* Get parameters of data distribution */
  alloc_local = pfft_local_size_dft_3d(n, comm_cart_2d, PFFT_TRANSPOSED_NONE,
      local_ni, local_i_start, local_no, local_o_start);

  /* Allocate memory */
  in  = pfft_alloc_complex(alloc_local);
  out = pfft_alloc_complex(alloc_local);

  /* Plan parallel forward FFT */
  plan_forw = pfft_plan_dft_3d(
      n, in, out, comm_cart_2d, PFFT_FORWARD, PFFT_TRANSPOSED_OUT| PFFT_MEASURE| PFFT_DESTROY_INPUT| PFFT_TUNE| PFFT_SHIFTED_IN);
  
  /* Plan parallel backward FFT */
  plan_back = pfft_plan_dft_3d(
      n, out, in, comm_cart_2d, PFFT_BACKWARD, PFFT_TRANSPOSED_IN| PFFT_MEASURE| PFFT_DESTROY_INPUT| PFFT_TUNE| PFFT_SHIFTED_OUT);

  /* Initialize input with random numbers */
  pfft_init_input_complex_3d(n, local_ni, local_i_start,
      in);

  for(int i=0; i<runs; i++)
  {
    /* execute parallel forward FFT */
    pfft_execute(plan_forw);

    /* clear the old input */
    /* pfft_clear_input_complex_3d(n, local_ni, local_i_start,
        in);
    */
    /* execute parallel backward FFT */
    pfft_execute(plan_back);

    /* Scale data */
    ptrdiff_t l;
    for(l=0; l < local_ni[0] * local_ni[1] * local_ni[2]; l++)
      in[l] /= (n[0]*n[1]*n[2]);
  }

  pfft_print_average_timer_adv(plan_forw, MPI_COMM_WORLD);
  pfft_print_average_timer_adv(plan_back, MPI_COMM_WORLD);

  /* Print error of back transformed data */
  err = pfft_check_output_complex_3d(n, local_ni, local_i_start, in, comm_cart_2d);
  pfft_printf(comm_cart_2d, "Error after %d forward and backward trafos of size n=(%td, %td, %td):\n", runs, n[0], n[1], n[2]); 
  pfft_printf(comm_cart_2d, "maxerror = %6.2e;\n", err);
  
  /* free mem and finalize */
  pfft_destroy_plan(plan_forw);
  pfft_destroy_plan(plan_back);
  MPI_Comm_free(&comm_cart_2d);
  pfft_free(in); pfft_free(out);
  MPI_Finalize();
  return 0;
}
