#include <stdlib.h>
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



#include "mpi_type.h"
#include "mpi.h"
#include "mpi_sas.h"

#include "smp_mptctl_glue.h"

#include "mptctl.h"


#define REPLY_SIZE 128

typedef struct mpt_ioctl_command mpiIoctlBlk_t;


/*****************************************************************
 *                                                               *
 * Copyright 2000-2002 LSI Logic. All rights reserved.           *
 *                                                               *
 *****************************************************************/

/* Function Prototypes
 */
void SmpTwoSGLsIoctl(int fd, int ioc_num);
void SmpImmediateIoctl(int fd, int ioc_num);

/* Globals
 */
// extern mpiIoctlBlk_t    *mpiBlkPtr;
// extern int              g_iocnum;
// extern int              g_bigEndian;

/*****************************************************************
 *
 *
 *  MAIN
 *
 *
 *****************************************************************/
int main (int argc, char *argv[])
{
	FILE *pfile;
	char *name = "mptctl";
#if 0
	char dataIn[PROC_READ_LINE];
#endif
	int  c;
#if 0
	test_endianess_t test;
#endif

	/* Initialize (force a buffer flush)
	 */
	setvbuf(stdout,NULL,_IONBF,0);

#if 0
	/* Get the system endianness....MPI request frames must have
	 * Little Endianness. If bigEndian, byte swap on data coming from FW
	 * and all request frame fields.
	 */
	test.u.foo = 0x01020304;
	g_bigEndian = 0;
	if (test.u.bar[0] != (test.u.foo & 0xFF))
		g_bigEndian = 1;
#endif

#if 0
	/* Open /proc/modules to determine if
	 * driver loaded.
	 */
	snprintf(dataIn, PROC_READ_LINE, "/proc/modules");
	if ((pfile = fopen(dataIn, "r")) == NULL ) {
		printf("Open of /proc/modules failed!\n");
		DoExit();
	}

	c = 0;
	while (1) {
		if (fgets(dataIn, PROC_READ_LINE, pfile) == NULL)
			break;

		if (strstr(dataIn, name) != NULL) {
			c = 1;
			break;
		}
	}
	fclose(pfile);

	if (c == 0) {
		printf("Driver mptctl not loaded. Execute: insmod mptctl\n");
		DoExit();
	}
#endif

	/* Usage
	 */
	if (argc < 2){
		fprintf (stderr, "usage: %s iocnum [target]\n", argv[0]);
		return (0);
	}

	/* Get and open scsi node.
	 */
	if (OpenDevice(argv[1]))
		return 1; // DoExit();

#if 0
	printf("\n\nCompiled with Fusion-MPT %s Driver Header Files\n\n", DRIVER_VERSION);
#endif

	printf("SmpTwoSGLsIoctl\n");
	SmpTwoSGLsIoctl(0, 0);
	printf("SmpImmediateIoctl\n");
	SmpImmediateIoctl(0, 0);

	// DoExit();
	return 0;
}

/*****************************************************************
 * issueMptIoctl
 *
 * Generic command to issue the MPT command using the special
 * SDI_IOC | 0x01 Ioctl Function.
 *****************************************************************/
int issueMptCommand(int g_fd, int ioc_num, mpiIoctlBlk_t *mpiBlkPtr)
{
        MPIDefaultReply_t *pReply = NULL;
        int CmdBlkSize;
        int status = -1;

        CmdBlkSize = sizeof(mpiIoctlBlk_t) + ((mpiBlkPtr->dataSgeOffset)*4) + 8;

        ShowBuf("Command Block Before: ", mpiBlkPtr, CmdBlkSize, 0);

        /* Set the IOC number prior to issuing this command.
         */
        mpiBlkPtr->hdr.iocnum = ioc_num;
        mpiBlkPtr->hdr.port = 0;

        if (ioctl(g_fd, (unsigned long) MPTCOMMAND, (char *) mpiBlkPtr) != 0)
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
                        u8 funct = pReply->Function;
                        u8 state = mpiBlkPtr->replyFrameBufPtr[0x0D];

                        pReply->IOCStatus = le16_to_cpu(pReply->IOCStatus);

                        ShowBuf("Reply Frame : ", pReply, pReply->MsgLength * 4, 0);

                        status = pReply->IOCStatus & MPI_IOCSTATUS_MASK;

                } else
                        status = 0;
        }

        return status;
}

/*****************************************************************
 * ConfigIoctl
 *
 *****************************************************************/
void SmpTwoSGLsIoctl(int g_fd, int ioc_num)
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
	u16	ioc_stat;

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
	smpReq->RequestDataLength = sizeof(smp_request);	// <<<<<<<<<<<< ??
	smpReq->Function = MPI_FUNCTION_SMP_PASSTHROUGH;
	memcpy(&smpReq->SASAddress, expanderSasAddr, 8);

	status = issueMptCommand(g_fd, ioc_num, mpiBlkPtr);

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

void SmpImmediateIoctl(int g_fd, int ioc_num)
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
	u16	ioc_stat;

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

	status = issueMptCommand(g_fd, ioc_num, mpiBlkPtr);

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
