#include <fti.h>
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../../../deps/iniparser/iniparser.h"
#include "../../../deps/iniparser/dictionary.h"

/**
 * run with x mpi rank, where x is the cube of an integer.
 */

/**
 *
 * This test writes integers from 1 -> x*y into a 2 dim
 * dataset inside a hdf5 file. The integers are ordered
 * as follows:
 *  
 *          |- x -|       
 * +---------------------------+
 * | 1  2  3  4  ... x-2 x-1 x | –
 * | x+1 x+2 x+3               | |      
 * | .                         | y
 * | .                         | |                   
 * | .                         | _                 
 * | (y-1)x+1 ... xy-2 xy-3 xy |      
 * +---------------------------+
 *  
 * The values for x and y are restricted so that x/sqrt(n) 
 * and y/sqrt(n) must be integers.
 *
 * The grid is partitioned into cells of size 
 * x/sqrt(n) * y/sqrt(n) and the domain decomposition
 * follows a round-robin distribution to the ranks of all
 * cells of that size.
 *
 *          |- x -|
 * +-------------------------+
 * | 1 | 2 | 3 | ... |sqrt(n)| –
 * +---+---+---+     +-------+ |
 * | .                       | y
 * | .                       | |                   
 * | .               +-------| _               
 * |   ...  ...      |   n   |      
 * +-----------------+-------+
 *
 * thus in a quadratic decomposition.
 *
 * In this example, the dataset is not completely contiguous.
 * Only the rows of the 2 dim subsets are contiguous.
 *
 */

#define X 64
#define Y (1024*256)

#define fdim0 X
#define fdim1 Y

#define fn "row-conti.h5"
#define dn "shared dataset"

int main() {

//
// -->> INIT AND DEFINITIONS
//
    
    int rank, grank; 
    int size; 
    int size_bac; 
    int i,j;

    MPI_Init(NULL,NULL);
    
    // determine global rank
    MPI_Comm_rank(MPI_COMM_WORLD, &grank);
    
    dictionary *ini = iniparser_load( "config.fti" );
    int restart = (int)iniparser_getint(ini, "Restart:failure", -1);

    FTI_Init("config.fti", MPI_COMM_WORLD);

    MPI_Comm_rank(FTI_COMM_WORLD, &rank);
    MPI_Comm_size(FTI_COMM_WORLD, &size);
    
    int nbHeads = (int)iniparser_getint(ini, "Basic:head", -1); 
    int nodeSize = (int)iniparser_getint(ini, "Basic:node_size", -1);
    if ( (nbHeads<0) || (nodeSize<0) ) {
        printf("wrong configuration (for head or node-size settings)!\n");
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    int finalTag = (int)iniparser_getint(ini, "Advanced:final_tag", 3107);
    int headRank = grank - grank%nodeSize;
    
    switch( restart ) {
        case -1:
            printf("wrong configuration (Restart:failure < 0)!\n");
            MPI_Abort(MPI_COMM_WORLD, -1);
        case 1:
            if( size > 16 ) {
                printf("wrong configuration for restart (Restart:failure '%d != 3')!\n", restart);
                MPI_Abort(MPI_COMM_WORLD, -1);
            }
        case 0:
        case 3:
            break;
        default:
            printf("invalid configuration for restart\n");
            MPI_Abort(MPI_COMM_WORLD, -1);
    }


    size_bac = size;

    int ldim0 = fdim0/((int)sqrt(size));
    int ldim1 = fdim1/((int)sqrt(size));
    
    // define local properties
    hsize_t offset[] = { ((rank/((int)sqrt(size)))%((int)sqrt(size)))*ldim0, (rank%((int)sqrt(size)))*ldim1 };
    hsize_t count[] = { 1, ldim1 };
    
    // set dataset properties
    hsize_t fdim[2] = { fdim0, fdim1 }; 
 
    // check for correct behavior using define datatypes
    struct STRUCT {
        int one;
        int two;
    };

    // create FTI type of structure
    FTIT_complexType FTI_NEW_STRUCT_def;
    FTI_AddSimpleField( &FTI_NEW_STRUCT_def, &FTI_INTG, offsetof(struct STRUCT, one), 0, "one" ); 
    FTI_AddSimpleField( &FTI_NEW_STRUCT_def, &FTI_INTG, offsetof(struct STRUCT, two), 1, "two" ); 
    FTIT_type FTI_NEW_STRUCT;
    FTI_InitComplexType( &FTI_NEW_STRUCT, &FTI_NEW_STRUCT_def, 2, sizeof(struct STRUCT), "struct_one_two", NULL );

    // create a group
    FTIT_H5Group gr;
    FTI_InitGroup( &gr, "sructgroup", NULL );

    // create datasets
    FTI_DefineGlobalDataset( 0, 2, fdim, dn, NULL, FTI_INTG );
    FTI_DefineGlobalDataset( 1, 2, fdim, "struct", &gr, FTI_NEW_STRUCT );
    
    // create row contiguous array and add rows to dataset
    int **data = (int**) malloc( sizeof(int*) * ldim0 );
    struct STRUCT **sdata = (struct STRUCT**) malloc( sizeof(struct STRUCT*) * ldim0 );
    for(i=0; i<ldim0; ++i) {
        data[i] = (int*) malloc( sizeof(int) * ldim1 );
        sdata[i] = (struct STRUCT*) malloc( sizeof(struct STRUCT) * ldim1 );
        FTI_Protect( i, data[i], ldim1, FTI_INTG );
        FTI_Protect( i+ldim0, sdata[i], ldim1, FTI_NEW_STRUCT );
        FTI_AddSubset( i, 2, offset, count, 0 );
        FTI_AddSubset( i+ldim0, 2, offset, count, 1 );
        offset[0]++;
    }
    MPI_Barrier(FTI_COMM_WORLD);
    
    // reset offset 0 to original value
    offset[0] = ((rank/((int)sqrt(size)))%((int)sqrt(size)))*ldim0;

//
// -->> CHECKPOINT AND RESTART
//
    
    // RESTART
    if( restart ) {
        for(i=0; i<ldim0; ++i) {
            for(j=0; j<ldim1; ++j) {
                data[i][j] = -1;
            }
        }
        FTI_Recover();
        int out[3];
        bzero( out, 3*sizeof(int) );
        for(i=0; i<ldim0; ++i) {
            for(j=0; j<ldim1; ++j) {
                out[0] += data[i][j];
                out[1] += sdata[i][j].one;
                out[2] += sdata[i][j].two;
            }
        }
        int res[3];
        int check = fdim[0]*fdim[1]*(fdim[0]*fdim[1]+1)/2;
        MPI_Allreduce( &out, &res, 3, MPI_INT, MPI_SUM, FTI_COMM_WORLD ); 
        bool success = ( (res[0] == check) && (res[1] == check) && (res[2] == (2*check)) );
        if(rank==0) { 
            printf("[%s]\n", (success) ? "SUCCESS" : "FAILURE", res, check ); 
        }
        FTI_Finalize();
        for(i=0; i<ldim0; ++i) {
            free(data[i]);
        }
        
        free(ini);

        MPI_Finalize();

        exit(!success);
    
    // CHECKPOINT
    } else {

        //
        // -->> INIT DATASET
        //
    
        int base = offset[0]*fdim[1] + offset[1] + 1;
        int stride = 0;
        // init array elements with increasing integers
        for(i=0; i<ldim0; ++i) {
            for(j=0; j<ldim1; ++j) {
                data[i][j] = base;
                sdata[i][j].one = base;
                sdata[i][j].two = 2*base;
                base++;
            }
            base += fdim[1] - ldim1;
        }
        FTI_Checkpoint( 1, FTI_L4_H5_SINGLE );
        FTI_Checkpoint( 2, FTI_L4_H5_SINGLE );
        FTI_Checkpoint( 3, FTI_L4_H5_SINGLE );
        FTI_Checkpoint( 4, FTI_L4_H5_SINGLE );
        FTI_Checkpoint( 5, FTI_L4_H5_SINGLE );
        FTI_Checkpoint( 6, FTI_L4_H5_SINGLE );
        FTI_Checkpoint( 7, FTI_L4_H5_SINGLE );
        FTI_Checkpoint( 8, 1 );
        FTI_Checkpoint( 9, 2 );
        FTI_Checkpoint( 10, 3 );
        FTI_Checkpoint( 11, 4 );
        FTI_Checkpoint( 12, FTI_L4_H5_SINGLE );
        
        // simulate crash
        if( nbHeads > 0 ) { 
            int value = FTI_ENDW;
            MPI_Send(&value, 1, MPI_INT, headRank, finalTag, MPI_COMM_WORLD);
            MPI_Barrier(MPI_COMM_WORLD);
        }
        
        free(ini);
        
        MPI_Finalize();
        exit(0);
    }

}

