#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mpi_type.h"
#include "mpi.h"
#include "mpi_sas.h"

#include "smp_aac_io.h"

#include "aacraid.h"


#define AAC_DEV_MAJOR 251
#define AAC_DEV_MINOR 0

static int aacDevMjr;
static int aacDevMnr;


//initalise the aac device

int
chk_aac_device(const char * dev_name,int verbose)
{
    int ret =0;

 struct stat st;
    FILE *fp;

    aacDevMjr = -1;
    aacDevMnr = -1;

    int  nc  = -1;
    char line[256];

    //Open /proc/devices to find if aac interface it present
    fp = fopen("/proc/devices","r");
    if(NULL == fp && verbose) {
        fprintf(stderr,"chk_aac_device : /proc/devices Not Found : %s\n",safe_strerror(errno));
        return ret;
    }

    //Search for aac in /proc/devices
    while(fgets(line, sizeof(line), fp) != NULL) {
        nc = -1;
        if(sscanf(line,"%d aac%n", &aacDevMjr, &nc) == 1
                && nc > 0 && '\n' == line[nc])
          break;
        aacDevMjr = -1;
    }

  //work with /proc/devices is done
    fclose(fp);

  // aac in /proc/devices is not found
    if ( aacDevMjr < 0 ){
        if (verbose)
            fprintf(stderr,"chk_aac_device : aac entry not found in /proc/devices \n");
        return 0;
    }


//Get the minor number from arguments
    if(sscanf(dev_name, "/dev/aac%d", &aacDevMnr) != 1) {
        if(strncmp(dev_name,"/dev/aac",8) == 0) {
            aacDevMnr = 0;
        } else {
            fprintf(stderr,"chk_aac_device : Invalid device name\n");
            return 0;
        }
    }

    //checks if file already exists
    if(open(dev_name,O_RDWR) < 0) {
        if(mknod(dev_name,S_IFCHR,makedev(aacDevMjr,aacDevMnr))) {
            fprintf (stderr,"chk_aac_device : Mknod failed : %s\n",safe_strerror(errno));
            return 0;
        }
    }

    //Checks for /dev/aacX created with different major and minor numbers
    if (stat(dev_name, &st) < 0) {
        fprintf (stderr,"chk_aac_device : Stat failed : %s \n",safe_strerror(errno));
    }

    if ((S_ISCHR(st.st_mode)) && (aacDevMjr ==(int) major(st.st_rdev))) {
        if (aacDevMnr == (int) minor(st.st_rdev))
           return 1;
    }


    if (verbose) {
        if (S_ISCHR(st.st_mode))
            fprintf(stderr, "chk_aac_device: wanted char device "
              "major,minor=%d,%d\n got=%d,%d\n", aacDevMjr,
               aacDevMnr, major(st.st_rdev),minor(st.st_rdev));
        else
            fprintf(stderr, "chk_aac_device: wanted char device "
              "major,minor=%d,%d\n but didn't get char device\n",
               aacDevMjr,aacDevMnr);
    }
    return 0;
}

int
open_aac_device(const char * dev_name ,int verbose)
{
    int res;

    struct stat st;

    res = open(dev_name,O_RDWR);

    if(res<0){
        if (verbose)
            fprintf(stderr,"Open_aac_device failed");
    }else if (fstat(res, &st) >= 0){
        if (!((S_ISCHR(st.st_mode)) && (aacDevMjr ==(int) major(st.st_rdev)) &&
            (aacDevMnr ==(int) minor(st.st_rdev))))
            fprintf(stderr,"Major and Minor  do not match\n");
    }else if (verbose)
        fprintf(stderr,"open_aac_device:stat failed");
    return res;
}

int
close_aac_device(int fd)
{
    return close(fd);

}

int
send_req_aac(int fd, int subvalue, const unsigned char * target_sa,
             struct smp_req_resp * rresp, int verbose)
{
    int ret = -1;

    HostSmpPassThruRequest *aSmpPassThruReq = NULL;
    HostSmpPassThruResult  *aSmpPassThruRes = NULL;
    HostSmpPassThruResult  *aSmpResult = NULL;

    SmpPassThruHeader tmpSmpHdr;

    Fib *aFib =NULL;

    unsigned int aSmpCmdReqLen  = rresp->request_len - CRC_LEN;
    unsigned int aSmpCmdReqOff  = 0;
    unsigned int aSmpReqHdrStat = 0;
    unsigned int bytesToCopy    = 0;

    unsigned int bytesToSave    = 0;
    unsigned int aSmpCmdRespLen = 0;
    unsigned int aSmpCmdRespOff = 0;

    if (target_sa) { ; }                /* suppress unused warning */
    if (verbose) { ; }                  /* suppress unused warning */
    for(aSmpCmdReqOff = 0 ; aSmpCmdReqOff <(unsigned int) rresp->request_len - CRC_LEN;) {

        if(aSmpCmdReqLen <= MAX_SIZE_SMP_PASS_THRU_LEN){
            bytesToCopy = aSmpCmdReqLen;
        } else {
            bytesToCopy = MAX_SIZE_SMP_PASS_THRU_LEN;
        }

        aSmpPassThruReq = (HostSmpPassThruRequest *)malloc(SIZE_SMP_PASS_THRU_REQ);
        if( NULL == aSmpPassThruReq){
         fprintf(stderr,"send_req_aac: Could not allocate memory for SMP Pass Thru Request \n");
         goto err_out;
        }
        memset(aSmpPassThruReq,0,SIZE_SMP_PASS_THRU_REQ);

        aSmpPassThruReq->header.function     = SMP_FUNC_CMD_BY_EXP_INDEX;
        aSmpPassThruReq->header.expander_id  = subvalue;
        aSmpPassThruReq->header.dataTransmitLength = rresp->request_len - CRC_LEN; // REMOVE CRC BITS
        aSmpPassThruReq->header.dataReceiveLength =  rresp->max_response_len - CRC_LEN;
        aSmpPassThruReq->header.status = aSmpReqHdrStat;

        memcpy(aSmpPassThruReq->smp_request,rresp->request + aSmpCmdReqOff,bytesToCopy);


        aFib = (Fib *)malloc(SIZE_FIB);
        if( NULL == aFib){
            fprintf(stderr,"send_req_aac: Could not allocate memory for FIB \n");
            goto err_out;
        }
        memset(aFib,0,SIZE_FIB);

        aFib->fib_header.Command = SMPPassThruFib;
        aFib->fib_header.Size    = SIZE_SMP_PASS_THRU_REQ;
        aFib->fib_header.XferState = (ApiFib|HostOwned); //ApiFib SentFromHost
        aFib->fib_header.StructType= Tfib;
        aFib->fib_header.SenderData = 0;
        aFib->fib_header.SenderSize = SIZE_FIB;

        memcpy(aFib->data,aSmpPassThruReq,SIZE_SMP_PASS_THRU_REQ);

        if( ioctl (fd,FSACTL_SENDFIB,aFib) !=0) {
            fprintf(stderr,"send_req_aac: Request FSACTL_SENDFIB ioctl failed - %s",safe_strerror(errno));
            goto err_out;
        }

        aSmpResult = (HostSmpPassThruResult*)aFib->data;

        aSmpCmdReqOff += bytesToCopy;
        aSmpCmdReqLen -= bytesToCopy;

    if(aSmpResult -> header.status != SMP_REQ_WAIT_4_REQUEST) {
        break;
    }

    aSmpReqHdrStat = aSmpResult->header.status;
  }
  if (aSmpCmdReqLen != 0) {
    fprintf(stderr,"send_req_aac:Firmware did not aacept the request\n ");
    goto err_out;
  }

if((aSmpResult->header.status != SMP_PASS_THRU_SUCCESS)
   && (aSmpResult->header.status != SMP_REQ_WAIT_4_RESPONSE)){

    switch(aSmpResult->header.status)
    {
    case SMP_PASS_THRU_BUSY:
      fprintf(stderr,"send_req_aac: Request Firmware Busy\n ");
      break;
    case SMP_PASS_THRU_TIMEOUT:
      fprintf(stderr,"send_req_aac: Request Firmware Timeout\n ");
      break;
    case SMP_PASS_THRU_PARM_INVALID:
      fprintf(stderr,"send_req_aac: Request Firmware Parmeters Invalid\n ");
      break;
    default:
      fprintf(stderr,"send_req_aac: Request Firmware command error\n ");
      break;
    }
    fprintf(stderr,"send_req_aac: Request SMP Header Status - %x \n ",aSmpResult->header.status);
  goto err_out;
  }


  tmpSmpHdr = aSmpResult->header;

  rresp->act_response_len = tmpSmpHdr.dataReceiveLength;
  aSmpCmdRespLen =          tmpSmpHdr.dataReceiveLength;

  bytesToSave    = 0;
  aSmpCmdRespOff = 0;

  for(aSmpCmdRespOff = 0; aSmpCmdRespOff < tmpSmpHdr.dataReceiveLength;) {

    if(aSmpCmdRespLen <= MAX_SIZE_SMP_PASS_THRU_LEN) {
      bytesToSave = aSmpCmdRespLen;
    } else {
      bytesToSave = MAX_SIZE_SMP_PASS_THRU_LEN;
    }


    memcpy(rresp->response + aSmpCmdRespOff ,&aFib->data[sizeof(SmpPassThruHeader)],aSmpResult->header.dataReceiveLength);

    aSmpCmdRespOff += bytesToSave;
    aSmpCmdRespLen -= bytesToSave;

    if(aSmpResult->header.status != SMP_REQ_WAIT_4_RESPONSE)
    {
      break;
    }

    aSmpPassThruRes = (HostSmpPassThruResult *)malloc(SIZE_SMP_PASS_THRU_RES);
    if( NULL == aSmpPassThruRes) {
        fprintf(stderr,"send_req_aac: Could not allocate memory for SMP PASS THRU RESULT \n");
        goto err_out;
    }
    memset(aSmpPassThruRes,0,SIZE_SMP_PASS_THRU_RES);

    aSmpPassThruRes->header.function            = tmpSmpHdr.function;
    aSmpPassThruRes->header.expander_id         = tmpSmpHdr.expander_id;
    aSmpPassThruRes->header.dataTransmitLength  = tmpSmpHdr.dataTransmitLength;
    aSmpPassThruRes->header.dataReceiveLength   = tmpSmpHdr.dataReceiveLength;
    aSmpPassThruRes->header.status              = tmpSmpHdr.status;

    memset(aFib,0,SIZE_FIB);

    aFib->fib_header.Command = SMPPassThruFib;
    aFib->fib_header.Size    = SIZE_SMP_PASS_THRU_RES;
    aFib->fib_header.XferState = (ApiFib|HostOwned); //ApiFib SentFromHost
    aFib->fib_header.StructType= Tfib;
    aFib->fib_header.SenderData = 0;
    aFib->fib_header.SenderSize = SIZE_FIB;

    memcpy(aFib->data,aSmpPassThruRes,SIZE_SMP_PASS_THRU_RES);

    if( ioctl (fd,FSACTL_SENDFIB,aFib) !=0){
        fprintf(stderr,"send_req_aac: Result FSACTL_SENDFIB ioctl failed  - %s",safe_strerror(errno));
        goto err_out;
    }

    aSmpResult = (HostSmpPassThruResult*)aFib->data;

  }

if((aSmpResult->header.status != SMP_PASS_THRU_SUCCESS)) {

 switch(aSmpResult->header.status)
    {
     case SMP_PASS_THRU_BUSY:
      fprintf(stderr,"send_req_aac: Result Firmware Busy\n ");
       break;
     case SMP_PASS_THRU_TIMEOUT:
       fprintf(stderr,"send_req_aac: Result Firmware Timeout\n ");
       break;
     case SMP_PASS_THRU_PARM_INVALID:
       fprintf(stderr,"send_req_aac: Result Firmware Parmeters Invalid\n ");
       break;
     default:
       fprintf(stderr,"send_req_aac: Result Firmware command error\n ");
       break;
     }
     fprintf(stderr,"send_req_aac: Result SMP Header Status - %x \n ",aSmpResult->header.status);

  } else {
   ret = 0;
}

err_out:

    if(aSmpPassThruReq)
        free(aSmpPassThruReq);
    if(aSmpPassThruRes)
        free(aSmpPassThruRes);
    if(aFib)
       free(aFib);

  return ret;
}
