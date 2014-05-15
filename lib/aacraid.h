/*
 *  Copyright (c) 2014 PMC-Sierra Corporation.
 *
 *
 *           Name:  aacraid.h
 *          Title:  aacraid driver Serial Attached SCSI structures and definitions
 *  Creation Date:  April 12, 2014
 *
 *   Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------

 *  --------------------------------------------------------------------------
 */

struct _LIST_ENTRY {
    struct _LIST_ENTRY *Flink;
    struct _LIST_ENTRY *Blink;
}  LIST_ENTRY;


typedef struct {
    uint32_t    XferState;
    uint16_t    Command;
    uint8_t     StructType;
    uint8_t     Flags;
    uint16_t    Size;
    uint16_t    SenderSize;
    uint32_t    SenderFibAddress;
    uint32_t    ReceiveFibAddress;
    uint32_t    SenderData;

    union {
       struct{
       uint32_t    ReceiverTimeStart;
       uint32_t    ReceiverTimeDone;
    }_s;
  }_u;
}  FibHdr;

#define SIZE_FIB_HDR     (sizeof(FibHdr))
#define SIZE_FIB_DATA    (512 - SIZE_FIB_HDR)

typedef struct {
    uint32_t function;
    uint32_t expander_id;
    uint8_t  sas_address[8];
    uint32_t status;
    uint32_t dataTransmitLength;
    uint32_t dataReceiveLength;
}  SmpPassThruHeader;

#define SIZE_SMP_PASS_THRU_HDR     (sizeof(SmpPassThruHeader))
#define SIZE_SMP_PASS_THRU_DATA    (512 - SIZE_FIB_HDR - SIZE_SMP_PASS_THRU_HDR)

typedef struct {
    FibHdr   fib_header;
    uint8_t  data[SIZE_FIB_DATA];
}  Fib;

typedef struct {
    SmpPassThruHeader header;
    uint8_t           smp_request[SIZE_SMP_PASS_THRU_DATA] ;
}  HostSmpPassThruRequest;

typedef struct {
    SmpPassThruHeader header;
    union {
        uint32_t expander_count;
        uint8_t  sas_address[8];
        uint8_t  reponse_data[SIZE_SMP_PASS_THRU_DATA];
    }  SmpResponse;
}  HostSmpPassThruResult;

#define CRC_LEN 4

#define MAX_SIZE_SMP_PASS_THRU_LEN (SIZE_FIB_DATA - SIZE_SMP_PASS_THRU_HDR)

#define SIZE_FIB (sizeof(Fib))
#define SIZE_SMP_PASS_THRU_REQ (sizeof(HostSmpPassThruRequest))
#define SIZE_SMP_PASS_THRU_RES (sizeof(HostSmpPassThruResult))

#define SMP_PASS_THRU_SUCCESS                   0x00
#define SMP_PASS_THRU_ERROR                     0x01
#define SMP_PASS_THRU_BUSY                      0x02
#define SMP_PASS_THRU_TIMEOUT                   0x03
#define SMP_PASS_THRU_PARM_INVALID              0x04

#define SMP_REQ_IN_PROGRESS                     0x00010000
#define SMP_REQ_WAIT_4_REQUEST                  0x00020000
#define SMP_REQ_SEND                            0x00040000
#define SMP_REQ_IDLE                            0x00080000
#define SMP_REQ_WAIT_4_RESPONSE                 0x00100000

#define SMP_FUNC_CMD_BY_EXP_INDEX               0x00
#define SMPPassThruFib                          0x38E
#define HostOwned                               (1<<0)
#define SentFromHost                            (1<<5)
#define ApiFib                                  (1<<20)
#define Tfib                                    1

#define CTL_CODE(function, method) (                 \
            (4<< 16) | ((function) << 2) | (method) \
)

#define METHOD_BUFFERED                 0
#define METHOD_NEITHER                  3

#define FSACTL_SENDFIB                  CTL_CODE(2050, METHOD_BUFFERED)
