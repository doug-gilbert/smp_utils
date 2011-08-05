#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <fcntl.h>
//#include <curses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
// #include <linux/major.h>


#include "mpi_type.h"
#include "mpi.h"
#include "mpi_sas.h"

#include "smp_mptctl_glue.h"
#include "smp_mptctl_io.h"

#include "mptctl.h"


#define REPLY_SIZE 128

typedef struct mpt_ioctl_command mpiIoctlBlk_t;

#define MPT_DEV_MAJOR 10
#define MPT_DEV_MINOR 220
#define MPT2_DEV_MINOR 221

static const char null_sas_addr[8];
static int mptcommand = MPTCOMMAND;


/* Part of interface to upper level. */
int
chk_mpt_device(const char * dev_name, int verbose)
{
    struct stat st;

    if (stat(dev_name, &st) < 0) {
        if (verbose)
            perror("chk_mpt_device: stat failed");
        return 0;
    }
    if ((S_ISCHR(st.st_mode)) && (MPT_DEV_MAJOR == major(st.st_rdev))) {
        if ((MPT_DEV_MINOR == minor(st.st_rdev)) ||
            (MPT2_DEV_MINOR == minor(st.st_rdev)))
            return 1;
    }
    if (verbose) {
        if (S_ISCHR(st.st_mode))
            fprintf(stderr, "chk_mpt_device: wanted char device "
                    "major,minor=%d,%d-%d\n    got=%d,%d\n", MPT_DEV_MAJOR,
                    MPT_DEV_MINOR, MPT2_DEV_MINOR, major(st.st_rdev),
                    minor(st.st_rdev));
        else
            fprintf(stderr, "chk_mpt_device: wanted char device "
                    "major,minor=%d,%d-%d\n    but didn't get char device\n",
                    MPT_DEV_MAJOR, MPT_DEV_MINOR, MPT2_DEV_MINOR);
    }
    return 0;
}

/* Part of interface to upper level. */
int
open_mpt_device(const char * dev_name, int verbose)
{
    int res;
    struct stat st;

    res = open(dev_name, O_RDWR);
    if (res < 0) {
        if (verbose)
            perror("open_mpt_device failed");
    } else if (fstat(res, &st) >= 0) {
        if ((S_ISCHR(st.st_mode)) && (MPT_DEV_MAJOR == major(st.st_rdev)) &&
            (MPT2_DEV_MINOR == minor(st.st_rdev)))
            mptcommand = MPT2COMMAND;
        else
            mptcommand = MPTCOMMAND;
    } else if (verbose)
        perror("open_mpt_device: stat failed");
    return res;
}

/* Part of interface to upper level. */
int
close_mpt_device(int fd)
{
    return close(fd);
}
        

/*****************************************************************
 *                                                               *
 * Copyright 2000-2002 LSI Logic. All rights reserved.           *
 *                                                               *
 *****************************************************************/

#if 0
/* Function Prototypes
 */
void SmpTwoSGLsIoctl(int fd, int ioc_num);
void SmpImmediateIoctl(int fd, int ioc_num);
#endif

/*****************************************************************
 * issueMptIoctl
 *
 * Generic command to issue the MPT command using the special
 * SDI_IOC | 0x01 Ioctl Function.
 *****************************************************************/
int
issueMptCommand(int fd, int ioc_num, mpiIoctlBlk_t *mpiBlkPtr)
{
        int CmdBlkSize;
        int status = -1;

        CmdBlkSize = sizeof(mpiIoctlBlk_t) + ((mpiBlkPtr->dataSgeOffset)*4) + 8;

        // ShowBuf("Command Block Before: ", mpiBlkPtr, CmdBlkSize, 0);

        /* Set the IOC number prior to issuing this command.
         */
        mpiBlkPtr->hdr.iocnum = ioc_num;
        mpiBlkPtr->hdr.port = 0;

        if (ioctl(fd, mptcommand, (char *) mpiBlkPtr) != 0)
                perror("MPTCOMMAND or MPT2COMMAND ioctl failed");
        else {
#if 0
                MPIDefaultReply_t *pReply = NULL;

                /* Be smarter about dumping and using data.
                 * If SCSI IO, reply may be null and data xfer
                 * will be good. If a non-SCSI IO, if reply is
                 * NULL data is definately garbage.
                 *
                 */
                pReply = (MPIDefaultReply_t *) mpiBlkPtr->replyFrameBufPtr;
                if ((pReply) && (pReply->MsgLength > 0)) {
                        // >>>>> pReply->IOCStatus = le16_to_cpu(pReply->IOCStatus); <<<<

                        // ShowBuf("Reply Frame : ", pReply, pReply->MsgLength * 4, 0);

                        status = pReply->IOCStatus & MPI_IOCSTATUS_MASK;

                } else
#endif
                        status = 0;
        }

        return status;
}

/* Part of interface to upper level. */
int
send_req_mpt(int fd, int subvalue, const unsigned char * target_sa,
             struct smp_req_resp * rresp, int verbose)
{
        mpiIoctlBlk_t * mpiBlkPtr = NULL;
        pSmpPassthroughRequest_t smpReq;
        pSmpPassthroughReply_t smpReply;
        uint numBytes;
        int  k, status;
        char reply_m[1200];
        u16     ioc_stat;
        unsigned char * ucp;
        int ret = -1;

        if (verbose && (0 == memcmp(target_sa, null_sas_addr, 8))) {
                fprintf(stderr, "The MPT interface typically needs SAS "
                        "address of target (e.g. expander).\n");
                fprintf(stderr, "A '--sa=SAS_ADDR' command line option "
                        "may be required. See man page.\n");
        }
        if (verbose > 2) {
                fprintf(stderr, "send_req_mpt: subvalue=%d  ", subvalue);
                fprintf(stderr, "SAS address=0x");
                for (k = 0; k < 8; ++k)
                        fprintf(stderr, "%02x", target_sa[7 - k]);
                fprintf(stderr, "\n");
                if (verbose > 3)
                        fprintf(stderr, "    mptctl two scatter gather list "
                                "interface\n");
        }
        numBytes = offsetof(SmpPassthroughRequest_t, SGL) +
                   (2 * sizeof(SGESimple64_t));
        mpiBlkPtr = malloc(sizeof(mpiIoctlBlk_t) + numBytes);
        if (mpiBlkPtr == NULL)
                goto err_out;
        memset(mpiBlkPtr, 0, sizeof(mpiIoctlBlk_t) + numBytes);
        mpiBlkPtr->replyFrameBufPtr = reply_m;
        memset(mpiBlkPtr->replyFrameBufPtr, 0, sizeof(reply_m));
        mpiBlkPtr->maxReplyBytes = sizeof(reply_m);
        smpReq = (pSmpPassthroughRequest_t)mpiBlkPtr->MF;
        mpiBlkPtr->dataSgeOffset = offsetof(SmpPassthroughRequest_t, SGL) / 4;
        smpReply = (pSmpPassthroughReply_t)mpiBlkPtr->replyFrameBufPtr;

        /* send smp request */
        mpiBlkPtr->dataOutSize = rresp->request_len - 4;
        mpiBlkPtr->dataOutBufPtr = (char *)rresp->request;
        mpiBlkPtr->dataInSize = rresp->max_response_len + 4;
        mpiBlkPtr->dataInBufPtr = malloc(mpiBlkPtr->dataInSize);
        if(mpiBlkPtr->dataInBufPtr == NULL)
                goto err_out;
        memset(mpiBlkPtr->dataInBufPtr, 0, mpiBlkPtr->dataInSize);

        /* Populate the SMP Request
         */

        /* PassthroughFlags
         * Bit7: 0=two SGLs 1=Payload returned in Reply
         */
        memset(smpReq, 0, sizeof(smpReq));
        smpReq->RequestDataLength = rresp->request_len - 4; // <<<<<<<<<<<< ??
        smpReq->Function = MPI_FUNCTION_SMP_PASSTHROUGH;
        ucp = (unsigned char *)&smpReq->SASAddress;
        memcpy(ucp, target_sa, 8);

        status = issueMptCommand(fd, subvalue, mpiBlkPtr);

        if (status != 0) {
                fprintf(stderr, "ioctl failed\n");
                goto err_out;
        }

        ioc_stat = smpReply->IOCStatus & MPI_IOCSTATUS_MASK;
        if ((ioc_stat != MPI_IOCSTATUS_SUCCESS) ||
            (smpReply->SASStatus != MPI_SASSTATUS_SUCCESS)) {
                if (verbose) {
                        switch(smpReply->SASStatus) {
                        case MPI_SASSTATUS_UNKNOWN_ERROR: 
                                fprintf(stderr, "Unknown SAS (SMP) error\n");
                                break;
                        case MPI_SASSTATUS_INVALID_FRAME: 
                                fprintf(stderr, "Invalid frame\n");
                                break;
                        case MPI_SASSTATUS_UTC_BAD_DEST: 
                                fprintf(stderr, "Unable to connect (bad "
                                        "destination)\n");
                                break;
                        case MPI_SASSTATUS_UTC_BREAK_RECEIVED: 
                                fprintf(stderr, "Unable to connect (break "
                                        "received)\n");
                                break;
                        case MPI_SASSTATUS_UTC_CONNECT_RATE_NOT_SUPPORTED: 
                                fprintf(stderr, "Unable to connect (connect "
                                        "rate not supported)\n");
                                break;
                        case MPI_SASSTATUS_UTC_PORT_LAYER_REQUEST: 
                                fprintf(stderr, "Unable to connect (port "
                                        "layer request)\n");
                                break;
                        case MPI_SASSTATUS_UTC_PROTOCOL_NOT_SUPPORTED: 
                                fprintf(stderr, "Unable to connect (protocol "
                                        "(SMP target) not supported)\n");
                                break;
                        case MPI_SASSTATUS_UTC_WRONG_DESTINATION: 
                                fprintf(stderr, "Unable to connect (wrong "
                                        "destination)\n");
                                break;
                        case MPI_SASSTATUS_SHORT_INFORMATION_UNIT: 
                                fprintf(stderr, "Short information unit\n");
                                break;
                        case MPI_SASSTATUS_DATA_INCORRECT_DATA_LENGTH: 
                                fprintf(stderr, "Incorrect data length\n");
                                break;
                        case MPI_SASSTATUS_INITIATOR_RESPONSE_TIMEOUT: 
                                fprintf(stderr, "Initiator response "
                                        "timeout\n");
                                break;
                        default:
                                if (smpReply->SASStatus !=
                                    MPI_SASSTATUS_SUCCESS) {
                                        fprintf(stderr, "Unrecognized SAS "
                                                "(SMP) error 0x%x\n",
                                                smpReply->SASStatus);
                                        break;
                                }
                                if (smpReply->IOCStatus ==
                                    MPI_IOCSTATUS_SAS_SMP_REQUEST_FAILED)
                                        fprintf(stderr, "SMP request failed "
                                                "(IOCStatus)\n");
                                else if (smpReply->IOCStatus ==
                                         MPI_IOCSTATUS_SAS_SMP_DATA_OVERRUN)
                                        fprintf(stderr, "SMP data overrun "
                                                "(IOCStatus)\n");
                                else if (smpReply->IOCStatus ==
                                         MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE)
                                        fprintf(stderr, "Device not there "
                                                "(IOCStatus)\n");
                                else
                                        fprintf(stderr, "IOCStatus=0x%x\n",
                                                smpReply->IOCStatus);
                        }
                }
                if (verbose > 1)
                        fprintf(stderr, "IOCStatus=0x%X IOCLogInfo=0x%X "
                                "SASStatus=0x%X\n",
                                smpReply->IOCStatus,
                                smpReply->IOCLogInfo,
                                smpReply->SASStatus);
        } else
                ret = 0;

        memcpy(rresp->response, mpiBlkPtr->dataInBufPtr, rresp->max_response_len);
        rresp->act_response_len = -1;

err_out:
        if (mpiBlkPtr) {
                if (mpiBlkPtr->dataInBufPtr)
                        free(mpiBlkPtr->dataInBufPtr);
                free(mpiBlkPtr);
        }
        return ret;
}

#if 0
/*****************************************************************
 * ConfigIoctl
 *
 *****************************************************************/
void
SmpTwoSGLsIoctl(int fd, int ioc_num)
{
        mpiIoctlBlk_t * mpiBlkPtr = NULL;
        pSmpPassthroughRequest_t smpReq;
        pSmpPassthroughReply_t smpReply;
        uint numBytes;
        int  status;
        /* here is my hard coded expander sas address */
        unsigned char expanderSasAddr[] = {0x9C,0x03,0x00,0x00,0x60,0x05,0x06,0x50};
        /* here is a Request General */
        unsigned char smp_request[] = {0x40, 0, 0, 0};
        u16     ioc_stat;

        numBytes = offsetof(SmpPassthroughRequest_t, SGL) +
                   (2 * sizeof(SGESimple64_t));
        mpiBlkPtr = malloc(sizeof(mpiIoctlBlk_t) + numBytes);
        if (mpiBlkPtr == NULL)
                return;
        memset(mpiBlkPtr, 0, sizeof(mpiIoctlBlk_t) + numBytes);
        mpiBlkPtr->replyFrameBufPtr = malloc(REPLY_SIZE);
        if (mpiBlkPtr->replyFrameBufPtr == NULL)
                goto err_out;
        memset(mpiBlkPtr->replyFrameBufPtr, 0, REPLY_SIZE);
        smpReq = (pSmpPassthroughRequest_t)mpiBlkPtr->MF;
        mpiBlkPtr->dataSgeOffset = offsetof(SmpPassthroughRequest_t, SGL) / 4;
        smpReply = (pSmpPassthroughReply_t)mpiBlkPtr->replyFrameBufPtr;

        /* send smp request */
        mpiBlkPtr->dataOutSize = sizeof(smp_request);
        mpiBlkPtr->dataOutBufPtr = malloc(mpiBlkPtr->dataOutSize);
        if (mpiBlkPtr->dataOutBufPtr == NULL)
                goto err_out;
        memcpy(mpiBlkPtr->dataOutBufPtr, smp_request, sizeof(smp_request));
        mpiBlkPtr->dataInSize = 1020;
        mpiBlkPtr->dataInBufPtr = malloc(mpiBlkPtr->dataInSize);
        if(mpiBlkPtr->dataInBufPtr == NULL)
                goto err_out;
        memset(mpiBlkPtr->dataInBufPtr, 0, mpiBlkPtr->dataInSize);

        /* Populate the SMP Request
         */

        /* PassthroughFlags
         * Bit7: 0=two SGLs 1=Payload returned in Reply
         */
        memset(smpReq, 0, sizeof(smpReq));
        smpReq->RequestDataLength = sizeof(smp_request);        // <<<<<<<<<<<< ??
        smpReq->Function = MPI_FUNCTION_SMP_PASSTHROUGH;
        memcpy(&smpReq->SASAddress, expanderSasAddr, 8);

        status = issueMptCommand(fd, ioc_num, mpiBlkPtr);

        if (status != 0) {
                printf("ioctl failed\n");
                goto err_out;
        }

        ioc_stat = smpReply->IOCStatus & MPI_IOCSTATUS_MASK;
        if ((ioc_stat != MPI_IOCSTATUS_SUCCESS) &&
            (ioc_stat != MPI_IOCSTATUS_SCSI_DATA_UNDERRUN)) {
                printf("IOCStatus=0x%X IOCLogInfo=0x%X SASStatus=0x%X\n",
                    smpReply->IOCStatus,
                    smpReply->IOCLogInfo,
                    smpReply->SASStatus);
        }else{
                printf("succeeded\n");
        }

        ShowBuf("Data In:  ", mpiBlkPtr->dataInBufPtr,
            mpiBlkPtr->dataInSize,1);

err_out:
        if (mpiBlkPtr) {
                if (mpiBlkPtr->dataInBufPtr)
                        free(mpiBlkPtr->dataInBufPtr);
                if (mpiBlkPtr->dataOutBufPtr)
                        free(mpiBlkPtr->dataOutBufPtr);
                if (mpiBlkPtr->replyFrameBufPtr)
                        free(mpiBlkPtr->replyFrameBufPtr);
                free(mpiBlkPtr);
        }
        return;
}

void
SmpImmediateIoctl(int fd, int ioc_num)
{
        mpiIoctlBlk_t * mpiBlkPtr = NULL;
        pSmpPassthroughRequest_t smpReq;
        pSmpPassthroughReply_t smpReply;
        uint numBytes;
        int  status;
        /* here is my hard coded expander sas address */
        unsigned char expanderSasAddr[] = {0x9C,0x03,0x00,0x00,0x60,0x05,0x06,0x50};
        /* here is a Request General */
        unsigned char smp_request[] = {0x40, 0, 0, 0};
        u16     ioc_stat;

        numBytes = offsetof(SmpPassthroughRequest_t,SGL) +
                (sizeof(smp_request));
        mpiBlkPtr = malloc(sizeof(mpiIoctlBlk_t) + numBytes);
        if (mpiBlkPtr == NULL)
                return;
        memset(mpiBlkPtr, 0, sizeof(mpiIoctlBlk_t) + numBytes);
        mpiBlkPtr->replyFrameBufPtr = malloc(REPLY_SIZE);
        if (mpiBlkPtr->replyFrameBufPtr == NULL)
                goto err_out;
        memset(mpiBlkPtr->replyFrameBufPtr, 0, REPLY_SIZE);
        smpReq = (pSmpPassthroughRequest_t)mpiBlkPtr->MF;
        mpiBlkPtr->dataSgeOffset = (offsetof(SmpPassthroughRequest_t,SGL) +
                sizeof(smp_request))/4;
        smpReply = (pSmpPassthroughReply_t)mpiBlkPtr->replyFrameBufPtr;

        /* Populate the SMP Request
         */

        /* PassthroughFlags
         * Bit7: 0=two SGLs 1=Payload returned in Reply
         */
        memset(smpReq, 0, sizeof(smpReq));
        smpReq->PassthroughFlags   = 1<<7;
        smpReq->Function           = MPI_FUNCTION_SMP_PASSTHROUGH;
        smpReq->RequestDataLength = sizeof(smp_request);
        memcpy(&smpReq->SASAddress,expanderSasAddr,8);
        memcpy(&smpReq->SGL,smp_request,sizeof(smp_request));

        status = issueMptCommand(fd, ioc_num, mpiBlkPtr);

        if (status != 0) {
                printf("ioctl failed\n");
                goto err_out;
        }

        ioc_stat = smpReply->IOCStatus & MPI_IOCSTATUS_MASK;
        if ((ioc_stat != MPI_IOCSTATUS_SUCCESS) &&
            (ioc_stat != MPI_IOCSTATUS_SCSI_DATA_UNDERRUN)) {
                printf("IOCStatus=0x%X IOCLogInfo=0x%X SASStatus=0x%X\n",
                    smpReply->IOCStatus,
                    smpReply->IOCLogInfo,
                    smpReply->SASStatus);
        } else
                printf("succeeded\n");

        ShowBuf("Data In:  ", smpReply->ResponseData,
            smpReply->ResponseDataLength,1);


err_out:
        if (mpiBlkPtr) {
                if (mpiBlkPtr->dataInBufPtr)
                        free(mpiBlkPtr->dataInBufPtr);
                if (mpiBlkPtr->dataOutBufPtr)
                        free(mpiBlkPtr->dataOutBufPtr);
                if (mpiBlkPtr->replyFrameBufPtr)
                        free(mpiBlkPtr->replyFrameBufPtr);
                free(mpiBlkPtr);
        }

        return;
}
#endif
