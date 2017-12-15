/**
 *  Copyright (c) 2017 Leonardo A. Bautista-Gomez
 *  All rights reserved
 *
 *  FTI - A multi-level checkpointing library for C/C++/Fortran applications
 *
 *  Revision 1.0 : Fault Tolerance Interface (FTI)
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its contributors
 *  may be used to endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  @file   meta.c
 *  @date   October, 2017
 *  @brief  Metadata functions for the FTI library.
 */

#include "interface.h"

/*-------------------------------------------------------------------------*/
/**
    @brief      It gets the checksums from metadata.
    @param      FTI_Conf        Configuration metadata.
    @param      FTI_Exec        Execution metadata.
    @param      FTI_Topo        Topology metadata.
    @param      FTI_Ckpt        Checkpoint metadata.
    @param      checksum        Pointer to fill the checkpoint checksum.
    @param      ptnerChecksum   Pointer to fill the ptner file checksum.
    @param      rsChecksum      Pointer to fill the RS file checksum.
    @return     integer         FTI_SCES if successful.

    This function reads the metadata file created during checkpointing and
    recovers the checkpoint checksum. If there is no RS file, rsChecksum
    string length is 0.

 **/
/*-------------------------------------------------------------------------*/
int FTI_GetChecksums(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
                    FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt,
                    char* checksum, char* ptnerChecksum, char* rsChecksum)
{

    char mfn[FTI_BUFS]; //Path to the metadata file
    char str[FTI_BUFS]; //For console output
    if (FTI_Exec->ckptLvel == 0) {
        sprintf(mfn, "%s/sector%d-group%d.fti", FTI_Conf->mTmpDir, FTI_Topo->sectorID, FTI_Topo->groupID);
    }
    else {
        sprintf(mfn, "%s/sector%d-group%d.fti", FTI_Ckpt[FTI_Exec->ckptLvel].metaDir, FTI_Topo->sectorID, FTI_Topo->groupID);
    }

    sprintf(str, "Getting FTI metadata file (%s)...", mfn);
    FTI_Print(str, FTI_DBUG);
    if (access(mfn, R_OK) != 0) {
        FTI_Print("FTI metadata file NOT accessible.", FTI_WARN);
        return FTI_NSCS;
    }
    dictionary* ini = iniparser_load(mfn);
    if (ini == NULL) {
        FTI_Print("Iniparser failed to parse the metadata file.", FTI_WARN);
        return FTI_NSCS;
    }

    //Get checksum of checkpoint file
    sprintf(str, "%d:Ckpt_checksum", FTI_Topo->groupRank);
    char* checksumTemp = iniparser_getstring(ini, str, NULL);
    strncpy(checksum, checksumTemp, MD5_DIGEST_STRING_LENGTH);

    //Get checksum of partner checkpoint file
    sprintf(str, "%d:Ckpt_checksum", (FTI_Topo->groupRank + FTI_Topo->groupSize - 1) % FTI_Topo->groupSize);
    checksumTemp = iniparser_getstring(ini, str, NULL);
    strncpy(ptnerChecksum, checksumTemp, MD5_DIGEST_STRING_LENGTH);

    //Get checksum of Reed-Salomon file
    sprintf(str, "%d:RSed_checksum", FTI_Topo->groupRank);
    checksumTemp = iniparser_getstring(ini, str, NULL);

    //If RS checksum don't exists length set to 0;
    if (checksumTemp != NULL) {
        strncpy(rsChecksum, checksumTemp, MD5_DIGEST_STRING_LENGTH);
    } else {
        rsChecksum[0] = '\0';
    }

    iniparser_freedict(ini);

    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
    @brief      It writes the RSed file checksum to metadata.
    @param      FTI_Conf        Configuration metadata.
    @param      FTI_Exec        Execution metadata.
    @param      FTI_Topo        Topology metadata.
    @param      FTI_Ckpt        Checkpoint metadata.
    @param      rank            global rank of the process
    @param      checksum        Pointer to the checksum.
    @return     integer         FTI_SCES if successful.

    This function should be executed only by one process per group. It
    writes the RSed checksum to the metadata file.

 **/
/*-------------------------------------------------------------------------*/
int FTI_WriteRSedChecksum(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
                            FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt,
                             int rank, char* checksum)
{
    // Fake call for FTI-FF. checksum is done for the datasets.
    if (FTI_Conf->ioMode == FTI_IO_FTIFF) {return FTI_SCES;}

    char str[FTI_BUFS], fileName[FTI_BUFS];

    //Calcuate which groupID rank belongs
    int sectorID = rank / (FTI_Topo->groupSize * FTI_Topo->nodeSize);
    int node = rank / FTI_Topo->nodeSize;
    int rankInGroup = node - (sectorID * FTI_Topo->groupSize);
    int groupID = rank % FTI_Topo->nodeSize;

    char* checksums = talloc(char, FTI_Topo->groupSize * MD5_DIGEST_STRING_LENGTH);
    MPI_Allgather(checksum, MD5_DIGEST_STRING_LENGTH, MPI_CHAR, checksums, MD5_DIGEST_STRING_LENGTH, MPI_CHAR, FTI_Exec->groupComm);

    //Only first process in group save RS checksum
    if (rankInGroup) {
        free(checksums);
        return FTI_SCES;
    }

    sprintf(fileName, "%s/sector%d-group%d.fti", FTI_Conf->mTmpDir, FTI_Topo->sectorID, groupID);
    dictionary* ini = iniparser_load(fileName);
    if (ini == NULL) {
        FTI_Print("Temporary metadata file could NOT be parsed", FTI_WARN);
        free(checksums);
        return FTI_NSCS;
    }
    // Add metadata to dictionary
    int i;
    for (i = 0; i < FTI_Topo->groupSize; i++) {
        char buf[FTI_BUFS];
        strncpy(buf, checksums + (i * MD5_DIGEST_STRING_LENGTH), MD5_DIGEST_STRING_LENGTH);
        sprintf(str, "%d:RSed_checksum", i);
        iniparser_set(ini, str, buf);
    }
    free(checksums);

    sprintf(str, "Recreating metadata file (%s)...", fileName);
    FTI_Print(str, FTI_DBUG);

    FILE* fd = fopen(fileName, "w");
    if (fd == NULL) {
        FTI_Print("Metadata file could NOT be opened.", FTI_WARN);

        iniparser_freedict(ini);

        return FTI_NSCS;
    }

    // Write metadata
    iniparser_dump_ini(ini, fd);

    if (fclose(fd) != 0) {
        FTI_Print("Metadata file could NOT be closed.", FTI_WARN);

        iniparser_freedict(ini);

        return FTI_NSCS;
    }

    iniparser_freedict(ini);

    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
    @brief      It gets the temporary metadata.
    @param      FTI_Conf        Configuration metadata.
    @param      FTI_Exec        Execution metadata.
    @param      FTI_Topo        Topology metadata.
    @param      FTI_Ckpt        Checkpoint metadata.
    @return     integer         FTI_SCES if successful.

    This function reads the temporary metadata file created during checkpointing and
    recovers the checkpoint file name, file size, partner file size and the size
    of the largest file in the group (for padding if necessary during decoding).

 **/
/*-------------------------------------------------------------------------*/
int FTI_LoadTmpMeta(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
                FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt)
{
    // no metadata files for FTI-FF
    if ( FTI_Conf->ioMode == FTI_IO_FTIFF ) { return FTI_SCES; }
    if (FTI_Topo->amIaHead) { //I am a head
        int j, biggestCkptID = 0; //Need to find biggest CkptID
        for (j = 1; j < FTI_Topo->nodeSize; j++) { //all body processes
            char metaFileName[FTI_BUFS], str[FTI_BUFS];
            sprintf(metaFileName, "%s/sector%d-group%d.fti", FTI_Conf->mTmpDir, FTI_Topo->sectorID, j);
            sprintf(str, "Getting FTI metadata file (%s)...", metaFileName);
            FTI_Print(str, FTI_DBUG);
            if (access(metaFileName, R_OK) == 0) {
                dictionary* ini = iniparser_load(metaFileName);
                if (ini == NULL) {
                    FTI_Print("Iniparser failed to parse the metadata file.", FTI_WARN);
                    return FTI_NSCS;
                }
                else {
                    FTI_Exec->meta[0].exists[j] = 1;

                    sprintf(str, "%d:Ckpt_file_name", FTI_Topo->groupRank);
                    char* ckptFileName = iniparser_getstring(ini, str, NULL);
                    snprintf(&FTI_Exec->meta[0].ckptFile[j * FTI_BUFS], FTI_BUFS, "%s", ckptFileName);

                    //update head's ckptID
                    sscanf(&FTI_Exec->meta[0].ckptFile[j * FTI_BUFS], "Ckpt%d", &FTI_Exec->ckptID);
                    if (FTI_Exec->ckptID < biggestCkptID) {
                        FTI_Exec->ckptID = biggestCkptID;
                    }

                    sprintf(str, "%d:Ckpt_file_size", FTI_Topo->groupRank);
                    FTI_Exec->meta[0].fs[j] = iniparser_getlint(ini, str, -1);

                    sprintf(str, "%d:Ckpt_file_size", (FTI_Topo->groupRank + FTI_Topo->groupSize - 1) % FTI_Topo->groupSize);
                    FTI_Exec->meta[0].pfs[j] = iniparser_getlint(ini, str, -1);

                    FTI_Exec->meta[0].maxFs[j] = iniparser_getlint(ini, "0:Ckpt_file_maxs", -1);

                    int k;
                    for (k = 0; k < FTI_BUFS; k++) {
                        sprintf(str, "%d:Var%d_id", FTI_Topo->groupRank, k);
                        int id = iniparser_getint(ini, str, -1);
                        if (id == -1) {
                            //No more variables
                            break;
                        }
                        //Variable exists
                        FTI_Exec->meta[0].varID[j * FTI_BUFS + k] = id;

                        sprintf(str, "%d:Var%d_size", FTI_Topo->groupRank, k);
                        FTI_Exec->meta[0].varSize[j * FTI_BUFS + k] = iniparser_getlint(ini, str, -1);
                    }
                    //Save number of variables in metadata
                    FTI_Exec->meta[0].nbVar[j] = k;

                    iniparser_freedict(ini);
                }
            }
            else {
                sprintf(str, "Temporary metadata do not exist for node process %d.", j);
                FTI_Print(str, FTI_WARN);
                return FTI_NSCS;
            }
        }
    }
    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
    @brief      It gets the metadata to recover the data after a failure.
    @param      FTI_Conf        Configuration metadata.
    @param      FTI_Exec        Execution metadata.
    @param      FTI_Topo        Topology metadata.
    @param      FTI_Ckpt        Checkpoint metadata.
    @return     integer         FTI_SCES if successful.

    This function reads the metadata file created during checkpointing and
    recovers the checkpoint file name, file size, partner file size and the size
    of the largest file in the group (for padding if necessary during decoding).

 **/
/*-------------------------------------------------------------------------*/
int FTI_LoadMeta(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
                FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt)
{
    // no metadata files for FTI-FF
    if ( FTI_Conf->ioMode == FTI_IO_FTIFF ) { return FTI_SCES; }
    if (!FTI_Topo->amIaHead) {
        int i;
        for (i = 0; i < 5; i++) { //for each level
            char metaFileName[FTI_BUFS], str[FTI_BUFS];
            if (i == 0) {
                sprintf(metaFileName, "%s/sector%d-group%d.fti", FTI_Conf->mTmpDir, FTI_Topo->sectorID, FTI_Topo->groupID);
            } else {
                sprintf(metaFileName, "%s/sector%d-group%d.fti", FTI_Ckpt[i].metaDir, FTI_Topo->sectorID, FTI_Topo->groupID);
            }
            sprintf(str, "Getting FTI metadata file (%s)...", metaFileName);
            FTI_Print(str, FTI_DBUG);
            if (access(metaFileName, R_OK) == 0) {
                dictionary* ini = iniparser_load(metaFileName);
                if (ini == NULL) {
                    FTI_Print("Iniparser failed to parse the metadata file.", FTI_WARN);
                    return FTI_NSCS;
                }
                else {
                    sprintf(str, "Meta for level %d exists.", i);
                    FTI_Print(str, FTI_DBUG);
                    FTI_Exec->meta[i].exists[0] = 1;

                    sprintf(str, "%d:Ckpt_file_name", FTI_Topo->groupRank);
                    char* ckptFileName = iniparser_getstring(ini, str, NULL);
                    snprintf(FTI_Exec->meta[i].ckptFile, FTI_BUFS, "%s", ckptFileName);

                    sprintf(str, "%d:Ckpt_file_size", FTI_Topo->groupRank);
                    FTI_Exec->meta[i].fs[0] = iniparser_getlint(ini, str, -1);

                    sprintf(str, "%d:Ckpt_file_size", (FTI_Topo->groupRank + FTI_Topo->groupSize - 1) % FTI_Topo->groupSize);
                    FTI_Exec->meta[i].pfs[0] = iniparser_getlint(ini, str, -1);

                    FTI_Exec->meta[i].maxFs[0] = iniparser_getlint(ini, "0:Ckpt_file_maxs", -1);

                    int k;
                    for (k = 0; k < FTI_BUFS; k++) {
                        sprintf(str, "%d:Var%d_id", FTI_Topo->groupRank, k);
                        int id = iniparser_getint(ini, str, -1);
                        if (id == -1) {
                            //No more variables
                            break;
                        }
                        //Variable exists
                        FTI_Exec->meta[i].varID[k] = id;

                        sprintf(str, "%d:Var%d_size", FTI_Topo->groupRank, k);
                        FTI_Exec->meta[i].varSize[k] = iniparser_getlint(ini, str, -1);
                    }
                    //Save number of variables in metadata
                    FTI_Exec->meta[i].nbVar[0] = k;

                    iniparser_freedict(ini);
                }
            }
        }
    }
    else { //I am a head
        int biggestCkptID = 0;
        int i;
        for (i = 0; i < 5; i++) {        //for each level
            int j;
            for (j = 1; j < FTI_Topo->nodeSize; j++) { //for all body processes
                dictionary* ini;
                char metaFileName[FTI_BUFS], str[FTI_BUFS];
                if (i == 0) {
                    sprintf(metaFileName, "%s/sector%d-group%d.fti", FTI_Conf->mTmpDir, FTI_Topo->sectorID, j);
                } else {
                    sprintf(metaFileName, "%s/sector%d-group%d.fti", FTI_Ckpt[i].metaDir, FTI_Topo->sectorID, j);
                }
                sprintf(str, "Getting FTI metadata file (%s)...", metaFileName);
                FTI_Print(str, FTI_DBUG);
                if (access(metaFileName, R_OK) == 0) {
                    ini = iniparser_load(metaFileName);
                    if (ini == NULL) {
                        FTI_Print("Iniparser failed to parse the metadata file.", FTI_WARN);
                        return FTI_NSCS;
                    }
                    else {
                        sprintf(str, "Meta for level %d exists.", i);
                        FTI_Print(str, FTI_DBUG);
                        FTI_Exec->meta[i].exists[j] = 1;

                        sprintf(str, "%d:Ckpt_file_name", FTI_Topo->groupRank);
                        char* ckptFileName = iniparser_getstring(ini, str, NULL);
                        snprintf(&FTI_Exec->meta[i].ckptFile[j * FTI_BUFS], FTI_BUFS, "%s", ckptFileName);

                        //update heads ckptID
                        sscanf(&FTI_Exec->meta[i].ckptFile[j * FTI_BUFS], "Ckpt%d", &FTI_Exec->ckptID);
                        if (FTI_Exec->ckptID < biggestCkptID) {
                            FTI_Exec->ckptID = biggestCkptID;
                        }

                        sprintf(str, "%d:Ckpt_file_size", FTI_Topo->groupRank);
                        FTI_Exec->meta[i].fs[j] = iniparser_getlint(ini, str, -1);

                        sprintf(str, "%d:Ckpt_file_size", (FTI_Topo->groupRank + FTI_Topo->groupSize - 1) % FTI_Topo->groupSize);
                        FTI_Exec->meta[i].pfs[j] = iniparser_getlint(ini, str, -1);

                        FTI_Exec->meta[i].maxFs[j] = iniparser_getlint(ini, "0:Ckpt_file_maxs", -1);
                        int k;
                        for (k = 0; k < FTI_BUFS; k++) {
                            sprintf(str, "%d:Var%d_id", FTI_Topo->groupRank, k);
                            int id = iniparser_getint(ini, str, -1);
                            if (id == -1) {
                                //No more variables
                                break;
                            }
                            //Variable exists
                            FTI_Exec->meta[i].varID[j * FTI_BUFS + k] = id;

                            sprintf(str, "%d:Var%d_size", FTI_Topo->groupRank, k);
                            FTI_Exec->meta[i].varSize[j * FTI_BUFS + k] = iniparser_getlint(ini, str, -1);
                        }
                        //Save number of variables in metadata
                        FTI_Exec->meta[i].nbVar[j] = k;

                        iniparser_freedict(ini);
                    }
                }
            }
        }
    }
    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
    @brief      It writes the metadata to recover the data after a failure.
    @param      FTI_Conf        Configuration metadata.
    @param      FTI_Exec        Execution metadata.
    @param      FTI_Topo        Topology metadata.
    @param      fs              Pointer to the list of checkpoint sizes.
    @param      mfs             The maximum checkpoint file size.
    @param      fnl             Pointer to the list of checkpoint names.
    @param      checksums       Checksums array.
    @param      allVarIDs       IDs of vars from all processes in group.
    @param      allVarSizes     Sizes of vars from all processes in group.
    @return     integer         FTI_SCES if successful.

    This function should be executed only by one process per group. It
    writes the metadata file used to recover in case of failure.

 **/
/*-------------------------------------------------------------------------*/
int FTI_WriteMetadata(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
                      FTIT_topology* FTI_Topo, long* fs, long mfs, char* fnl,
                      char* checksums, int* allVarIDs, long* allVarSizes)
{
    // no metadata files for FTI-FF
    if ( FTI_Conf->ioMode == FTI_IO_FTIFF ) { return FTI_SCES; }
    char str[FTI_BUFS], buf[FTI_BUFS];
    snprintf(buf, FTI_BUFS, "%s/Topology.fti", FTI_Conf->metadDir);
    sprintf(str, "Temporary load of topology file (%s)...", buf);
    FTI_Print(str, FTI_DBUG);

    // To bypass iniparser bug while empty dict.
    dictionary* ini = iniparser_load(buf);
    if (ini == NULL) {
        FTI_Print("Temporary topology file could NOT be parsed", FTI_WARN);
        return FTI_NSCS;
    }

    // Add metadata to dictionary
    int i;
    for (i = 0; i < FTI_Topo->groupSize; i++) {
        strncpy(buf, fnl + (i * FTI_BUFS), FTI_BUFS - 1);
        sprintf(str, "%d", i);
        iniparser_set(ini, str, NULL);
        sprintf(str, "%d:Ckpt_file_name", i);
        iniparser_set(ini, str, buf);
        sprintf(str, "%d:Ckpt_file_size", i);
        sprintf(buf, "%lu", fs[i]);
        iniparser_set(ini, str, buf);
        sprintf(str, "%d:Ckpt_file_maxs", i);
        sprintf(buf, "%lu", mfs);
        iniparser_set(ini, str, buf);
        strncpy(buf, checksums + (i * MD5_DIGEST_STRING_LENGTH), MD5_DIGEST_STRING_LENGTH);
        sprintf(str, "%d:Ckpt_checksum", i);
        iniparser_set(ini, str, buf);
        int j;
        for (j = 0; j < FTI_Exec->nbVar; j++) {
            //Save id of variable
            sprintf(str, "%d:Var%d_id", i, j);
            sprintf(buf, "%d", allVarIDs[i * FTI_Exec->nbVar + j]);
            iniparser_set(ini, str, buf);

            //Save size of variable
            sprintf(str, "%d:Var%d_size", i, j);
            sprintf(buf, "%ld", allVarSizes[i * FTI_Exec->nbVar + j]);
            iniparser_set(ini, str, buf);
        }
    }

    // Remove topology section
    iniparser_unset(ini, "topology");
    if (mkdir(FTI_Conf->mTmpDir, 0777) == -1) {
        if (errno != EEXIST) {
            FTI_Print("Cannot create directory", FTI_EROR);
        }
    }

    sprintf(buf, "%s/sector%d-group%d.fti", FTI_Conf->mTmpDir, FTI_Topo->sectorID, FTI_Topo->groupID);
    if (remove(buf) == -1) {
        if (errno != ENOENT) {
            FTI_Print("Cannot remove sector-group.fti", FTI_EROR);
        }
    }

    sprintf(str, "Creating metadata file (%s)...", buf);
    FTI_Print(str, FTI_DBUG);

    FILE* fd = fopen(buf, "w");
    if (fd == NULL) {
        FTI_Print("Metadata file could NOT be opened.", FTI_WARN);

        iniparser_freedict(ini);

        return FTI_NSCS;
    }

    // Write metadata
    iniparser_dump_ini(ini, fd);

    if (fclose(fd) != 0) {
        FTI_Print("Metadata file could NOT be closed.", FTI_WARN);

        iniparser_freedict(ini);

        return FTI_NSCS;
    }

    iniparser_freedict(ini);

    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
    @brief      It writes the metadata to recover the data after a failure.
    @param      FTI_Conf        Configuration metadata.
    @param      FTI_Exec        Execution metadata.
    @param      FTI_Topo        Topology metadata.
    @param      FTI_Ckpt        Checkpoint metadata.
    @param      FTI_Data        Dataset metadata.
    @return     integer         FTI_SCES if successful.

    This function gathers information about the checkpoint files in the
    group (name and sizes), and creates the metadata file used to recover in
    case of failure.

 **/
/*-------------------------------------------------------------------------*/
int FTI_CreateMetadata(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
                       FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt,
                       FTIT_dataset* FTI_Data)
{
    FTI_Exec->meta[0].fs[0] = FTI_Exec->ckptSize;
    long fs = FTI_Exec->meta[0].fs[0]; // Gather all the file sizes
    long fileSizes[FTI_BUFS];
    MPI_Allgather(&fs, 1, MPI_LONG, fileSizes, 1, MPI_LONG, FTI_Exec->groupComm);

    //update partner file size:
    if (FTI_Exec->ckptLvel == 2) {
        int ptnerGroupRank = (FTI_Topo->groupRank + FTI_Topo->groupSize - 1) % FTI_Topo->groupSize;
        FTI_Exec->meta[0].pfs[0] = fileSizes[ptnerGroupRank];
    }

    long mfs = 0; //Max file size in group
    int i;
    for (i = 0; i < FTI_Topo->groupSize; i++) {
        if (fileSizes[i] > mfs) {
            mfs = fileSizes[i]; // Search max. size
        }
    }
    // for FTIFF we need space for the meta data at the end
    //if ( FTI_Conf->ioMode == FTI_IO_FTIFF ) {
    //    mfs += sizeof( FTIFF_metaInfo );
    //}
    FTI_Exec->meta[0].maxFs[0] = mfs;
    char str[FTI_BUFS]; //For console output
    sprintf(str, "Max. file size in group %lu.", mfs);
    FTI_Print(str, FTI_DBUG);

    char* ckptFileNames;
    if (FTI_Topo->groupRank == 0) {
        ckptFileNames = talloc(char, FTI_Topo->groupSize * FTI_BUFS);
    }
    strcpy(str, FTI_Exec->meta[0].ckptFile); // Gather all the file names
    MPI_Gather(str, FTI_BUFS, MPI_CHAR, ckptFileNames, FTI_BUFS, MPI_CHAR, 0, FTI_Exec->groupComm);

    char* checksums;
    // checksum is computed while doing checkpoint in FTI-FF.
    if ( FTI_Conf->ioMode != FTI_IO_FTIFF ) {
        char checksum[MD5_DIGEST_STRING_LENGTH];
        FTI_Checksum(FTI_Exec, FTI_Data, FTI_Conf, checksum);
        if (FTI_Topo->groupRank == 0) {
            checksums = talloc(char, FTI_Topo->groupSize * MD5_DIGEST_STRING_LENGTH);
        }
        MPI_Gather(checksum, MD5_DIGEST_STRING_LENGTH, MPI_CHAR, checksums, MD5_DIGEST_STRING_LENGTH, MPI_CHAR, 0, FTI_Exec->groupComm);
    }

    //Every process has the same number of protected variables

    int* allVarIDs;
    long* allVarSizes;
    if (FTI_Topo->groupRank == 0) {
         allVarIDs = talloc(int, FTI_Topo->groupSize * FTI_Exec->nbVar);
         allVarSizes = talloc(long, FTI_Topo->groupSize * FTI_Exec->nbVar);
    }
    int* myVarIDs = talloc(int, FTI_Exec->nbVar);
    long* myVarSizes = talloc(long, FTI_Exec->nbVar);
    for (i = 0; i < FTI_Exec->nbVar; i++) {
        myVarIDs[i] = FTI_Data[i].id;
        myVarSizes[i] =  FTI_Data[i].size;
    }
    //Gather variables IDs
    MPI_Gather(myVarIDs, FTI_Exec->nbVar, MPI_INT, allVarIDs, FTI_Exec->nbVar, MPI_INT, 0, FTI_Exec->groupComm);
    //Gather variables sizes
    MPI_Gather(myVarSizes, FTI_Exec->nbVar, MPI_LONG, allVarSizes, FTI_Exec->nbVar, MPI_LONG, 0, FTI_Exec->groupComm);

    free(myVarIDs);
    free(myVarSizes);

    // meta data is written into ckpt file for FTI-FF
    if ( FTI_Conf->ioMode != FTI_IO_FTIFF ) {
        if (FTI_Topo->groupRank == 0) { // Only one process in the group create the metadata
            int res = FTI_Try(FTI_WriteMetadata(FTI_Conf, FTI_Exec, FTI_Topo, fileSizes, mfs,
                        ckptFileNames, checksums, allVarIDs, allVarSizes), "write the metadata.");
            free(allVarIDs);
            free(allVarSizes);
            free(ckptFileNames);
            free(checksums);
            if (res == FTI_NSCS) {
                return FTI_NSCS;
            }
        }
    }

    //Flush metadata in case postCkpt done inline
    FTI_Exec->meta[FTI_Exec->ckptLvel].fs[0] = FTI_Exec->meta[0].fs[0];
    FTI_Exec->meta[FTI_Exec->ckptLvel].pfs[0] = FTI_Exec->meta[0].pfs[0];
    FTI_Exec->meta[FTI_Exec->ckptLvel].maxFs[0] = FTI_Exec->meta[0].maxFs[0];
    strcpy(FTI_Exec->meta[FTI_Exec->ckptLvel].ckptFile, FTI_Exec->meta[0].ckptFile);
    for (i = 0; i < FTI_Exec->nbVar; i++) {
        FTI_Exec->meta[0].varID[i] = FTI_Data[i].id;
        FTI_Exec->meta[0].varSize[i] = FTI_Data[i].size;
    }

    return FTI_SCES;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      updates datablock structure for FTI File Format.
    @param      FTI_Conf        Configuration metadata.
    @param      FTI_Exec        Execution metadata.
    @param      FTI_Topo        Topology metadata.
    @param      FTI_Ckpt        Checkpoint metadata.
    @param      FTI_Data        Dataset metadata.
    @return     integer         FTI_SCES if successful.
    
    Updates information about the checkpoint file. Updates file pointers
    in the dbvar structures and updates the db structure.

 **/
/*-------------------------------------------------------------------------*/
int FTI_UpdateDatastructFTIFF( FTIT_execution* FTI_Exec, 
      FTIT_dataset* FTI_Data )
{

    int dbvar_idx, pvar_idx, num_edit_pvars = 0;
    int *editflags = (int*) calloc( FTI_Exec->nbVar, sizeof(int) ); // 0 -> nothing changed, 1 -> new pvar, 2 -> size changed
    FTIFF_dbvar *dbvars = NULL;
    int isnextdb;
    long offset = sizeof(FTIFF_metaInfo), chunksize;
    long *FTI_Data_oldsize, dbsize;
    
    // first call, init first datablock
    if(!FTI_Exec->firstdb) { // init file info
        dbsize = FTI_dbstructsize + sizeof(FTIFF_dbvar) * FTI_Exec->nbVar;
        FTIFF_db *dblock = (FTIFF_db*) malloc( sizeof(FTIFF_db) );
        dbvars = (FTIFF_dbvar*) malloc( sizeof(FTIFF_dbvar) * FTI_Exec->nbVar );
        dblock->previous = NULL;
        dblock->next = NULL;
        dblock->numvars = FTI_Exec->nbVar;
        dblock->dbvars = dbvars;
        for(dbvar_idx=0;dbvar_idx<dblock->numvars;dbvar_idx++) {
            dbvars[dbvar_idx].fptr = offset + dbsize;
            dbvars[dbvar_idx].dptr = 0;
            dbvars[dbvar_idx].id = FTI_Data[dbvar_idx].id;
            dbvars[dbvar_idx].idx = dbvar_idx;
            dbvars[dbvar_idx].chunksize = FTI_Data[dbvar_idx].size;
            dbsize += dbvars[dbvar_idx].chunksize; 
        }
        FTI_Exec->nbVarStored = FTI_Exec->nbVar;
        dblock->dbsize = dbsize;
        
        // set as first datablock
        FTI_Exec->firstdb = dblock;
        FTI_Exec->lastdb = dblock;
    
    } else {
       
        /*
         *  - check if protected variable is in file info
         *  - check if size has changed
         */
        
        FTI_Data_oldsize = (long*) calloc( FTI_Exec->nbVarStored, sizeof(long) );
        FTI_Exec->lastdb = FTI_Exec->firstdb;
        
        // iterate though datablock list
        do {
            isnextdb = 0;
            for(dbvar_idx=0;dbvar_idx<FTI_Exec->lastdb->numvars;dbvar_idx++) {
                for(pvar_idx=0;pvar_idx<FTI_Exec->nbVarStored;pvar_idx++) {
                    if(FTI_Exec->lastdb->dbvars[dbvar_idx].id == FTI_Data[pvar_idx].id) {
                        chunksize = FTI_Exec->lastdb->dbvars[dbvar_idx].chunksize;
                        FTI_Data_oldsize[pvar_idx] += chunksize;
                    }
                }
            }
            offset += FTI_Exec->lastdb->dbsize;
            if (FTI_Exec->lastdb->next) {
                FTI_Exec->lastdb = FTI_Exec->lastdb->next;
                isnextdb = 1;
            }
        } while( isnextdb );

        // check for new protected variables
        for(pvar_idx=FTI_Exec->nbVarStored;pvar_idx<FTI_Exec->nbVar;pvar_idx++) {
            editflags[pvar_idx] = 1;
            num_edit_pvars++;
        }
        
        // check if size changed
        for(pvar_idx=0;pvar_idx<FTI_Exec->nbVarStored;pvar_idx++) {
            if(FTI_Data_oldsize[pvar_idx] != FTI_Data[pvar_idx].size) {
                editflags[pvar_idx] = 2;
                num_edit_pvars++;
            }
        }
                
        // if size changed or we have new variables to protect, create new block. 
        dbsize = FTI_dbstructsize + sizeof(FTIFF_dbvar) * num_edit_pvars;
       
        int evar_idx = 0;
        if( num_edit_pvars ) {
            for(pvar_idx=0; pvar_idx<FTI_Exec->nbVar; pvar_idx++) {
                switch(editflags[pvar_idx]) {

                    case 1:
                        // add new protected variable in next datablock
                        dbvars = (FTIFF_dbvar*) realloc( dbvars, (evar_idx+1) * sizeof(FTIFF_dbvar) );
                        dbvars[evar_idx].fptr = offset + dbsize;
                        dbvars[evar_idx].dptr = 0;
                        dbvars[evar_idx].id = FTI_Data[pvar_idx].id;
                        dbvars[evar_idx].idx = pvar_idx;
                        dbvars[evar_idx].chunksize = FTI_Data[pvar_idx].size;
                        dbsize += dbvars[evar_idx].chunksize; 
                        evar_idx++;

                        break;

                    case 2:
                        
                        // create data chunk info
                        dbvars = (FTIFF_dbvar*) realloc( dbvars, (evar_idx+1) * sizeof(FTIFF_dbvar) );
                        dbvars[evar_idx].fptr = offset + dbsize;
                        dbvars[evar_idx].dptr = FTI_Data_oldsize[pvar_idx];
                        dbvars[evar_idx].id = FTI_Data[pvar_idx].id;
                        dbvars[evar_idx].idx = pvar_idx;
                        dbvars[evar_idx].chunksize = FTI_Data[pvar_idx].size - FTI_Data_oldsize[pvar_idx];
                        dbsize += dbvars[evar_idx].chunksize; 
                        evar_idx++;

                        break;

                }

            }

            FTIFF_db  *dblock = (FTIFF_db*) malloc( sizeof(FTIFF_db) );
            FTI_Exec->lastdb->next = dblock;
            dblock->previous = FTI_Exec->lastdb;
            dblock->next = NULL;
            dblock->numvars = num_edit_pvars;
            dblock->dbsize = dbsize;
            dblock->dbvars = dbvars;
            FTI_Exec->lastdb = dblock;
        
        }

        FTI_Exec->nbVarStored = FTI_Exec->nbVar;
        
        free(FTI_Data_oldsize);
    
    }

    free(editflags);
    return FTI_SCES;

}

/*-------------------------------------------------------------------------*/
/**
    @brief      Reads datablock structure for FTI File Format from ckpt file.
    @param      FTI_Exec        Execution metadata.
    @return     integer         FTI_SCES if successful.
    
    Builds meta data list from checkpoint file for the FTI File Format

 **/
/*-------------------------------------------------------------------------*/
int FTI_ReadDbFTIFF( FTIT_execution *FTI_Exec, FTIT_checkpoint* FTI_Ckpt ) 
{
    char fn[FTI_BUFS]; //Path to the checkpoint file
    char str[FTI_BUFS]; //For console output
	
    int varCnt = 0;

    //Recovering from local for L4 case in FTI_Recover
    if (FTI_Exec->ckptLvel == 4) {
        sprintf(fn, "%s/%s", FTI_Ckpt[1].dir, FTI_Exec->meta[1].ckptFile);
    }
    else {
        sprintf(fn, "%s/%s", FTI_Ckpt[FTI_Exec->ckptLvel].dir, FTI_Exec->meta[FTI_Exec->ckptLvel].ckptFile);
    }

    // get filesize
	struct stat st;
	stat(fn, &st);
	int ferr;
	char strerr[FTI_BUFS];

	// open checkpoint file for read only
	int fd = open( fn, O_RDONLY, 0 );
	if (fd == -1) {
		sprintf( strerr, "FTIFF: Updatedb - could not open '%s' for reading.", fn);
		FTI_Print(strerr, FTI_EROR);
		return FTI_NREC;
	}

	// map file into memory
	char* fmmap = (char*) mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (fmmap == MAP_FAILED) {
		sprintf( strerr, "FTIFF: Updatedb - could not map '%s' to memory.", fn);
		FTI_Print(strerr, FTI_EROR);
		close(fd);
		return FTI_NREC;
	}

	// file is mapped, we can close it.
	close(fd);

    // get file meta info
    memcpy( &(FTI_Exec->FTIFFMeta), fmmap, sizeof(FTIFF_metaInfo) );
	
    FTIFF_db *currentdb, *nextdb;
	FTIFF_dbvar *currentdbvar = NULL;
	int dbvar_idx, pvar_idx, dbcounter=0;

	long endoffile = sizeof(FTIFF_metaInfo); // space for timestamp 
    long mdoffset;

	int isnextdb;
	
	currentdb = (FTIFF_db*) malloc( sizeof(FTIFF_db) );
	if (!currentdb) {
		sprintf( strerr, "FTIFF: Updatedb - failed to allocate %ld bytes for 'currentdb'", sizeof(FTIFF_db));
		FTI_Print(strerr, FTI_EROR);
		return FTI_NREC;
	}

	FTI_Exec->firstdb = currentdb;
	FTI_Exec->firstdb->next = NULL;
	FTI_Exec->firstdb->previous = NULL;

	do {

		nextdb = (FTIFF_db*) malloc( sizeof(FTIFF_db) );
		if (!currentdb) {
			sprintf( strerr, "FTIFF: Updatedb - failed to allocate %ld bytes for 'nextdb'", sizeof(FTIFF_db));
			FTI_Print(strerr, FTI_EROR);
			return FTI_NREC;
		}

		isnextdb = 0;

		mdoffset = endoffile;

		memcpy( &(currentdb->numvars), fmmap+mdoffset, sizeof(int) ); 
		mdoffset += sizeof(int);
		memcpy( &(currentdb->dbsize), fmmap+mdoffset, sizeof(long) );
		mdoffset += sizeof(long);

		sprintf(str, "FTIFF: Updatedb - dataBlock:%i, dbsize: %ld, numvars: %i.", 
				dbcounter, currentdb->dbsize, currentdb->numvars);
		FTI_Print(str, FTI_DBUG);

		currentdb->dbvars = (FTIFF_dbvar*) malloc( sizeof(FTIFF_dbvar) * currentdb->numvars );
		if (!currentdb) {
			sprintf( strerr, "FTIFF: Updatedb - failed to allocate %ld bytes for 'currentdb->dbvars'", sizeof(FTIFF_dbvar));
			FTI_Print(strerr, FTI_EROR);
			return FTI_NREC;
		}

		for(dbvar_idx=0;dbvar_idx<currentdb->numvars;dbvar_idx++) {

			currentdbvar = &(currentdb->dbvars[dbvar_idx]);

			memcpy( currentdbvar, fmmap+mdoffset, sizeof(FTIFF_dbvar) );
			mdoffset += sizeof(FTIFF_dbvar);
            
            if ( varCnt == 0 ) { 
                varCnt++;
                FTI_Exec->meta[FTI_Exec->ckptLvel].varID[0] = currentdbvar->id;
                FTI_Exec->meta[FTI_Exec->ckptLvel].varSize[0] = currentdbvar->chunksize;
            } else {
                int i;
                for(i=0; i<varCnt; i++) {
                    if ( FTI_Exec->meta[FTI_Exec->ckptLvel].varID[i] == currentdbvar->id ) {
                        FTI_Exec->meta[FTI_Exec->ckptLvel].varSize[i] += currentdbvar->chunksize;
                        break;
                    }
                }
                if( i == varCnt ) {
                    varCnt++;
                    FTI_Exec->meta[FTI_Exec->ckptLvel].varID[varCnt-1] = currentdbvar->id;
                    FTI_Exec->meta[FTI_Exec->ckptLvel].varSize[varCnt-1] = currentdbvar->chunksize;
                }
            }

			// debug information
			sprintf(str, "FTIFF: Updatedb -  dataBlock:%i/dataBlockVar%i id: %i, idx: %i"
					", destptr: %ld, fptr: %ld, chunksize: %ld.",
					dbcounter, dbvar_idx,  
					currentdbvar->id, currentdbvar->idx, currentdbvar->dptr,
					currentdbvar->fptr, currentdbvar->chunksize);
			FTI_Print(str, FTI_DBUG);

		}

		endoffile += currentdb->dbsize;

		if ( endoffile < FTI_Exec->FTIFFMeta.ckptSize ) {
			memcpy( nextdb, fmmap+endoffile, FTI_dbstructsize );
			currentdb->next = nextdb;
			nextdb->previous = currentdb;
			currentdb = nextdb;
			isnextdb = 1;
		}

		dbcounter++;

	} while( isnextdb );

    FTI_Exec->meta[FTI_Exec->ckptLvel].nbVar[0] = varCnt;
	
    FTI_Exec->lastdb = currentdb;
	FTI_Exec->lastdb->next = NULL;

	// unmap memory
	if ( munmap( fmmap, st.st_size ) == -1 ) {
		FTI_Print("FTIFF: Updatedb - unable to unmap memory", FTI_WARN);
	}

}
