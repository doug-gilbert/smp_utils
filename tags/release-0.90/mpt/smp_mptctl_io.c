#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
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
#include <malloc.h>
#include <linux/major.h>


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

int chk_mpt_device(const char * dev_name, int verbose)
{
    struct stat st;

    if (stat(dev_name, &st) < 0) {
        if (verbose)
            perror("chk_mpt_dev_file: stat failed");
        return 0;
    }
    if ((S_ISCHR(st.st_mode)) && (MPT_DEV_MAJOR == major(st.st_rdev)) &&
        (MPT_DEV_MINOR == minor(st.st_rdev)))
        return 1;
    else {
        if (verbose) {
            if (S_ISCHR(st.st_mode))
                fprintf(stderr, "chk_mpt_dev_file: wanted char device "
                        "major,minor=%d,%d\n    got=%d,%d\n", MPT_DEV_MAJOR,
                        MPT_DEV_MINOR, major(st.st_rdev), minor(st.st_rdev));
            else
                fprintf(stderr, "chk_mpt_dev_file: wanted char device "
                        "major,minor=%d,%d\n    didn't get char device\n",
                        MPT_DEV_MAJOR, MPT_DEV_MINOR);
        }
        return 0;
    }
}

int open_mpt_device(const char * dev_name, int verbose)
{
    int res;

    res = open(dev_name, O_RDWR);
    if (res < 0) {
        if (verbose)
            perror("open_mpt_device failed");
    }
    return res;
}

int close_mpt_device(int fd)
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
int issueMptCommand(int fd, int ioc_num, mpiIoctlBlk_t *mpiBlkPtr)
{
        MPIDefaultReply_t *pReply = NULL;
        int CmdBlkSize;
        int status = -1;

        CmdBlkSize = sizeof(mpiIoctlBlk_t) + ((mpiBlkPtr->dataSgeOffset)*4) + 8;

        // ShowBuf("Command Block Before: ", mpiBlkPtr, CmdBlkSize, 0);

        /* Set the IOC number prior to issuing this command.
         */
        mpiBlkPtr->hdr.iocnum = ioc_num;
        mpiBlkPtr->hdr.port = 0;

        if (ioctl(fd, (unsigned long) MPTCOMMAND, (char *) mpiBlkPtr) != 0)
                perror("IOCTL failed");
        else {
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
                        status = 0;
        }

        return status;
}

#if 1
int send_req_mpt(int fd, int subvalue, const unsigned char * target_sa,
                        struct smp_req_resp * rresp, int verbose)
{
        mpiIoctlBlk_t * mpiBlkPtr = NULL;
        pSmpPassthroughRequest_t smpReq;
        pSmpPassthroughReply_t smpReply;
        uint numBytes;
        int  k, status;
        /* here is my hard coded expander sas address */
        // unsigned char expanderSasAddr[] =
        //               {0x9C,0x03,0x00,0x00,0x60,0x05,0x06,0x50};
        /* here is a Request General */
        // unsigned char smp_request[] = {0x40, 0, 0, 0};
        u16     ioc_stat;
        unsigned char * ucp;
        int ret = -1;

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
        mpiBlkPtr->replyFrameBufPtr = (char *)rresp->response;
        memset(mpiBlkPtr->replyFrameBufPtr, 0, rresp->max_response_len);
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
        if (ioc_stat != MPI_IOCSTATUS_SUCCESS) {
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

#else

int smp_send_req(int fd, int subvalue, struct smp_req_resp * rresp,
                 int verbose)
{
        mpiIoctlBlk_t * mpiBlkPtr = NULL;
        pSmpPassthroughRequest_t smpReq;
        pSmpPassthroughReply_t smpReply;
        uint numBytes;
        int  status, k;
        u16     ioc_stat;
        unsigned char * ucp;
        int ret = -1;

        numBytes = offsetof(SmpPassthroughRequest_t, SGL) +
                   (2 * sizeof(SGESimple64_t));
        mpiBlkPtr = malloc(sizeof(mpiIoctlBlk_t) + numBytes);
        if (mpiBlkPtr == NULL)
                goto err_out;
        memset(mpiBlkPtr, 0, sizeof(mpiIoctlBlk_t) + numBytes);
        mpiBlkPtr->replyFrameBufPtr = malloc(REPLY_SIZE);
        if (mpiBlkPtr->replyFrameBufPtr == NULL)
                goto err_out;
        memset(mpiBlkPtr->replyFrameBufPtr, 0, REPLY_SIZE);
        smpReq = (pSmpPassthroughRequest_t)mpiBlkPtr->MF;
        mpiBlkPtr->dataSgeOffset = offsetof(SmpPassthroughRequest_t, SGL) / 4;
        smpReply = (pSmpPassthroughReply_t)mpiBlkPtr->replyFrameBufPtr;

        /* send smp request */
        mpiBlkPtr->dataOutSize = rresp->request_len;
        mpiBlkPtr->dataOutBufPtr = malloc(mpiBlkPtr->dataOutSize);
        if (mpiBlkPtr->dataOutBufPtr == NULL)
                goto err_out;
        memcpy(mpiBlkPtr->dataOutBufPtr, rresp->request, rresp->request_len);
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
        smpReq->RequestDataLength = rresp->request_len; // <<<<<<<<<<<< ??
        smpReq->Function = MPI_FUNCTION_SMP_PASSTHROUGH;
        ucp = (unsigned char *)&smpReq->SASAddress;
        for (k = 0; k < 8; ++k)
                ucp[k] = rresp->sas_addr[7 - k];

        status = issueMptCommand(fd, subvalue, mpiBlkPtr);

        if (status != 0) {
                if (verbose > 1)
                        fprintf(stderr, "issueMptCommand failed\n");
                goto err_out;
        }

        ioc_stat = smpReply->IOCStatus & MPI_IOCSTATUS_MASK;
        if (ioc_stat != MPI_IOCSTATUS_SUCCESS) {
                printf("IOCStatus=0x%X IOCLogInfo=0x%X SASStatus=0x%X\n",
                    smpReply->IOCStatus,
                    smpReply->IOCLogInfo,
                    smpReply->SASStatus);
        } else {
                printf("succeeded\n");
        }
        memcpy(rresp->response, mpiBlkPtr->dataInBufPtr, rresp->max_response_len);
        rresp->act_response_len = -1;
        ret = 0;

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
        rresp->transport_err = ret;
        return ret;
}
#endif

#if 0
/*****************************************************************
 * ConfigIoctl
 *
 *****************************************************************/
void SmpTwoSGLsIoctl(int fd, int ioc_num)
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

void SmpImmediateIoctl(int fd, int ioc_num)
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
