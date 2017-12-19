#ifndef _FTIFF_H
#define _FTIFF_H

#define DECL_MPI_TYPES(TYPE) MPI_Datatype MPI_ ## TYPE ## _RAW, MPI_ ## TYPE
#define MBR_CNT(TYPE) int TYPE ## _mbrCnt
#define MBR_BLK_LEN(TYPE) int TYPE ## _mbrBlkLen[]
#define MBR_TYPES(TYPE) MPI_Datatype TYPE ## _mbrTypes[]
#define MBR_DISP(TYPE) MPI_Aint TYPE ## _mbrDisp[]


#include "fti.h"
#include <assert.h>

/*
 *  structure defines
 */

// create struct to send info
// send nbVar to knwo how many elements head must receive of varSize and varID
typedef struct FTIFF_headInfo {
    int exists; 
    int nbVar;
    char ckptFile[FTI_BUFS];
    long maxFs;
    long fs;
    long pfs;
} FTIFF_headInfo;

/*
 *  MPI datatypes
 */

// general variables
MPI_Aint lb, extent;

// generic types
typedef struct FTIFF_MPITypeInfo {
    MPI_Datatype        raw;
    MPI_Datatype        final;
    int                 mbrCnt;
    int*                mbrBlkLen;
    MPI_Datatype*       mbrTypes;
    MPI_Aint*           mbrDisp;
} FTIFF_MPITypeInfo;

// ID MPI types
enum {
    FTIFF_HEAD_INFO,
    FTIFF_NUM_MPI_TYPES
};

// declare MPI datatypes
extern FTIFF_MPITypeInfo FTIFF_MPITypes[FTIFF_NUM_MPI_TYPES];

// Function definitions
int FTIFF_InitMpiTypes();
void FTIFF_FreeDbFTIFF(FTIFF_db* last);
int FTIFF_Checksum(FTIT_execution* FTI_Exec, FTIT_dataset* FTI_Data, char* checksum);
int FTIFF_Recover( FTIT_execution *FTI_Exec, FTIT_dataset *FTI_Data, FTIT_checkpoint *FTI_Ckpt );
int FTIFF_RecoverVar( int id, FTIT_execution *FTI_Exec, FTIT_dataset *FTI_Data, FTIT_checkpoint *FTI_Ckpt );
int FTIFF_UpdateDatastructFTIFF( FTIT_execution* FTI_Exec, FTIT_dataset* FTI_Data );
int FTIFF_ReadDbFTIFF( FTIT_execution *FTI_Exec, FTIT_checkpoint* FTI_Ckpt );
int FTIFF_WriteFTIFF(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
                    FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt,
                    FTIT_dataset* FTI_Data);
int FTIFF_CheckL1RecoverInit( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
                  FTIT_checkpoint* FTI_Ckpt );
int FTIFF_CheckL2RecoverInit( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
                  FTIT_checkpoint* FTI_Ckpt, int *exists);
int FTIFF_CheckL3RecoverInit( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
                  FTIT_checkpoint* FTI_Ckpt, int* erased);
int FTIFF_CheckL4RecoverInit( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
                  FTIT_checkpoint* FTI_Ckpt, char *checksum);
#endif
