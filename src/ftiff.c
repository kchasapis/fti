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
 *  @author Kai Keller (kellekai@gmx.de)
 *  @file   ftiff.c
 *  @date   October, 2017
 *  @brief  Functions for the FTI File Format (FTI-FF).
 */

#include "interface.h"

/**  

  +-------------------------------------------------------------------------+
  |   STATIC TYPE DECLARATIONS                                              |
  +-------------------------------------------------------------------------+

 **/

MPI_Datatype FTIFF_MpiTypes[FTIFF_NUM_MPI_TYPES];

/**

  +-------------------------------------------------------------------------+
  |   FUNCTION DEFINITIONS                                                  |
  +-------------------------------------------------------------------------+

 **/

/*-------------------------------------------------------------------------*/
/**
  @brief      Initializes the derived MPI data types used for FTI-FF
 **/
/*-------------------------------------------------------------------------*/
void FTIFF_InitMpiTypes() 
{

    MPI_Aint lb, extent;
    FTIFF_MPITypeInfo MPITypeInfo[FTIFF_NUM_MPI_TYPES];

    // define MPI datatypes

    // headInfo
    MBR_CNT( headInfo ) =  6;
    MBR_BLK_LEN( headInfo ) = { 1, 1, FTI_BUFS, 1, 1, 1 };
    MBR_TYPES( headInfo ) = { MPI_INT, MPI_INT, MPI_CHAR, MPI_LONG, MPI_LONG, MPI_LONG };
    MBR_DISP( headInfo ) = {  
        offsetof( FTIFF_headInfo, exists), 
        offsetof( FTIFF_headInfo, nbVar), 
        offsetof( FTIFF_headInfo, ckptFile), 
        offsetof( FTIFF_headInfo, maxFs), 
        offsetof( FTIFF_headInfo, fs), 
        offsetof( FTIFF_headInfo, pfs) 
    };
    MPITypeInfo[FTIFF_HEAD_INFO].mbrCnt = headInfo_mbrCnt;
    MPITypeInfo[FTIFF_HEAD_INFO].mbrBlkLen = headInfo_mbrBlkLen;
    MPITypeInfo[FTIFF_HEAD_INFO].mbrTypes = headInfo_mbrTypes;
    MPITypeInfo[FTIFF_HEAD_INFO].mbrDisp = headInfo_mbrDisp;

    // L2Info
    MBR_CNT( L2Info ) =  6;
    MBR_BLK_LEN( L2Info ) = { 1, 1, 1, 1, 1, 1 };
    MBR_TYPES( L2Info ) = { MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_LONG, MPI_LONG };
    MBR_DISP( L2Info ) = {  
        offsetof( FTIFF_L2Info, FileExists), 
        offsetof( FTIFF_L2Info, CopyExists), 
        offsetof( FTIFF_L2Info, ckptID), 
        offsetof( FTIFF_L2Info, rightIdx), 
        offsetof( FTIFF_L2Info, fs), 
        offsetof( FTIFF_L2Info, pfs), 
    };
    MPITypeInfo[FTIFF_L2_INFO].mbrCnt = L2Info_mbrCnt;
    MPITypeInfo[FTIFF_L2_INFO].mbrBlkLen = L2Info_mbrBlkLen;
    MPITypeInfo[FTIFF_L2_INFO].mbrTypes = L2Info_mbrTypes;
    MPITypeInfo[FTIFF_L2_INFO].mbrDisp = L2Info_mbrDisp;

    // L3Info
    MBR_CNT( L3Info ) =  5;
    MBR_BLK_LEN( L3Info ) = { 1, 1, 1, 1, 1 };
    MBR_TYPES( L3Info ) = { MPI_INT, MPI_INT, MPI_INT, MPI_LONG, MPI_LONG };
    MBR_DISP( L3Info ) = {  
        offsetof( FTIFF_L3Info, FileExists), 
        offsetof( FTIFF_L3Info, RSFileExists), 
        offsetof( FTIFF_L3Info, ckptID), 
        offsetof( FTIFF_L3Info, fs), 
        offsetof( FTIFF_L3Info, RSfs), 
    };
    MPITypeInfo[FTIFF_L3_INFO].mbrCnt = L3Info_mbrCnt;
    MPITypeInfo[FTIFF_L3_INFO].mbrBlkLen = L3Info_mbrBlkLen;
    MPITypeInfo[FTIFF_L3_INFO].mbrTypes = L3Info_mbrTypes;
    MPITypeInfo[FTIFF_L3_INFO].mbrDisp = L3Info_mbrDisp;

    // commit MPI types
    int i;
    for(i=0; i<FTIFF_NUM_MPI_TYPES; i++) {
        MPI_Type_create_struct( 
                MPITypeInfo[i].mbrCnt, 
                MPITypeInfo[i].mbrBlkLen, 
                MPITypeInfo[i].mbrDisp, 
                MPITypeInfo[i].mbrTypes, 
                &MPITypeInfo[i].raw );
        MPI_Type_get_extent( 
                MPITypeInfo[i].raw, 
                &lb, 
                &extent );
        MPI_Type_create_resized( 
                MPITypeInfo[i].raw, 
                lb, 
                extent, 
                &FTIFF_MpiTypes[i]);
        MPI_Type_commit( &FTIFF_MpiTypes[i] );
    }


}

/*-------------------------------------------------------------------------*/
/**
  @brief      Reads datablock structure for FTI File Format from ckpt file.
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Ckpt        Checkpoint metadata.
  @return     integer         FTI_SCES if successful.

  Builds meta data list from checkpoint file for the FTI File Format

 **/
/*-------------------------------------------------------------------------*/
int FTIFF_ReadDbFTIFF( FTIT_execution *FTI_Exec, FTIT_checkpoint* FTI_Ckpt ) 
{
    char fn[FTI_BUFS]; //Path to the checkpoint file
    char str[FTI_BUFS]; //For console output

    int varCnt = 0;

    //Recovering from local for L4 case in FTI_Recover
    if (FTI_Exec->ckptLvel == 4) {
        snprintf(fn, FTI_BUFS, "%s/%s", FTI_Ckpt[1].dir, FTI_Exec->meta[1].ckptFile);
    }
    else {
        snprintf(fn, FTI_BUFS, "%s/%s", FTI_Ckpt[FTI_Exec->ckptLvel].dir, FTI_Exec->meta[FTI_Exec->ckptLvel].ckptFile);
    }

    // get filesize
    struct stat st;
    stat(fn, &st);
    char strerr[FTI_BUFS];

    // open checkpoint file for read only
    int fd = open( fn, O_RDONLY, 0 );
    if (fd == -1) {
        snprintf( strerr, FTI_BUFS, "FTI-FF: Updatedb - could not open '%s' for reading.", fn);
        FTI_Print(strerr, FTI_EROR);
        return FTI_NSCS;
    }

    // map file into memory
    char* fmmap = (char*) mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fmmap == MAP_FAILED) {
        snprintf( strerr, FTI_BUFS, "FTI-FF: Updatedb - could not map '%s' to memory.", fn);
        FTI_Print(strerr, FTI_EROR);
        close(fd);
        return FTI_NSCS;
    }

    // file is mapped, we can close it.
    close(fd);

    // get file meta info
    memcpy( &(FTI_Exec->FTIFFMeta), fmmap, sizeof(FTIFF_metaInfo) );

    FTIFF_db *currentdb, *nextdb;
    FTIFF_dbvar *currentdbvar = NULL;
    int dbvar_idx, dbcounter=0;

    long endoffile = sizeof(FTIFF_metaInfo); // space for timestamp 
    long mdoffset;

    int isnextdb;

    currentdb = (FTIFF_db*) malloc( sizeof(FTIFF_db) );
    if (!currentdb) {
        snprintf( strerr, FTI_BUFS, "FTI-FF: Updatedb - failed to allocate %ld bytes for 'currentdb'", sizeof(FTIFF_db));
        FTI_Print(strerr, FTI_EROR);
        return FTI_NSCS;
    }

    FTI_Exec->firstdb = currentdb;
    FTI_Exec->firstdb->next = NULL;
    FTI_Exec->firstdb->previous = NULL;

    do {

        nextdb = (FTIFF_db*) malloc( sizeof(FTIFF_db) );
        if (!currentdb) {
            snprintf( strerr, FTI_BUFS, "FTI-FF: Updatedb - failed to allocate %ld bytes for 'nextdb'", sizeof(FTIFF_db));
            FTI_Print(strerr, FTI_EROR);
            return FTI_NSCS;
        }

        isnextdb = 0;

        mdoffset = endoffile;

        memcpy( &(currentdb->numvars), fmmap+mdoffset, sizeof(int) ); 
        mdoffset += sizeof(int);
        memcpy( &(currentdb->dbsize), fmmap+mdoffset, sizeof(long) );
        mdoffset += sizeof(long);

        snprintf(str, FTI_BUFS, "FTI-FF: Updatedb - dataBlock:%i, dbsize: %ld, numvars: %i.", 
                dbcounter, currentdb->dbsize, currentdb->numvars);
        FTI_Print(str, FTI_DBUG);

        currentdb->dbvars = (FTIFF_dbvar*) malloc( sizeof(FTIFF_dbvar) * currentdb->numvars );
        if (!currentdb) {
            snprintf( strerr, FTI_BUFS, "FTI-FF: Updatedb - failed to allocate %ld bytes for 'currentdb->dbvars'", sizeof(FTIFF_dbvar));
            FTI_Print(strerr, FTI_EROR);
            return FTI_NSCS;
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
            snprintf(str, FTI_BUFS, "FTI-FF: Updatedb -  dataBlock:%i/dataBlockVar%i id: %i, idx: %i"
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
        FTI_Print("FTI-FF: Updatedb - unable to unmap memory", FTI_WARN);
    }

    return FTI_SCES;

}

/*-------------------------------------------------------------------------*/
/**
  @brief      updates datablock structure for FTI File Format.
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Data        Dataset metadata.
  @return     integer         FTI_SCES if successful.

  Updates information about the checkpoint file. Updates file pointers
  in the dbvar structures and updates the db structure.

 **/
/*-------------------------------------------------------------------------*/
int FTIFF_UpdateDatastructFTIFF( FTIT_execution* FTI_Exec, 
        FTIT_dataset* FTI_Data )
{

    if( FTI_Exec->nbVar == 0 ) {
        FTI_Print("FTI-FF - UpdateDatastructFTIFF: No protected Variables, discarding checkpoint!", FTI_WARN);
        return FTI_NSCS;
    }

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
  @brief      It calculates checksum of a checkpoint file for FTI-FF.
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Data        Dataset metadata.
  @param      checksum        Checksum that is calculated.
  @return     integer         FTI_SCES if successful.

  This function calculates checksum of the checkpoint file based on
  MD5 algorithm and saves it in checksum for a checkpointfile taken in the
  FTI File Format.

 **/
/*-------------------------------------------------------------------------*/
int FTIFF_Checksum(FTIT_execution* FTI_Exec, FTIT_dataset* FTI_Data, char* checksum)
{       

    if( !FTI_Exec->firstdb ){
        FTI_Print("FTI-FF - Checksum: No ckpt meta information, Cannot compute checksum!", FTI_WARN);
        return FTI_NSCS;
    }

    MD5_CTX mdContext;
    MD5_Init (&mdContext);

    FTIFF_db *currentdb = FTI_Exec->firstdb;
    FTIFF_dbvar *currentdbvar = NULL;
    char *dptr;
    int dbvar_idx, dbcounter=0;

    int isnextdb;

    do {

        isnextdb = 0;

        MD5_Update (&mdContext, &(currentdb->numvars), sizeof(int));
        MD5_Update (&mdContext, &(currentdb->dbsize), sizeof(long));

        for(dbvar_idx=0;dbvar_idx<currentdb->numvars;dbvar_idx++) {

            currentdbvar = &(currentdb->dbvars[dbvar_idx]);
            MD5_Update (&mdContext, currentdbvar, sizeof(FTIFF_dbvar));

        }

        for(dbvar_idx=0;dbvar_idx<currentdb->numvars;dbvar_idx++) {

            currentdbvar = &(currentdb->dbvars[dbvar_idx]);
            dptr = (char*)(FTI_Data[currentdbvar->idx].ptr) + currentdb->dbvars[dbvar_idx].dptr;
            MD5_Update (&mdContext, dptr, currentdbvar->chunksize);

        }

        if (currentdb->next) {
            currentdb = currentdb->next;
            isnextdb = 1;
        }

        dbcounter++;

    } while( isnextdb );

    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final (hash, &mdContext);
    int ii = 0, i;
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&checksum[ii], "%02x", hash[i]);
        ii += 2;
    }

    return FTI_SCES;

}

/*-------------------------------------------------------------------------*/
/**
  @brief      Writes ckpt to local/PFS using FTIFF.
  @param      FTI_Conf        Configuration metadata.
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Topo        Topology metadata.
  @param      FTI_Ckpt        Checkpoint metadata.
  @param      FTI_Data        Dataset metadata.
  @return     integer         FTI_SCES if successful.

  FTI-FF structure:
  =================

  +------+---------+-------------+       +------+---------+-------------+
  |      |         |             |       |      |         |             |
  | db 1 | dbvar 1 | ckpt data 1 | . . . | db n | dbvar n | ckpt data n |
  |      |         |             |       |      |         |             |
  +------+---------+-------------+       +------+---------+-------------+

 **/
/*-------------------------------------------------------------------------*/
int FTIFF_WriteFTIFF(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
        FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt,
        FTIT_dataset* FTI_Data)
{ 
    FTI_Print("I/O mode: FTI File Format.", FTI_DBUG);

    // Update the meta data information -> FTIT_db and FTIT_dbvar
    if( FTIFF_UpdateDatastructFTIFF( FTI_Exec, FTI_Data ) != FTI_SCES ) {
        return FTI_NSCS;
    }

    // check if metadata exists
    if(!FTI_Exec->firstdb) {
        FTI_Print("No data structure found to write data to file. Discarding checkpoint.", FTI_WARN);
        return FTI_NSCS;
    }

    char str[FTI_BUFS], fn[FTI_BUFS], fnr[FTI_BUFS];

    //If inline L4 save directly to global directory
    int level = FTI_Exec->ckptLvel;
    if (level == 4 && FTI_Ckpt[4].isInline) { 
        snprintf(fn, FTI_BUFS, "%s/%s", FTI_Conf->gTmpDir, FTI_Exec->meta[0].ckptFile);
    }
    else {
        if( FTI_Exec->hasCkpt && FTI_Conf->enableDiffCkpt ) {
            snprintf(fn, FTI_BUFS, "%s/l1/%s", FTI_Conf->localDir, FTI_Exec->meta[0].currentCkptFile);
            snprintf(fnr, FTI_BUFS, "%s/%s", FTI_Conf->lTmpDir, FTI_Exec->meta[0].ckptFile);
        } else {
            snprintf(fn, FTI_BUFS, "%s/%s", FTI_Conf->lTmpDir, FTI_Exec->meta[0].ckptFile);
        }
    }

    FILE* fd;

    // If ckpt file does not exist -> open with wb+ (Truncate to zero length or create file for update.)
    if (access(fn,R_OK) != 0) {
        fd = fopen(fn, "wb+");
    } 
    // If file exists -> open with rb+ (Open file for update (reading and writing).)
    else {
        fd = fopen(fn, "rb+");
    }

    if (fd == NULL) {
        snprintf(str, FTI_BUFS, "FTI checkpoint file (%s) could not be opened.", fn);
        FTI_Print(str, FTI_EROR);

        return FTI_NSCS;
    }

    int writeFailed;

    // make sure that is never a null ptr. otherwise its to fix.
    assert(FTI_Exec->firstdb);
    FTIFF_db *currentdb = FTI_Exec->firstdb;
    FTIFF_dbvar *currentdbvar = NULL;
    char *dptr;
    int dbvar_idx, dbcounter=0;
    long mdoffset;
    long endoffile = sizeof(FTIFF_metaInfo); // offset metaInfo FTI-FF

    // MD5 context for checksum of data chunks
    MD5_CTX mdContext;

    // block size for fwrite buffer in file.
    long membs = 1024*1024*16; // 16 MB
    long cpybuf, cpynow, cpycnt, fptr;

    int isnextdb;

    // Write in file with FTI-FF
    do {

        writeFailed = 0;
        isnextdb = 0;

        mdoffset = endoffile;

        // write db - datablock meta data
        fseek( fd, mdoffset, SEEK_SET );
        writeFailed += ( fwrite( &(currentdb->numvars), sizeof(int), 1, fd ) == 1 ) ? 0 : 1;
        mdoffset += sizeof(int);
        fseek( fd, mdoffset, SEEK_SET );
        writeFailed += ( fwrite( &(currentdb->dbsize), sizeof(long), 1, fd ) == 1 ) ? 0 : 1;
        mdoffset += sizeof(long);

        // debug information
        snprintf(str, FTI_BUFS, "FTIFF: CKPT(id:%i), dataBlock:%i, dbsize: %ld, numvars: %i, write failed: %i", 
                FTI_Exec->ckptID, dbcounter, currentdb->dbsize, currentdb->numvars, writeFailed);
        FTI_Print(str, FTI_DBUG);

        // write dbvar - datablock variables meta data and 
        // ckpt data
        for(dbvar_idx=0;dbvar_idx<currentdb->numvars;dbvar_idx++) {

            currentdbvar = &(currentdb->dbvars[dbvar_idx]);

            clearerr(fd);
            errno = 0;

            // ***** JUST FOR TESTING *****
            //if( FTI_Topo->splitRank==0 ) {
            //    uintptr_t buffer_offset, buffer_size;
            //    while( FTI_ReceiveDiffChunk(currentdbvar->id, (FTI_ADDRVAL) dptr, (FTI_ADDRVAL) currentdbvar->chunksize, &buffer_offset, &buffer_size, FTI_Exec) ) {
            //        printf("ID: %i, Data-ptr: %p, offset: %p, size: %lu\n",
            //                currentdbvar->id,
            //                (void*) dptr,
            //                (void*) buffer_offset,
            //                buffer_size);

            //    }
            //}

            // get source and destination pointer
            dptr = (char*)(FTI_Data[currentdbvar->idx].ptr) + currentdb->dbvars[dbvar_idx].dptr;
            fptr = currentdbvar->fptr;
            uintptr_t chunk_addr, chunk_size, chunk_offset, base;
            
            uintptr_t diffSize = 0;

            MD5_Init( &mdContext );
            // write ckpt data
            while( FTI_ReceiveDiffChunk(currentdbvar->id, (FTI_ADDRVAL) dptr, (FTI_ADDRVAL) currentdbvar->chunksize, &chunk_addr, &chunk_size, FTI_Exec) ) {
                cpycnt = 0;
                chunk_offset = chunk_addr - (FTI_ADDRVAL) dptr;
                // add hash of unchanged data and advance data and file pointer
                MD5_Update( &mdContext, dptr, chunk_offset );
                dptr += chunk_offset;
                fptr += chunk_offset;

                while ( cpycnt < chunk_size ) {
                    cpybuf = chunk_size - cpycnt;
                    cpynow = ( cpybuf > membs ) ? membs : cpybuf;
                    cpycnt += cpynow;
                    fseek( fd, fptr, SEEK_SET );
                    diffSize += (fwrite( dptr, cpynow, 1, fd ))*cpynow;
                    // if error for writing the data, print error and exit to calling function.
                    if (ferror(fd)) {
                        int fwrite_errno = errno;
                        char error_msg[FTI_BUFS];
                        error_msg[0] = 0;
                        strerror_r(fwrite_errno, error_msg, FTI_BUFS);
                        snprintf(str, FTI_BUFS, "Dataset #%d could not be written: %s.", currentdbvar->id, error_msg);
                        FTI_Print(str, FTI_EROR);
                        fclose(fd);
                        return FTI_NSCS;
                    }
                    MD5_Update( &mdContext, dptr, cpynow );
                    dptr += cpynow;
                    fptr += cpynow;
                }
            }
            MD5_Final( currentdbvar->hash, &mdContext );

            char strinfo[FTI_BUFS];
            snprintf(strinfo, FTI_BUFS, "DIFF: written %lu bytes out of %lu bytes.", diffSize, currentdbvar->chunksize);
            FTI_Print(strinfo, FTI_INFO);

            // write datablock variables meta data
            fseek( fd, mdoffset, SEEK_SET );
            writeFailed += ( fwrite( currentdbvar, sizeof(FTIFF_dbvar), 1, fd ) == 1 ) ? 0 : 1;
            mdoffset += sizeof(FTIFF_dbvar);

            // debug information
            snprintf(str, FTI_BUFS, "FTIFF: CKPT(id:%i) dataBlock:%i/dataBlockVar%i id: %i, idx: %i"
                    ", dptr: %ld, fptr: %ld, chunksize: %ld, "
                    "base_ptr: 0x%" PRIxPTR " ptr_pos: 0x%" PRIxPTR " write failed: %i", 
                    FTI_Exec->ckptID, dbcounter, dbvar_idx,  
                    currentdbvar->id, currentdbvar->idx, currentdbvar->dptr,
                    currentdbvar->fptr, currentdbvar->chunksize,
                    (uintptr_t)FTI_Data[currentdbvar->idx].ptr, (uintptr_t)dptr, writeFailed);
            FTI_Print(str, FTI_DBUG);

        }

        endoffile += currentdb->dbsize;

        if (currentdb->next) {
            currentdb = currentdb->next;
            isnextdb = 1;
        }

        dbcounter++;

    } while( isnextdb );

    FTI_Exec->ckptSize = endoffile;

    // create checkpoint meta data
    FTIFF_CreateMetadata( FTI_Exec, FTI_Topo, FTI_Data, FTI_Conf );

    fseek( fd, 0, SEEK_SET );

    writeFailed += ( fwrite( &(FTI_Exec->FTIFFMeta), sizeof(FTIFF_metaInfo), 1, fd ) == 1 ) ? 0 : 1;

    fclose( fd );

    if (writeFailed) {
        snprintf(str, FTI_BUFS, "FTIFF: An error occured. Discarding checkpoint");
        FTI_Print(str, FTI_WARN);
        return FTI_NSCS;
    }
    if( FTI_Exec->hasCkpt && FTI_Conf->enableDiffCkpt ) {
        int indicator = rename(fn, fnr);
    }
    
    strncpy(FTI_Exec->meta[0].currentCkptFile, FTI_Exec->meta[0].ckptFile, FTI_BUFS);

    return FTI_SCES;

}

/*-------------------------------------------------------------------------*/
/**
  @brief      Assign meta data to runtime and file meta data types
  @param      FTI_Conf        Configuration metadata.
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Topo        Topology metadata.
  @param      FTI_Data        Dataset metadata.
  @return     integer         FTI_SCES if successful.

  This function gathers information about the checkpoint files in the
  group and stores it in the respective meta data types runtime and
  ckpt file.

 **/
/*-------------------------------------------------------------------------*/
int FTIFF_CreateMetadata( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
        FTIT_dataset* FTI_Data, FTIT_configuration* FTI_Conf )
{
    int i;

    // FTI_Exec->ckptSize has to be assigned after successful ckpt write and before this call!
    long fs = FTI_Exec->ckptSize;
    FTI_Exec->FTIFFMeta.ckptSize = fs;
    FTI_Exec->FTIFFMeta.fs = fs;

    // allgather not needed for L1 checkpoint
    if( (FTI_Exec->ckptLvel == 2) || (FTI_Exec->ckptLvel == 3) ) { 

        long fileSizes[FTI_BUFS], mfs = 0;
        MPI_Allgather(&fs, 1, MPI_LONG, fileSizes, 1, MPI_LONG, FTI_Exec->groupComm);
        int ptnerGroupRank, i;
        switch(FTI_Exec->ckptLvel) {

            //get partner file size:
            case 2:

                ptnerGroupRank = (FTI_Topo->groupRank + FTI_Topo->groupSize - 1) % FTI_Topo->groupSize;
                FTI_Exec->FTIFFMeta.ptFs = fileSizes[ptnerGroupRank];
                FTI_Exec->FTIFFMeta.maxFs = -1;
                break;

                //get max file size in group 
            case 3:
                for (i = 0; i < FTI_Topo->groupSize; i++) {
                    if (fileSizes[i] > mfs) {
                        mfs = fileSizes[i]; // Search max. size
                    }
                }

                FTI_Exec->FTIFFMeta.maxFs = mfs;
                FTI_Exec->FTIFFMeta.ptFs = -1;
        }     

    } else {

        FTI_Exec->FTIFFMeta.ptFs = -1;
        FTI_Exec->FTIFFMeta.maxFs = -1;

    }

    FTI_Exec->meta[0].fs[0] = FTI_Exec->FTIFFMeta.fs;
    FTI_Exec->meta[0].pfs[0] = FTI_Exec->FTIFFMeta.ptFs;
    FTI_Exec->meta[0].maxFs[0] = FTI_Exec->FTIFFMeta.maxFs;

    // write meta data and its hash
    struct timespec ntime;
    clock_gettime(CLOCK_REALTIME, &ntime);
    FTI_Exec->FTIFFMeta.timestamp = ntime.tv_sec*1000000000 + ntime.tv_nsec;

    char checksum[MD5_DIGEST_STRING_LENGTH];
    FTI_Checksum( FTI_Exec, FTI_Data, FTI_Conf, checksum );
    strncpy( FTI_Exec->FTIFFMeta.checksum, checksum, MD5_DIGEST_STRING_LENGTH );

    // create checksum of meta data
    FTIFF_GetHashMetaInfo( FTI_Exec->FTIFFMeta.myHash, &(FTI_Exec->FTIFFMeta) );

    //Flush metadata in case postCkpt done inline
    FTI_Exec->meta[FTI_Exec->ckptLvel].fs[0] = FTI_Exec->meta[0].fs[0];
    FTI_Exec->meta[FTI_Exec->ckptLvel].pfs[0] = FTI_Exec->meta[0].pfs[0];
    FTI_Exec->meta[FTI_Exec->ckptLvel].maxFs[0] = FTI_Exec->meta[0].maxFs[0];
    strncpy(FTI_Exec->meta[FTI_Exec->ckptLvel].ckptFile, FTI_Exec->meta[0].ckptFile, FTI_BUFS);
    for (i = 0; i < FTI_Exec->nbVar; i++) {
        FTI_Exec->meta[0].varID[i] = FTI_Data[i].id;
        FTI_Exec->meta[0].varSize[i] = FTI_Data[i].size;
    }

    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
  @brief      Recovers protected data to the variable pointers for FTI-FF
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Ckpt        Checkpoint metadata.
  @param      FTI_Data        Dataset metadata.
  @return     integer         FTI_SCES if successful.

  This function restores the data of the protected variables to the state
  of the last checkpoint. The function is called by the API function 
  'FTI_Recover'.

 **/
/*-------------------------------------------------------------------------*/
int FTIFF_Recover( FTIT_execution *FTI_Exec, FTIT_dataset *FTI_Data, FTIT_checkpoint *FTI_Ckpt ) 
{
    if (FTI_Exec->initSCES == 0) {
        FTI_Print("FTI is not initialized.", FTI_WARN);
        return FTI_NSCS;
    }
    if (FTI_Exec->initSCES == 2) {
        FTI_Print("No checkpoint files to make recovery.", FTI_WARN);
        return FTI_NSCS;
    }

    char fn[FTI_BUFS]; //Path to the checkpoint file
    char str[FTI_BUFS]; //For console output

    //Check if nubmer of protected variables matches
    if (FTI_Exec->nbVar != FTI_Exec->meta[FTI_Exec->ckptLvel].nbVar[0]) {
        snprintf(str, FTI_BUFS, "Checkpoint has %d protected variables, but FTI protects %d.",
                FTI_Exec->meta[FTI_Exec->ckptLvel].nbVar[0], FTI_Exec->nbVar);
        FTI_Print(str, FTI_WARN);
        return FTI_NREC;
    }
    //Check if sizes of protected variables matches
    int i;
    for (i = 0; i < FTI_Exec->nbVar; i++) {
        if (FTI_Data[i].size != FTI_Exec->meta[FTI_Exec->ckptLvel].varSize[i]) {
            snprintf(str, FTI_BUFS, "Cannot recover %ld bytes to protected variable (ID %d) size: %ld",
                    FTI_Exec->meta[FTI_Exec->ckptLvel].varSize[i], FTI_Exec->meta[FTI_Exec->ckptLvel].varID[i],
                    FTI_Data[i].size);
            FTI_Print(str, FTI_WARN);
            return FTI_NREC;
        }
    }
    //Recovering from local for L4 case in FTI_Recover
    if (FTI_Exec->ckptLvel == 4) {
        snprintf(fn, FTI_BUFS, "%s/%s", FTI_Ckpt[1].dir, FTI_Exec->meta[1].ckptFile);
    }
    else {
        snprintf(fn, FTI_BUFS, "%s/%s", FTI_Ckpt[FTI_Exec->ckptLvel].dir, FTI_Exec->meta[FTI_Exec->ckptLvel].ckptFile);
    }
    // get filesize
    struct stat st;
    stat(fn, &st);
    char strerr[FTI_BUFS];

    // block size for memcpy of pointer.
    long membs = 1024*1024*16; // 16 MB
    long cpybuf, cpynow, cpycnt;

    if (!FTI_Exec->firstdb) {
        FTI_Print( "FTI-FF: RecoveryGlobal - No db meta information. Nothing to recover.", FTI_WARN );
        return FTI_NREC;
    }

    // open checkpoint file for read only
    int fd = open( fn, O_RDONLY, 0 );
    if (fd == -1) {
        snprintf( strerr, FTI_BUFS, "FTI-FF: RecoveryGlobal - could not open '%s' for reading.", fn);
        FTI_Print(strerr, FTI_EROR);
        return FTI_NREC;
    }

    // map file into memory
    char* fmmap = (char*) mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fmmap == MAP_FAILED) {
        snprintf( strerr, FTI_BUFS, "FTI-FF: RecoveryGlobal - could not map '%s' to memory.", fn);
        FTI_Print(strerr, FTI_EROR);
        close(fd);
        return FTI_NREC;
    }

    // file is mapped, we can close it.
    close(fd);

    FTIFF_db *currentdb;
    FTIFF_dbvar *currentdbvar = NULL;
    char *destptr, *srcptr;
    int dbvar_idx, dbcounter=0;

    // MD5 context for checksum of data chunks
    MD5_CTX mdContext;
    unsigned char hash[MD5_DIGEST_LENGTH];

    int isnextdb;

    currentdb = FTI_Exec->firstdb;

    do {

        isnextdb = 0;

        for(dbvar_idx=0;dbvar_idx<currentdb->numvars;dbvar_idx++) {

            currentdbvar = &(currentdb->dbvars[dbvar_idx]);

            // get source and destination pointer
            destptr = (char*) FTI_Data[currentdbvar->idx].ptr + currentdbvar->dptr;
            srcptr = (char*) fmmap + currentdbvar->fptr;

            MD5_Init( &mdContext );
            cpycnt = 0;
            while ( cpycnt < currentdbvar->chunksize ) {
                cpybuf = currentdbvar->chunksize - cpycnt;
                cpynow = ( cpybuf > membs ) ? membs : cpybuf;
                cpycnt += cpynow;
                memcpy( destptr, srcptr, cpynow );
                MD5_Update( &mdContext, destptr, cpynow );
                destptr += cpynow;
                srcptr += cpynow;
            }

            // debug information
            snprintf(str, FTI_BUFS, "FTI-FF: RecoveryGlobal -  dataBlock:%i/dataBlockVar%i id: %i, idx: %i"
                    ", destptr: %ld, fptr: %ld, chunksize: %ld, "
                    "base_ptr: 0x%" PRIxPTR " ptr_pos: 0x%" PRIxPTR ".", 
                    dbcounter, dbvar_idx,  
                    currentdbvar->id, currentdbvar->idx, currentdbvar->dptr,
                    currentdbvar->fptr, currentdbvar->chunksize,
                    (uintptr_t)FTI_Data[currentdbvar->idx].ptr, (uintptr_t)destptr);
            FTI_Print(str, FTI_DBUG);

            MD5_Final( hash, &mdContext );

            //if ( memcmp( currentdbvar->hash, hash, MD5_DIGEST_LENGTH ) != 0 ) {
            //    snprintf( strerr, FTI_BUFS, "FTI-FF: RecoveryGlobal - dataset with id:%i has been corrupted! Discard recovery.", currentdbvar->id);
            //    FTI_Print(strerr, FTI_WARN);
            //    return FTI_NREC;
            //}

        }

        if (currentdb->next) {
            currentdb = currentdb->next;
            isnextdb = 1;
        }

        dbcounter++;

    } while( isnextdb );

    // unmap memory
    if ( munmap( fmmap, st.st_size ) == -1 ) {
        FTI_Print("FTIFF: RecoveryGlobal - unable to unmap memory", FTI_WARN);
    }

    FTI_Exec->reco = 0;

    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
  @brief      Recovers protected data to the variable pointer with id
  @param      id              Id of protected variable.
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Data        Dataset metadata.
  @param      FTI_Ckpt        Checkpoint metadata.
  @return     integer         FTI_SCES if successful.

  This function restores the data to the protected variable with given id 
  as it was checkpointed during the last checkpoint. 
  The function is called by the API function 'FTI_RecoverVar'.

 **/
/*-------------------------------------------------------------------------*/
int FTIFF_RecoverVar( int id, FTIT_execution *FTI_Exec, FTIT_dataset *FTI_Data, FTIT_checkpoint *FTI_Ckpt )
{
    if (FTI_Exec->initSCES == 0) {
        FTI_Print("FTI is not initialized.", FTI_WARN);
        return FTI_NSCS;
    }

    if(FTI_Exec->reco==0){
        /* This is not a restart: no actions performed */
        return FTI_SCES;
    }

    if (FTI_Exec->initSCES == 2) {
        FTI_Print("No checkpoint files to make recovery.", FTI_WARN);
        return FTI_NSCS;
    }

    //Check if sizes of protected variables matches
    int i;
    for (i = 0; i < FTI_Exec->nbVar; i++) {
        if (id == FTI_Exec->meta[FTI_Exec->ckptLvel].varID[i]) {
            if (FTI_Data[i].size != FTI_Exec->meta[FTI_Exec->ckptLvel].varSize[i]) {
                char str[FTI_BUFS];
                snprintf(str, FTI_BUFS, "Cannot recover %ld bytes to protected variable (ID %d) size: %ld",
                        FTI_Exec->meta[FTI_Exec->ckptLvel].varSize[i], FTI_Exec->meta[FTI_Exec->ckptLvel].varID[i],
                        FTI_Data[i].size);
                FTI_Print(str, FTI_WARN);
                return FTI_NREC;
            }
        }
    }

    char fn[FTI_BUFS]; //Path to the checkpoint file

    //Recovering from local for L4 case in FTI_Recover
    if (FTI_Exec->ckptLvel == 4) {
        snprintf(fn, FTI_BUFS, "%s/%s", FTI_Ckpt[1].dir, FTI_Exec->meta[1].ckptFile);
    }
    else {
        snprintf(fn, FTI_BUFS, "%s/%s", FTI_Ckpt[FTI_Exec->ckptLvel].dir, FTI_Exec->meta[FTI_Exec->ckptLvel].ckptFile);
    }

    char str[FTI_BUFS];

    snprintf(str, FTI_BUFS, "Trying to load FTI checkpoint file (%s)...", fn);
    FTI_Print(str, FTI_DBUG);

    // get filesize
    struct stat st;
    stat(fn, &st);
    char strerr[FTI_BUFS];

    // block size for memcpy of pointer.
    long membs = 1024*1024*16; // 16 MB
    long cpybuf, cpynow, cpycnt;

    if (!FTI_Exec->firstdb) {
        FTI_Print( "FTIFF: RecoveryLocal - No db meta information. Nothing to recover.", FTI_WARN );
        return FTI_NREC;
    }

    // open checkpoint file for read only
    int fd = open( fn, O_RDONLY, 0 );
    if (fd == -1) {
        snprintf( strerr, FTI_BUFS, "FTIFF: RecoveryLocal - could not open '%s' for reading.", fn);
        FTI_Print(strerr, FTI_EROR);
        return FTI_NREC;
    }

    // map file into memory
    char* fmmap = (char*) mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fmmap == MAP_FAILED) {
        snprintf( strerr, FTI_BUFS, "FTIFF: RecoveryLocal - could not map '%s' to memory.", fn);
        FTI_Print(strerr, FTI_EROR);
        close(fd);
        return FTI_NREC;
    }

    // file is mapped, we can close it.
    close(fd);

    FTIFF_db *currentdb;
    FTIFF_dbvar *currentdbvar = NULL;
    char *destptr, *srcptr;
    int dbvar_idx, dbcounter=0;

    // MD5 context for checksum of data chunks
    MD5_CTX mdContext;
    unsigned char hash[MD5_DIGEST_LENGTH];

    int isnextdb;

    currentdb = FTI_Exec->firstdb;

    do {

        isnextdb = 0;

        for(dbvar_idx=0;dbvar_idx<currentdb->numvars;dbvar_idx++) {

            currentdbvar = &(currentdb->dbvars[dbvar_idx]);

            if (currentdbvar->id == id) {
                // get source and destination pointer
                destptr = (char*) FTI_Data[currentdbvar->idx].ptr + currentdbvar->dptr;
                srcptr = (char*) fmmap + currentdbvar->fptr;

                MD5_Init( &mdContext );
                cpycnt = 0;
                while ( cpycnt < currentdbvar->chunksize ) {
                    cpybuf = currentdbvar->chunksize - cpycnt;
                    cpynow = ( cpybuf > membs ) ? membs : cpybuf;
                    cpycnt += cpynow;
                    memcpy( destptr, srcptr, cpynow );
                    MD5_Update( &mdContext, destptr, cpynow );
                    destptr += cpynow;
                    srcptr += cpynow;
                }

                // debug information
                snprintf(str, FTI_BUFS, "FTIFF: RecoveryLocal -  dataBlock:%i/dataBlockVar%i id: %i, idx: %i"
                        ", destptr: %ld, fptr: %ld, chunksize: %ld, "
                        "base_ptr: 0x%" PRIxPTR " ptr_pos: 0x%" PRIxPTR ".", 
                        dbcounter, dbvar_idx,  
                        currentdbvar->id, currentdbvar->idx, currentdbvar->dptr,
                        currentdbvar->fptr, currentdbvar->chunksize,
                        (uintptr_t)FTI_Data[currentdbvar->idx].ptr, (uintptr_t)destptr);
                FTI_Print(str, FTI_DBUG);

                MD5_Final( hash, &mdContext );

                if ( memcmp( currentdbvar->hash, hash, MD5_DIGEST_LENGTH ) != 0 ) {
                    snprintf( strerr, FTI_BUFS, "FTIFF: RecoveryLocal - dataset with id:%i has been corrupted! Discard recovery.", currentdbvar->id);
                    FTI_Print(strerr, FTI_WARN);
                    return FTI_NREC;
                }

            }

        }

        if (currentdb->next) {
            currentdb = currentdb->next;
            isnextdb = 1;
        }

        dbcounter++;

    } while( isnextdb );

    // unmap memory
    if ( munmap( fmmap, st.st_size ) == -1 ) {
        FTI_Print("FTIFF: RecoveryLocal - unable to unmap memory", FTI_WARN);
    }

    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
  @brief      Init of FTI-FF L1 recovery
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Topo        Topology metadata.
  @param      FTI_Ckpt        Checkpoint metadata.
  @return     integer         FTI_SCES if successful.

  This function initializes the L1 checkpoint recovery. It checks for 
  erasures and loads the required meta data. 
 **/
/*-------------------------------------------------------------------------*/
int FTIFF_CheckL1RecoverInit( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
        FTIT_checkpoint* FTI_Ckpt )
{
    char str[FTI_BUFS], tmpfn[FTI_BUFS];
    int fexist = 0, fileTarget, ckptID, fcount;
    struct dirent *entry = malloc(sizeof(struct dirent));
    struct stat ckptFS;
    FTIFF_metaInfo *FTIFFMeta = calloc( 1, sizeof(FTIFF_metaInfo) );
    MD5_CTX mdContext;
    DIR *L1CkptDir = opendir( FTI_Ckpt[1].dir );
    if(L1CkptDir) {
        while((entry = readdir(L1CkptDir)) != NULL) {
            if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) { 
                snprintf(str, FTI_BUFS, "FTI-FF: L1RecoveryInit - found file with name: %s", entry->d_name);
                FTI_Print(str, FTI_DBUG);
                sscanf(entry->d_name, "Ckpt%d-Rank%d.fti", &ckptID, &fileTarget );
                if( fileTarget == FTI_Topo->myRank ) {
                    snprintf(tmpfn, FTI_BUFS, "%s/%s", FTI_Ckpt[1].dir, entry->d_name);
                    int ferr = stat(tmpfn, &ckptFS);
                    if (!ferr && S_ISREG(ckptFS.st_mode) && ckptFS.st_size > sizeof(FTIFF_metaInfo) ) {
                        int fd = open(tmpfn, O_RDONLY);
                        lseek(fd, 0, SEEK_SET);
                        read( fd, FTIFFMeta, sizeof(FTIFF_metaInfo) );
                        unsigned char hash[MD5_DIGEST_LENGTH];
                        FTIFF_GetHashMetaInfo( hash, FTIFFMeta );
                        if ( memcmp( FTIFFMeta->myHash, hash, MD5_DIGEST_LENGTH ) == 0 ) {
                            long rcount = sizeof(FTIFF_metaInfo), toRead, diff;
                            int rbuffer;
                            char *buffer = malloc( CHUNK_SIZE );
                            MD5_Init (&mdContext);
                            while( rcount < FTIFFMeta->fs ) {
                                lseek( fd, rcount, SEEK_SET );
                                diff = FTIFFMeta->fs - rcount;
                                toRead = ( diff < CHUNK_SIZE ) ? diff : CHUNK_SIZE;
                                rbuffer = read( fd, buffer, toRead );
                                rcount += rbuffer;
                                MD5_Update (&mdContext, buffer, rbuffer);
                            }
                            unsigned char hash[MD5_DIGEST_LENGTH];
                            MD5_Final (hash, &mdContext);
                            int i;
                            char checksum[MD5_DIGEST_STRING_LENGTH];
                            int ii = 0;
                            for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
                                sprintf(&checksum[ii], "%02x", hash[i]);
                                ii += 2;
                            }
                            if ( strcmp( checksum, FTIFFMeta->checksum ) == 0 ) {
                                FTI_Exec->meta[1].fs[0] = ckptFS.st_size;    
                                FTI_Exec->ckptID = ckptID;
                                strncpy(FTI_Exec->meta[1].ckptFile, entry->d_name, NAME_MAX);
                                fexist = 1;
                            }
                        }
                        close(fd);
                        break;
                    }            
                }
            }
        }
    }
    MPI_Allreduce(&fexist, &fcount, 1, MPI_INT, MPI_SUM, FTI_COMM_WORLD);
    int fneeded = FTI_Topo->nbNodes*FTI_Topo->nbApprocs;
    int res = (fcount == fneeded) ? FTI_SCES : FTI_NSCS;
    closedir(L1CkptDir);
    return res;
}

/*-------------------------------------------------------------------------*/
/**
  @brief      Init of FTI-FF L2 recovery
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Topo        Topology metadata.
  @param      FTI_Ckpt        Checkpoint metadata.
  @param      exists          Array with info of erased files
  @return     integer         FTI_SCES if successful.

  This function initializes the L2 checkpoint recovery. It checks for 
  erasures and loads the required meta data. 
 **/
/*-------------------------------------------------------------------------*/
int FTIFF_CheckL2RecoverInit( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
        FTIT_checkpoint* FTI_Ckpt, int *exists)
{
    char dbgstr[FTI_BUFS];

    enum {
        LEFT_FILE,  // ckpt file of left partner (on left node)
        MY_FILE,    // my ckpt file (on my node)
        MY_COPY,    // copy of my ckpt file (on right node)
        LEFT_COPY   // copy of ckpt file of my left partner (on my node)
    };

    // determine app rank representation of group ranks left and right
    MPI_Group nodesGroup;
    MPI_Comm_group(FTI_Exec->groupComm, &nodesGroup);
    MPI_Group appProcsGroup;
    MPI_Comm_group(FTI_COMM_WORLD, &appProcsGroup);
    int baseRanks[] = { FTI_Topo->left, FTI_Topo->right };
    int projRanks[2];
    MPI_Group_translate_ranks( nodesGroup, 2, baseRanks, appProcsGroup, projRanks );
    int leftIdx = projRanks[0], rightIdx = projRanks[1];

    int appCommSize = FTI_Topo->nbNodes*FTI_Topo->nbApprocs;
    int fneeded = appCommSize;

    MPI_Group_free(&nodesGroup);
    MPI_Group_free(&appProcsGroup);

    FTIFF_L2Info *appProcsMetaInfo = calloc( appCommSize, sizeof(FTIFF_L2Info) );
    FTIFF_L2Info *myMetaInfo = calloc( 1, sizeof(FTIFF_L2Info) );

    myMetaInfo->rightIdx = rightIdx;

    MD5_CTX mdContext;

    char str[FTI_BUFS], tmpfn[FTI_BUFS];
    int fileTarget, ckptID = -1, fcount = 0, match;
    struct dirent *entry = malloc(sizeof(struct dirent));
    struct stat ckptFS;
    DIR *L2CkptDir = opendir( FTI_Ckpt[2].dir );

    FTIFF_metaInfo *FTIFFMeta = calloc( 1, sizeof(FTIFF_metaInfo) );
    if(L2CkptDir) {
        int tmpCkptID;
        while((entry = readdir(L2CkptDir)) != NULL) {
            if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) { 
                snprintf(str, FTI_BUFS, "FTI-FF: L2RecoveryInit - found file with name: %s", entry->d_name);
                FTI_Print(str, FTI_DBUG);
                tmpCkptID = ckptID;
                match = sscanf(entry->d_name, "Ckpt%d-Rank%d.fti", &ckptID, &fileTarget );
                if( match == 2 && fileTarget == FTI_Topo->myRank ) {
                    snprintf(tmpfn, FTI_BUFS, "%s/%s", FTI_Ckpt[2].dir, entry->d_name);
                    int ferr = stat(tmpfn, &ckptFS);
                    // check if regular file and of reasonable size (at least must contain meta info)
                    if ( !ferr && S_ISREG(ckptFS.st_mode) && ckptFS.st_size > sizeof(FTIFF_metaInfo) ) {
                        int fd = open(tmpfn, O_RDONLY);
                        lseek(fd, 0, SEEK_SET);
                        read( fd, FTIFFMeta, sizeof(FTIFF_metaInfo) );
                        unsigned char hash[MD5_DIGEST_LENGTH];
                        FTIFF_GetHashMetaInfo( hash, FTIFFMeta );
                        if ( memcmp( FTIFFMeta->myHash, hash, MD5_DIGEST_LENGTH ) == 0 ) {
                            long rcount = sizeof(FTIFF_metaInfo), toRead, diff;
                            int rbuffer;
                            char *buffer = malloc( CHUNK_SIZE );
                            MD5_Init (&mdContext);
                            while( rcount < FTIFFMeta->fs ) {
                                lseek( fd, rcount, SEEK_SET );
                                diff = FTIFFMeta->fs - rcount;
                                toRead = ( diff < CHUNK_SIZE ) ? diff : CHUNK_SIZE;
                                rbuffer = read( fd, buffer, toRead );
                                rcount += rbuffer;
                                MD5_Update (&mdContext, buffer, rbuffer);
                            }
                            unsigned char hash[MD5_DIGEST_LENGTH];
                            MD5_Final (hash, &mdContext);
                            int i;
                            char checksum[MD5_DIGEST_STRING_LENGTH];
                            int ii = 0;
                            for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
                                sprintf(&checksum[ii], "%02x", hash[i]);
                                ii += 2;
                            }
                            if ( strcmp( checksum, FTIFFMeta->checksum ) == 0 ) {
                                myMetaInfo->fs = FTIFFMeta->fs;    
                                myMetaInfo->ckptID = ckptID;    
                                myMetaInfo->FileExists = 1;
                            }
                        }
                        close(fd);
                    }            
                } else {
                    ckptID = tmpCkptID;
                }
                tmpCkptID = ckptID;
                match = sscanf(entry->d_name, "Ckpt%d-Pcof%d.fti", &ckptID, &fileTarget );
                if( match == 2 && fileTarget == FTI_Topo->myRank ) {
                    snprintf(tmpfn, FTI_BUFS, "%s/%s", FTI_Ckpt[2].dir, entry->d_name);
                    int ferr = stat(tmpfn, &ckptFS);
                    if (!ferr && S_ISREG(ckptFS.st_mode) && ckptFS.st_size > sizeof(FTIFF_metaInfo)) {
                        int fd = open(tmpfn, O_RDONLY);
                        lseek(fd, 0, SEEK_SET);
                        read( fd, FTIFFMeta, sizeof(FTIFF_metaInfo) );
                        unsigned char hash[MD5_DIGEST_LENGTH];
                        FTIFF_GetHashMetaInfo( hash, FTIFFMeta );
                        if ( memcmp( FTIFFMeta->myHash, hash, MD5_DIGEST_LENGTH ) == 0 ) {
                            long rcount = sizeof(FTIFF_metaInfo), toRead, diff;
                            int rbuffer;
                            char *buffer = malloc( CHUNK_SIZE );
                            MD5_Init (&mdContext);
                            while( rcount < FTIFFMeta->fs ) {
                                lseek( fd, rcount, SEEK_SET );
                                diff = FTIFFMeta->fs - rcount;
                                toRead = ( diff < CHUNK_SIZE ) ? diff : CHUNK_SIZE;
                                rbuffer = read( fd, buffer, toRead );
                                rcount += rbuffer;
                                MD5_Update (&mdContext, buffer, rbuffer);
                            }
                            unsigned char hash[MD5_DIGEST_LENGTH];
                            MD5_Final (hash, &mdContext);
                            int i;
                            char checksum[MD5_DIGEST_STRING_LENGTH];
                            int ii = 0;
                            for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
                                sprintf(&checksum[ii], "%02x", hash[i]);
                                ii += 2;
                            }
                            if ( strcmp( checksum, FTIFFMeta->checksum ) == 0 ) {
                                myMetaInfo->pfs = FTIFFMeta->fs;    
                                myMetaInfo->ckptID = ckptID;    
                                myMetaInfo->CopyExists = 1;
                            }
                        }
                        close(fd);
                    }            
                } else {
                    ckptID = tmpCkptID;
                }
            }
            if (myMetaInfo->FileExists && myMetaInfo->CopyExists) {
                break;
            }
        }
    }

    if(!(myMetaInfo->FileExists) && !(myMetaInfo->CopyExists)) {
        myMetaInfo->ckptID = -1;
    }

    // gather meta info
    MPI_Allgather( myMetaInfo, 1, FTIFF_MpiTypes[FTIFF_L2_INFO], appProcsMetaInfo, 1, FTIFF_MpiTypes[FTIFF_L2_INFO], FTI_COMM_WORLD);

    exists[LEFT_FILE] = appProcsMetaInfo[leftIdx].FileExists;
    exists[MY_FILE] = appProcsMetaInfo[FTI_Topo->splitRank].FileExists;
    exists[MY_COPY] = appProcsMetaInfo[rightIdx].CopyExists;
    exists[LEFT_COPY] = appProcsMetaInfo[FTI_Topo->splitRank].CopyExists;

    // debug Info
    snprintf(dbgstr, FTI_BUFS, "FTI-FF - L2Recovery::FileCheck - CkptFile: %i, CkptCopy: %i", 
            myMetaInfo->FileExists, myMetaInfo->CopyExists);
    FTI_Print(dbgstr, FTI_DBUG);

    // check if recovery possible
    int i, saneCkptID = 0;
    ckptID = 0;
    for(i=0; i<appCommSize; i++) { 
        fcount += ( appProcsMetaInfo[i].FileExists || appProcsMetaInfo[appProcsMetaInfo[i].rightIdx].CopyExists ) ? 1 : 0;
        if (appProcsMetaInfo[i].ckptID > 0) {
            saneCkptID++;
            ckptID += appProcsMetaInfo[i].ckptID;
        }
    }
    int res = (fcount == fneeded) ? FTI_SCES : FTI_NSCS;

    if (res == FTI_SCES) {
        FTI_Exec->ckptID = ckptID/saneCkptID;
        if (myMetaInfo->FileExists) {
            FTI_Exec->meta[2].fs[0] = myMetaInfo->fs;    
        } else {
            FTI_Exec->meta[2].fs[0] = appProcsMetaInfo[rightIdx].pfs;    
        }
        if (myMetaInfo->CopyExists) {
            FTI_Exec->meta[2].pfs[0] = myMetaInfo->pfs;    
        } else {
            FTI_Exec->meta[2].pfs[0] = appProcsMetaInfo[leftIdx].fs;    
        }
    }
    snprintf(dbgstr, FTI_BUFS, "FTI-FF: L2-Recovery - rank: %i, left: %i, right: %i, fs: %ld, pfs: %ld, ckptID: %i",
            FTI_Topo->myRank, leftIdx, rightIdx, FTI_Exec->meta[2].fs[0], FTI_Exec->meta[2].pfs[0], FTI_Exec->ckptID);
    FTI_Print(dbgstr, FTI_DBUG);

    snprintf(FTI_Exec->meta[2].ckptFile, FTI_BUFS, "Ckpt%d-Rank%d.fti", FTI_Exec->ckptID, FTI_Topo->myRank);

    closedir(L2CkptDir);
    free(appProcsMetaInfo);
    free(myMetaInfo);
    //free(entry);

    return res;
}

/*-------------------------------------------------------------------------*/
/**
  @brief      Init of FTI-FF L3 recovery
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Topo        Topology metadata.
  @param      FTI_Ckpt        Checkpoint metadata.
  @param      erased          Array with info of erased files
  @return     integer         FTI_SCES if successful.

  This function initializes the L3 checkpoint recovery. It checks for 
  erasures and loads the required meta data. 
 **/
/*-------------------------------------------------------------------------*/
int FTIFF_CheckL3RecoverInit( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
        FTIT_checkpoint* FTI_Ckpt, int* erased)
{

    FTIFF_L3Info *groupInfo = calloc( FTI_Topo->groupSize, sizeof(FTIFF_L3Info) );
    FTIFF_L3Info *myInfo = calloc( 1, sizeof(FTIFF_L3Info) );

    MD5_CTX mdContext;

    char str[FTI_BUFS], tmpfn[FTI_BUFS];
    int fileTarget, ckptID = -1, match;
    struct dirent *entry = malloc(sizeof(struct dirent));
    struct stat ckptFS;
    DIR *L3CkptDir = opendir( FTI_Ckpt[3].dir );

    FTIFF_metaInfo *FTIFFMeta = calloc( 1, sizeof(FTIFF_metaInfo) );
    if(L3CkptDir) {
        int tmpCkptID;
        while((entry = readdir(L3CkptDir)) != NULL) {
            if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) { 
                snprintf(str, FTI_BUFS, "FTI-FF: L3RecoveryInit - found file with name: %s", entry->d_name);
                FTI_Print(str, FTI_DBUG);
                tmpCkptID = ckptID;
                match = sscanf(entry->d_name, "Ckpt%d-Rank%d.fti", &ckptID, &fileTarget );
                if( match == 2 && fileTarget == FTI_Topo->myRank ) {
                    snprintf(tmpfn, FTI_BUFS, "%s/%s", FTI_Ckpt[3].dir, entry->d_name);
                    int ferr = stat(tmpfn, &ckptFS);
                    if (!ferr && S_ISREG(ckptFS.st_mode) && ckptFS.st_size > sizeof(FTIFF_metaInfo) ) {
                        int fd = open(tmpfn, O_RDONLY);
                        lseek(fd, 0, SEEK_SET);
                        read( fd, FTIFFMeta, sizeof(FTIFF_metaInfo) );
                        unsigned char hash[MD5_DIGEST_LENGTH];
                        FTIFF_GetHashMetaInfo( hash, FTIFFMeta );
                        if ( memcmp( FTIFFMeta->myHash, hash, MD5_DIGEST_LENGTH ) == 0 ) {
                            long rcount = sizeof(FTIFF_metaInfo) , toRead, diff;
                            int rbuffer;
                            char *buffer = malloc( CHUNK_SIZE );
                            MD5_Init (&mdContext);
                            while( rcount < FTIFFMeta->fs ) {
                                lseek( fd, rcount, SEEK_SET );
                                diff = FTIFFMeta->fs - rcount;
                                toRead = ( diff < CHUNK_SIZE ) ? diff : CHUNK_SIZE;
                                rbuffer = read( fd, buffer, toRead );
                                rcount += rbuffer;
                                MD5_Update (&mdContext, buffer, rbuffer);
                            }
                            unsigned char hash[MD5_DIGEST_LENGTH];
                            MD5_Final (hash, &mdContext);
                            int i;
                            char checksum[MD5_DIGEST_STRING_LENGTH];
                            int ii = 0;
                            for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
                                sprintf(&checksum[ii], "%02x", hash[i]);
                                ii += 2;
                            }
                            if ( strcmp( checksum, FTIFFMeta->checksum ) == 0 ) {
                                myInfo->fs = FTIFFMeta->fs;    
                                myInfo->ckptID = ckptID;    
                                myInfo->FileExists = 1;
                            }
                        }
                        close(fd);
                    }            
                } else {
                    ckptID = tmpCkptID;
                }
                tmpCkptID = ckptID;
                match = sscanf(entry->d_name, "Ckpt%d-RSed%d.fti", &ckptID, &fileTarget );
                if( match == 2 && fileTarget == FTI_Topo->myRank ) {
                    snprintf(tmpfn, FTI_BUFS, "%s/%s", FTI_Ckpt[3].dir, entry->d_name);
                    int ferr = stat(tmpfn, &ckptFS);
                    if (!ferr && S_ISREG(ckptFS.st_mode) && ckptFS.st_size > sizeof(FTIFF_metaInfo) ) {
                        int fd = open(tmpfn, O_RDONLY);
                        lseek(fd, -sizeof(FTIFF_metaInfo), SEEK_END);
                        read( fd, FTIFFMeta, sizeof(FTIFF_metaInfo) );
                        unsigned char hash[MD5_DIGEST_LENGTH];
                        FTIFF_GetHashMetaInfo( hash, FTIFFMeta );
                        if ( memcmp( FTIFFMeta->myHash, hash, MD5_DIGEST_LENGTH ) == 0 ) {
                            long rcount = 0, toRead, diff;
                            int rbuffer;
                            char *buffer = malloc( CHUNK_SIZE );
                            MD5_Init (&mdContext);
                            while( rcount < FTIFFMeta->fs ) {
                                lseek( fd, rcount, SEEK_SET );
                                diff = FTIFFMeta->fs - rcount;
                                toRead = ( diff < CHUNK_SIZE ) ? diff : CHUNK_SIZE;
                                rbuffer = read( fd, buffer, toRead );
                                rcount += rbuffer;
                                MD5_Update (&mdContext, buffer, rbuffer);
                            }
                            unsigned char hash[MD5_DIGEST_LENGTH];
                            MD5_Final (hash, &mdContext);
                            int i;
                            char checksum[MD5_DIGEST_STRING_LENGTH];
                            int ii = 0;
                            for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
                                sprintf(&checksum[ii], "%02x", hash[i]);
                                ii += 2;
                            }
                            if ( strcmp( checksum, FTIFFMeta->checksum ) == 0 ) {
                                myInfo->RSfs = FTIFFMeta->fs;    
                                myInfo->ckptID = ckptID;    
                                myInfo->RSFileExists = 1;
                            }
                        }
                        close(fd);
                    }            
                } else {
                    ckptID = tmpCkptID;
                }
            }
            if (myInfo->FileExists && myInfo->RSFileExists) {
                break;
            }
        }
    }

    if(!(myInfo->FileExists) && !(myInfo->RSFileExists)) {
        myInfo->ckptID = -1;
    }

    if(!(myInfo->RSFileExists)) {
        myInfo->RSfs = -1;
    }

    // gather meta info
    MPI_Allgather( myInfo, 1, FTIFF_MpiTypes[FTIFF_L3_INFO], groupInfo, 1, FTIFF_MpiTypes[FTIFF_L3_INFO], FTI_Exec->groupComm);

    // check if recovery possible
    int i, saneCkptID = 0, saneMaxFs = 0, erasures = 0;
    long maxFs = 0;
    ckptID = 0;
    for(i=0; i<FTI_Topo->groupSize; i++) { 
        erased[i]=!groupInfo[i].FileExists;
        erased[i+FTI_Topo->groupSize]=!groupInfo[i].RSFileExists;
        erasures += erased[i] + erased[i+FTI_Topo->groupSize];
        if (groupInfo[i].ckptID > 0) {
            saneCkptID++;
            ckptID += groupInfo[i].ckptID;
        }
        if (groupInfo[i].RSfs > 0) {
            saneMaxFs++;
            maxFs += groupInfo[i].RSfs;
        }
    }
    if( saneCkptID != 0 ) {
        FTI_Exec->ckptID = ckptID/saneCkptID;
    }
    if( saneMaxFs != 0 ) {
        FTI_Exec->meta[3].maxFs[0] = maxFs/saneMaxFs;
    }
    // for the case that all (and only) the encoded files are deleted
    if( saneMaxFs == 0 && !(erasures > FTI_Topo->groupSize) ) {
        MPI_Allreduce( &(FTIFFMeta->maxFs), FTI_Exec->meta[3].maxFs, 1, MPI_LONG, MPI_SUM, FTI_Exec->groupComm );
        FTI_Exec->meta[3].maxFs[0] /= FTI_Topo->groupSize;
    }

    FTI_Exec->meta[3].fs[0] = (myInfo->FileExists) ? myInfo->fs : 0;

    snprintf(FTI_Exec->meta[3].ckptFile, FTI_BUFS, "Ckpt%d-Rank%d.fti", FTI_Exec->ckptID, FTI_Topo->myRank);

    closedir(L3CkptDir);
    free(groupInfo);
    free(myInfo);
    //free(entry);

    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
  @brief      Init of FTI-FF L4 recovery
  @param      FTI_Exec        Execution metadata.
  @param      FTI_Topo        Topology metadata.
  @param      FTI_Ckpt        Checkpoint metadata.
  @param      checksum        Ckpt file checksum
  @return     integer         FTI_SCES if successful.

  This function initializes the L4 checkpoint recovery. It checks for 
  erasures and loads the required meta data. 
 **/
/*-------------------------------------------------------------------------*/
int FTIFF_CheckL4RecoverInit( FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo, 
        FTIT_checkpoint* FTI_Ckpt, char *checksum)
{
    char str[FTI_BUFS], tmpfn[FTI_BUFS];
    int fexist = 0, fileTarget, ckptID, fcount;
    struct dirent *entry = malloc(sizeof(struct dirent));
    struct stat ckptFS;
    DIR *L4CkptDir = opendir( FTI_Ckpt[4].dir );
    FTIFF_metaInfo *FTIFFMeta = calloc( 1, sizeof(FTIFF_metaInfo) );
    if(L4CkptDir) {
        while((entry = readdir(L4CkptDir)) != NULL) {
            if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) { 
                snprintf(str, FTI_BUFS, "FTI-FF: L4RecoveryInit - found file with name: %s", entry->d_name);
                FTI_Print(str, FTI_DBUG);
                sscanf(entry->d_name, "Ckpt%d-Rank%d.fti", &ckptID, &fileTarget );
                if( fileTarget == FTI_Topo->myRank ) {
                    snprintf(tmpfn, FTI_BUFS, "%s/%s", FTI_Ckpt[4].dir, entry->d_name);
                    int ferr = stat(tmpfn, &ckptFS);
                    if (!ferr && S_ISREG(ckptFS.st_mode) && ckptFS.st_size > sizeof(FTIFF_metaInfo) ) {
                        int fd = open(tmpfn, O_RDONLY);
                        lseek(fd, 0, SEEK_SET);
                        read( fd, FTIFFMeta, sizeof(FTIFF_metaInfo) );
                        unsigned char hash[MD5_DIGEST_LENGTH];
                        FTIFF_GetHashMetaInfo( hash, FTIFFMeta );
                        if ( memcmp( FTIFFMeta->myHash, hash, MD5_DIGEST_LENGTH ) == 0 ) {
                            FTI_Exec->meta[4].fs[0] = ckptFS.st_size;    
                            FTI_Exec->ckptID = ckptID;
                            strncpy(checksum, FTIFFMeta->checksum, MD5_DIGEST_STRING_LENGTH);
                            strncpy(FTI_Exec->meta[1].ckptFile, entry->d_name, NAME_MAX);
                            strncpy(FTI_Exec->meta[4].ckptFile, entry->d_name, NAME_MAX);
                            fexist = 1;
                        }
                        break;
                    }            
                }
            }
        }
    }
    MPI_Allreduce(&fexist, &fcount, 1, MPI_INT, MPI_SUM, FTI_COMM_WORLD);
    int fneeded = FTI_Topo->nbNodes*FTI_Topo->nbApprocs;
    int res = (fcount == fneeded) ? FTI_SCES : FTI_NSCS;
    closedir(L4CkptDir);
    return res;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Computes has of the ckpt meta data structure   
  @param    hash          hash to compute.
  @param    FTIFFMeta     Ckpt file meta data.
 **/
/*-------------------------------------------------------------------------*/
void FTIFF_GetHashMetaInfo( unsigned char *hash, FTIFF_metaInfo *FTIFFMeta ) 
{
    MD5_CTX md5Ctx;
    MD5_Init (&md5Ctx);
    MD5_Update( &md5Ctx, FTIFFMeta->checksum, MD5_DIGEST_STRING_LENGTH );
    MD5_Update( &md5Ctx, &(FTIFFMeta->timestamp), sizeof(long) );
    MD5_Update( &md5Ctx, &(FTIFFMeta->ckptSize), sizeof(long) );
    MD5_Update( &md5Ctx, &(FTIFFMeta->fs), sizeof(long) );
    MD5_Update( &md5Ctx, &(FTIFFMeta->ptFs), sizeof(long) );
    MD5_Update( &md5Ctx, &(FTIFFMeta->maxFs), sizeof(long) );
    MD5_Final( hash, &md5Ctx );
}

/*-------------------------------------------------------------------------*/
/**
  @brief      Frees allocated memory for the FTI-FF meta data struct list
  @param      last      Last element in FTI-FF metadata list.
 **/
/*-------------------------------------------------------------------------*/
void FTIFF_FreeDbFTIFF(FTIFF_db* last)
{
    if (last) {
        FTIFF_db *current = last;
        FTIFF_db *previous;
        while( current ) {
            previous = current->previous;
            // make sure there is a dbvar struct allocated.
            assert(current->dbvars);
            free(current->dbvars);
            free(current);
            current = previous;
        }
    }
}




