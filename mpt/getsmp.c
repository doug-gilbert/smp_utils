/*****************************************************************
 *                                                               *
 * Copyright 2000-2002 LSI Logic. All rights reserved.           *
 *                                                               *
 *****************************************************************/

/* System Include Files
 */
#include "apps.h"

/* Function Prototypes
 */
void SmpTwoSGLsIoctl (void);
void SmpImmediateIoctl (void);

/* Globals
 */
extern mpiIoctlBlk_t    *mpiBlkPtr;
extern int              g_iocnum;
extern int              g_bigEndian;

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
	char dataIn[PROC_READ_LINE];
	int  c;
	test_endianess_t test;

	/* Initialize (force a buffer flush)
	 */
	setvbuf(stdout,NULL,_IONBF,0);

	/* Get the system endianness....MPI request frames must have
	 * Little Endianness. If bigEndian, byte swap on data coming from FW
	 * and all request frame fields.
	 */
	test.u.foo = 0x01020304;
	g_bigEndian = 0;
	if (test.u.bar[0] != (test.u.foo & 0xFF))
		g_bigEndian = 1;

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

	/* Usage
	 */
	if (argc < 2){
		fprintf (stderr, "usage: %s iocnum [target]\n", argv[0]);
		return (0);
	}

	/* Get and open scsi node.
	 */
	if (OpenDevice(argv[1]))
		DoExit();

	printf("\n\nCompiled with Fusion-MPT %s Driver Header Files\n\n", DRIVER_VERSION);

	printf("SmpTwoSGLsIoctl\n");
	SmpTwoSGLsIoctl ();
	printf("SmpImmediateIoctl\n");
	SmpImmediateIoctl ();

	DoExit();
	return 0;
}

/*****************************************************************
 * ConfigIoctl
 *
 *****************************************************************/
void SmpTwoSGLsIoctl (void)
{
	pSmpPassthroughRequest_t smpReq;
	pSmpPassthroughReply_t smpReply;
	uint numBytes;
	int  status;
	/* here is my hard coded expander sas address */
	char expanderSasAddr[] = {0x9C,0x03,0x00,0x00,0x60,0x05,0x06,0x50};
	/* here is a Request General */
	char smp_request[] = {0x40, 0, 0, 0};
	u16	ioc_stat;

	numBytes = offsetof(SmpPassthroughRequest_t,SGL) +
		(2*sizeof (SGESimple64_t));
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL)
		return;
	smpReq = (pSmpPassthroughRequest_t) mpiBlkPtr->MF;
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
	mpiBlkPtr->dataInBufPtr = mpiBlkPtr->dataOutBufPtr = NULL;
	mpiBlkPtr->dataSgeOffset = offsetof(SmpPassthroughRequest_t,SGL)/4;
	smpReply = (pSmpPassthroughReply_t)mpiBlkPtr->replyFrameBufPtr;

	/* send smp request */
	mpiBlkPtr->dataOutSize = sizeof(smp_request);
	mpiBlkPtr->dataOutBufPtr = malloc(mpiBlkPtr->dataOutSize);
	if(mpiBlkPtr->dataOutBufPtr==NULL) {
		fprintf(stderr, "Can't allocate memory");
		perror("Reason");
		return;
	}
	memcpy(mpiBlkPtr->dataOutBufPtr,smp_request,sizeof(smp_request));
	mpiBlkPtr->dataInSize = 1020;
	mpiBlkPtr->dataInBufPtr = malloc(mpiBlkPtr->dataInSize);
	if(mpiBlkPtr->dataInBufPtr==NULL) {
		fprintf(stderr, "Can't allocate memory");
		perror("Reason");
		return;
	}

	/* Populate the SMP Request
	 */

	/* PassthroughFlags
	 * Bit7: 0=two SGLs 1=Payload returned in Reply
	 */
	smpReq->PassthroughFlags   = 0;
	smpReq->PhysicalPort       = 0;
	smpReq->ChainOffset        = 0;
	smpReq->Function           = MPI_FUNCTION_SMP_PASSTHROUGH;
	smpReq->RequestDataLength  = 0;
	smpReq->ConnectionRate     = 0;
	smpReq->MsgFlags           = 0;
	smpReq->MsgContext         = 0;
	smpReq->Reserved1          = 0;
	smpReq->Reserved2          = 0;
	smpReq->Reserved3          = 0;
	memcpy(&smpReq->SASAddress,expanderSasAddr,8);

	status = IssueMptCommand(MPT_FLAGS_KEEP_MEM);

	if (status != 0) {
		printf("ioctl failed\n");
		freeAllocMem();
		return;
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

	freeAllocMem();

	return;
}

void SmpImmediateIoctl (void)
{
	pSmpPassthroughRequest_t smpReq;
	pSmpPassthroughReply_t smpReply;
	uint numBytes;
	int  status;
	/* here is my hard coded expander sas address */
	char expanderSasAddr[] = {0x9C,0x03,0x00,0x00,0x60,0x05,0x06,0x50};
	/* here is a Request General */
	char smp_request[] = {0x40, 0, 0, 0};
	u16	ioc_stat;

	numBytes = offsetof(SmpPassthroughRequest_t,SGL) +
		(sizeof(smp_request));
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL)
		return;
	smpReq = (pSmpPassthroughRequest_t) mpiBlkPtr->MF;
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
	mpiBlkPtr->dataInBufPtr = mpiBlkPtr->dataOutBufPtr = NULL;
	mpiBlkPtr->dataSgeOffset = (offsetof(SmpPassthroughRequest_t,SGL) +
		sizeof(smp_request))/4;
	smpReply = (pSmpPassthroughReply_t)mpiBlkPtr->replyFrameBufPtr;

	/* Populate the SMP Request
	 */

	/* PassthroughFlags
	 * Bit7: 0=two SGLs 1=Payload returned in Reply
	 */
	smpReq->PassthroughFlags   = 1<<7;
	smpReq->PhysicalPort       = 0;
	smpReq->ChainOffset        = 0;
	smpReq->Function           = MPI_FUNCTION_SMP_PASSTHROUGH;
	smpReq->RequestDataLength  = 4;
	smpReq->ConnectionRate     = 0;
	smpReq->MsgFlags           = 0;
	smpReq->MsgContext         = 0;
	smpReq->Reserved1          = 0;
	smpReq->Reserved2          = 0;
	smpReq->Reserved3          = 0;
	memcpy(&smpReq->SASAddress,expanderSasAddr,8);
	memcpy(&smpReq->SGL,smp_request,sizeof(smp_request));

	status = IssueMptCommand(MPT_FLAGS_KEEP_MEM);

	if (status != 0) {
		printf("ioctl failed\n");
		freeAllocMem();
		return;
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

	ShowBuf("Data In:  ", smpReply->ResponseData,
	    smpReply->ResponseDataLength,1);

	freeAllocMem();

	return;
}
