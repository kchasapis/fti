/**
 *  @file   check.c
 *  @author Kai Keller (kellekai@gmx.de)
 *  @date   June, 2017
 *  @brief  FTI testing program.
 *
 *	The program may test the correct behaviour for checkpoint
 *	and restart for all configurations. The recovered data is also
 *	tested upon correct data fields.
 *
 *	The program takes four arguments:
 *	  - arg1: FTI configuration file
 *	  - arg2: Interrupt yes/no (1/0)
 *	  - arg3: Checkpoint level (1, 2, 3, 4)
 *	  - arg4: different ckpt. sizes yes/no (1/0)
 *
 * If arg2 = 0, the program simulates a clean run of FTI:
 *    FTI_Init
 *    FTI_Protect
 *    if FTI_Status = 0
 *      FTI_Checkpoint
 *    else
 *      FTI_Recover
 *    FTI_Finalize
 *
 * If arg2 = 1, the program simulates an execution failure:
 *    FTI_Init
 *    FTI_Protect
 *    if FTI_Status = 0
 *      exit(10)
 *    else
 *      FTI_Recover
 *    FTI_Finalize
 *
 */

#include "mpi.h"
#include "fti.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#define CUDA_CALL_SAFE(f)                                                                       \
do {                                                                                            \
    cudaError_t _e = f;                                                                          \
    if(_e != cudaSuccess) {                                                                    \
      fprintf(stderr, "Cuda error %d %s:: %s\n", __LINE__, __func__, cudaGetErrorString(_e));  \
      exit(EXIT_FAILURE);                                                                       \
    }                                                                                           \
} while(0)

#define BLOCK_SIZE 1024

//#define N 100000
//#define N ((size_t)1 << 25)
#define N ((size_t)1 << 20)
#define CNTRLD_EXIT 10
#define RECOVERY_FAILED 20
#define DATA_CORRUPT 30
#define KEEP 2
#define RESTART 1
#define INIT 0

/**
 * function prototypes
 */

/*-------------------------------------------------------------------------*/
/**
    @brief      Initialize test data
    @param      [out] A				Unit vector (1, 1, ....., 1)
    @param      [out] B				Random vector
    @param      [in] asize			Dimension

	Initializes A with 1's and B with random numbers r,  0 <= r <= 5.
	Dimension of both vectors is 'asize'
 **/
/*-------------------------------------------------------------------------*/
void init_arrays(double* A, double* B, size_t asize);

/*-------------------------------------------------------------------------*/
/**
    @brief      [CUDA] Multiplies components of A and B and stores result into A
    @param      [in/out] A			Unit vector (1, 1, ....., 1)
    @param      [in] B				Random vector
    @param      [in] asize			Dimension

    After function call, A equals B.
 **/
/*-------------------------------------------------------------------------*/
__global__
void vecmult(double* A, double* B, size_t asize);

/*-------------------------------------------------------------------------*/
/**
    @brief      Validifies the recovered data
    @param      [in] A			    A returned from vecmult
    @param      [in] B_chk			POSIX Backup of B
    @param      [in] asize			Dimension
    @return     integer             0 if successful, -1 else.

    Checks entry for entry if A equals the POSIX Backup of B, B_chk, from
    the preceding execution. This function must be called after the call to
    vecmult(A, B, asize).
 **/
/*-------------------------------------------------------------------------*/
int validify(double* A, double* B_chk, size_t asize);

/*-------------------------------------------------------------------------*/
/**
    @brief      Writes 'B' and 'asize' to file, using POSIX fwrite.
    @param      [in] B              Random array B from init_array call
    @param      [in] asize			Dimension
    @param      [in] rank           FTI application rank
 **/
/*-------------------------------------------------------------------------*/
int write_data(double* B, size_t* asize, int rank);

/*-------------------------------------------------------------------------*/
/**
    @brief      Recovers 'B' and 'asize' to 'B_chk' and 'asize_chk' from file,
                using POSIX fread.
    @param      [out] B_chk         B backup
    @param      [out] asize_chk     Dimension backup
    @param      [in] rank           FTI application rank
    @param      [in] asize			Dimension
    @return     integer             0 if successful, -1 else.

    Before recovering B, the function checks if 'asize_chk' equals 'asize',
    to prevent SIGSEGV. If not 'asize_chk' = 'asize' it returns -1.
 **/
/*-------------------------------------------------------------------------*/
int read_data(double* B_chk, size_t* asize_chk, int rank, size_t asize);

/**
 * main
 */

int main(int argc, char* argv[]) {

    unsigned char parity, crash, level, state, diff_sizes;
    int FTI_APP_RANK, result, tmp, success = 1;
    double *h_A, *h_B, *B_chk;
    double *d_A, *d_B;

    size_t asize, asize_chk;
    size_t asize_with_dt;

    int blocks_per_grid, threads_per_block;

    srand(time(NULL));

    MPI_Init(&argc, &argv);
    result = FTI_Init(argv[1], MPI_COMM_WORLD);
    if (result == FTI_NREC) {
        exit(RECOVERY_FAILED);
    }

    crash = atoi(argv[2]);
    level = atoi(argv[3]);
    diff_sizes = atoi(argv[4]);

    MPI_Comm_rank(FTI_COMM_WORLD,&FTI_APP_RANK);

    asize = N;

    if (diff_sizes) {
        parity = FTI_APP_RANK%7;

        switch (parity) {

            case 0:
                asize = N;
                break;

            case 1:
                asize = 2*N;
                break;

            case 2:
                asize = 3*N;
                break;

            case 3:
                asize = 4*N;
                break;

            case 4:
                asize = 5*N;
                break;

            case 5:
                asize = 6*N;
                break;

            case 6:
                asize = 7*N;
                break;

        }
    }

    asize_with_dt = asize * sizeof(double);

    threads_per_block = BLOCK_SIZE;
    blocks_per_grid = (asize + threads_per_block - 1) / threads_per_block;
    
    state = FTI_Status();

    if ( (FTI_APP_RANK == 0) && (state == INIT) ) { printf(
    "<< ALLOCATE GPU MEMORY >>\n"
    "## \n"
    "## h_A = (double*) malloc(asize_with_dt);\n"
    "## h_B = (double*) malloc(asize_with_dt);\n"
    "## \n"
    "## CUDA_CALL_SAFE(cudaMalloc(&d_A, asize_with_dt));\n"
    "## CUDA_CALL_SAFE(cudaMalloc(&d_B, asize_with_dt));\n"
    "## \n"
    "## FTI_Protect(0, d_A, asize, FTI_DBLE);\n"
    "## FTI_Protect(1, d_B, asize, FTI_DBLE);\n"
    "## FTI_Protect(2, &asize, 1, FTI_INTG);\n");}

    h_A = (double*) malloc(asize_with_dt);
    h_B = (double*) malloc(asize_with_dt);

    CUDA_CALL_SAFE(cudaMalloc(&d_A, asize_with_dt));
    CUDA_CALL_SAFE(cudaMalloc(&d_B, asize_with_dt));

    FTI_Protect(0, d_A, asize, FTI_DBLE);
    FTI_Protect(1, d_B, asize, FTI_DBLE);
    FTI_Protect(2, &asize, 1, FTI_INTG);

    if (state == INIT) {
        if (FTI_APP_RANK == 0) { printf(
        "<< SEND DATA TO GPU MEMORY PERFORM CHECKPOINT AND SIMULATE CRASH >>\n"
        "## \n"
        "## init_arrays(h_A, h_B, asize);\n"
        "## write_data(h_B, &asize, FTI_APP_RANK);\n"
        "## CUDA_CALL_SAFE(cudaMemcpy(d_A, h_A, asize_with_dt, cudaMemcpyHostToDevice));\n"
        "## CUDA_CALL_SAFE(cudaMemcpy(d_B, h_B, asize_with_dt, cudaMemcpyHostToDevice));\n"
        "## MPI_Barrier(FTI_COMM_WORLD);\n"
        "## FTI_Checkpoint(1,level);\n"
        "## if (crash && FTI_APP_RANK == 0) {\n"
        "##     exit(CNTRLD_EXIT);\n"
        "## }\n"); }
        init_arrays(h_A, h_B, asize);
        write_data(h_B, &asize, FTI_APP_RANK);
        CUDA_CALL_SAFE(cudaMemcpy(d_A, h_A, asize_with_dt, cudaMemcpyHostToDevice));
        CUDA_CALL_SAFE(cudaMemcpy(d_B, h_B, asize_with_dt, cudaMemcpyHostToDevice));
        MPI_Barrier(FTI_COMM_WORLD);
        FTI_Checkpoint(1,level);
        //sleep(5);
        if (crash && FTI_APP_RANK == 0) {
            MPI_Abort(MPI_COMM_WORLD,0);
        }
    }

    if ( state == RESTART || state == KEEP ) {
        result = FTI_Recover();
        if (result != FTI_SCES) {
            exit(RECOVERY_FAILED);
        }
        B_chk = (double*) malloc(asize_with_dt);
        result = read_data(B_chk, &asize_chk, FTI_APP_RANK, asize);
        MPI_Barrier(FTI_COMM_WORLD);
        if (result != 0) {
            exit(DATA_CORRUPT);
        }
    }

    /*
     * on INIT, B is initialized randomly
     * on RESTART or KEEP, B is recovered and must be equal to B_chk
     */

    if ( (FTI_APP_RANK == 0) ) { printf(
    "<< VALIDATE RESULT AFTER RECOVERY >>\n"
    "## \n"
    "## vecmult<<< blocks_per_grid, threads_per_block >>>(d_A, d_B, asize);\n"
    "## CUDA_CALL_SAFE(cudaDeviceSynchronize());\n"
    "## CUDA_CALL_SAFE(cudaMemcpy(h_A, d_A, asize_with_dt, cudaMemcpyDeviceToHost));\n"
    "## result = validify(h_A, B_chk, asize);\n"
    "## result += (asize_chk == asize) ? 0 : -1;\n"
    "## MPI_Allreduce(&result, &result_glb, 1, MPI_INT, MPI_SUM, FTI_COMM_WORLD);\n"); }

    vecmult<<< blocks_per_grid, threads_per_block >>>(d_A, d_B, asize);
    CUDA_CALL_SAFE(cudaDeviceSynchronize());
    CUDA_CALL_SAFE(cudaMemcpy(h_A, d_A, asize_with_dt, cudaMemcpyDeviceToHost));

    if (state == RESTART || state == KEEP) {
        result = validify(h_A, B_chk, asize);
        result += (asize_chk == asize) ? 0 : -1;
        MPI_Allreduce(&result, &tmp, 1, MPI_INT, MPI_SUM, FTI_COMM_WORLD);
        result = tmp;
        free(B_chk);
    }

    CUDA_CALL_SAFE(cudaFree(d_A));
    CUDA_CALL_SAFE(cudaFree(d_B));

    if (FTI_APP_RANK == 0 && (state == RESTART || state == KEEP)) {
        if (result == 0) {
            printf("[SUCCESSFUL]\n");
        } else {
            printf("[NOT SUCCESSFUL]\n");
            success=0;
        }
    }

    MPI_Barrier(FTI_COMM_WORLD);
    FTI_Finalize();
    MPI_Finalize();

    if (success == 1)
        return 0;
    else
        exit(DATA_CORRUPT);

}

/**
 * function definitions
 */

void init_arrays(double* A, double* B, size_t asize) {
    int i;
    for (i = 0; i< asize; i++) {
        A[i] = 1.0;
        B[i] = ((double)rand()/RAND_MAX)*5.0;
    }
}

__global__
void vecmult(double* A, double* B, size_t asize) {
    size_t i = (size_t)blockIdx.x * (size_t)blockDim.x + (size_t)threadIdx.x;
    if (i < asize) {
        A[i] = A[i] * B[i];
    }
}

int validify(double* A, double* B_chk, size_t asize) {
    int i;
    for (i=0; i<asize; i++) {
        if (A[i] != B_chk[i]){
            return -1;
        }
    }
    return 0;
}

int write_data(double* B, size_t *asize, int rank) {
    char str[256];
    sprintf(str, "chk/check-%i.tst", rank);
    FILE* f = fopen(str, "wb");
    size_t written = 0;

    fwrite( (void*) asize, sizeof(size_t), 1, f);

    while ( written < (*asize) ) {
        written += fwrite( (void*) B, sizeof(double), (*asize), f);
    }

    fclose(f);

    return 0;
}

int read_data(double* B_chk, size_t *asize_chk, int rank, size_t asize) {
    char str[256];
    sprintf(str, "chk/check-%i.tst", rank);
    FILE* f = fopen(str, "rb");
    size_t read = 0;

    fread( (void*) asize_chk, sizeof(size_t), 1, f);
    if ((*asize_chk) != asize) {
        printf("[ERROR -%i] : wrong dimension 'asize' -- asize: %zd, asize_chk: %zd\n", rank, asize, *asize_chk);
        fflush(stdout);
        return -1;
    }
    while ( read < (*asize_chk) ) {
        read += fread( (void*) B_chk, sizeof(double), (*asize_chk), f);
    }

    fclose(f);

    return 0;
}
