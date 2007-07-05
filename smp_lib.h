#ifndef SMP_LIB_H
#define SMP_LIB_H

/*
 * Copyright (c) 2006 Douglas Gilbert.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Version 1.03 [20060530]
 */


/* This header file contains defines and function declarations that may
 * be useful to Linux applications that communicate with devices that
 * use the Serial Attached SCSI (SAS) Management Protocol (SMP).
 * Reference: SCSI: http://www.t10.org and the most recent SAS draft
 * SAS-2 (revision 1).
 * This header is organised into two parts: part 1 is operating system
 * independent (i.e. may be useful to other OSes) and part 2 is Linux
 * specific (or at least closely related).
 */


/*
 * PART 1: OPERATING SYSTEM INDEPENDENT SECTION
 *         ------------------------------------
 */

#define SMP_FRAME_TYPE_REQ 0x40
#define SMP_FRAME_TYPE_RESP 0x41

#define SMP_FN_REPORT_GENERAL 0x0
#define SMP_FN_REPORT_MANUFACTURER 0x1
#define SMP_FN_READ_GPIO_REG 0x2
#define SMP_FN_REPORT_ZONE_PERMISSION 0x3
#define SMP_FN_DISCOVER 0x10
#define SMP_FN_REPORT_PHY_ERR_LOG 0x11
#define SMP_FN_REPORT_PHY_SATA 0x12
#define SMP_FN_REPORT_ROUTE_INFO 0x13
#define SMP_FN_REPORT_PHY_EVENT_INFO 0x14
#define SMP_FN_REPORT_PHY_BROADCAST 0x15
#define SMP_FN_DISCOVER_LIST 0x16
#define SMP_FN_CONFIG_GENERAL 0x80
#define SMP_FN_WRITE_GPIO_REG 0x82
#define SMP_FN_CONFIG_ZONE_PERMISSION 0x83
#define SMP_FN_ZONED_BROADCAST 0x85
#define SMP_FN_CONFIG_ROUTE_INFO 0x90
#define SMP_FN_PHY_CONTROL 0x91
#define SMP_FN_PHY_TEST_FUNCTION 0x92
#define SMP_FN_CONFIG_PHY_EVENT_INFO 0x93

#define SMP_FRES_FUNCTION_ACCEPTED 0x0
#define SMP_FRES_UNKNOWN_FUNCTION 0x1
#define SMP_FRES_FUNCTION_FAILED 0x2
#define SMP_FRES_INVALID_REQUEST_LEN 0x3
#define SMP_FRES_NO_PHY 0x10
#define SMP_FRES_NO_INDEX 0x11
#define SMP_FRES_NO_SATA_SUPPORT 0x12
#define SMP_FRES_UNKNOWN_PHY_OP 0x13
#define SMP_FRES_UNKNOWN_PHY_TEST_FN 0x14
#define SMP_FRES_PHY_TEST_IN_PROGRESS 0x15
#define SMP_FRES_PHY_VACANT 0x16
#define SMP_FRES_PHY_EVENT_INFO_SRC 0x17
#define SMP_FRES_ZONE_VIOLATION 0x20


#define SMP_MAX_DEVICE_NAME 256

struct smp_target_obj {
    char device_name[SMP_MAX_DEVICE_NAME];
    int subvalue;                       /* adapter number (opt) */
    unsigned char sas_addr[8];          /* target SMP (opt) */
    int interface_selector;
    int opened;
    int fd;
};

/* SAS standards include a 4 byte CRC at the end of each SMP request
   and response (why ??). All current pass throughs calculate and check
   the CRC in the driver, but some pass through want the space allocated.
 */
struct smp_req_resp {
    int request_len;            /* [i] in bytes, includes space for 4 byte
                                       CRC */
    unsigned char * request;    /* [*i], includes space for CRC */
    int max_response_len;       /* [i] in bytes, includes space for CRC */
    unsigned char * response;   /* [*o] */
    int act_response_len;       /* [o] -1 implies don't know */
    int transport_err;          /* [o] 0 implies no error */
};

extern int smp_initiator_open(const char * device_name, int subvalue,
                              const char * i_params, unsigned long long sa,
                              struct smp_target_obj * tobj, int verbose);

extern int smp_send_req(const struct smp_target_obj * tobj,
                        struct smp_req_resp * rresp, int verbose);

extern int smp_initiator_close(struct smp_target_obj * tobj);

extern char * smp_get_func_res_str(int func_res, int buff_len, char * buff);

extern int smp_get_func_def_req_len(int func_code);

extern int smp_get_func_def_resp_len(int func_code);

extern int smp_is_naa5(unsigned long long addr);

extern const char * smp_lib_version();

struct smp_val_name {
    int value;
    char * name;
};

/* <<< General purpose (i.e. not SMP specific) utility functions >>> */

/* Always returns valid string even if errnum is wild (or library problem).
   If errnum is negative, flip its sign. */
extern char * safe_strerror(int errnum);


/* Print (to stdout) 'str' of bytes in hex, 16 bytes per line optionally
   followed at the right hand side of the line with an ASCII interpretation.
   Each line is prefixed with an address, starting at 0 for str[0]..str[15].
   All output numbers are in hex. 'no_ascii' allows for 3 output types:
       > 0     each line has address then up to 16 ASCII-hex bytes
       = 0     in addition, the bytes are listed in ASCII to the right
       < 0     only the ASCII-hex bytes are listed (i.e. without address)
*/
extern void dStrHex(const char* str, int len, int no_ascii);

/* If the number in 'buf' can not be decoded or the multiplier is unknown
   then -1 is returned. Accepts a hex prefix (0x or 0X) or a 'h' (or 'H')
   suffix. Otherwise a decimal multiplier suffix may be given. Recognised
   multipliers: c C  *1;  w W  *2; b  B *512;  k K KiB  *1,024;
   KB  *1,000;  m M MiB  *1,048,576; MB *1,000,000; g G GiB *1,073,741,824;
   GB *1,000,000,000 and <n>x<m> which multiplies <n> by <m> . */
extern int smp_get_num(const char * buf);

/* If the number in 'buf' can not be decoded or the multiplier is unknown
   then -1LL is returned. Accepts a hex prefix (0x or 0X) or a 'h' (or 'H')
   suffix. Otherwise a decimal multiplier suffix may be given. In addition
   to supporting the multipliers of sg_get_num(), this function supports:
   t T TiB  *(2**40); TB *(10**12); p P PiB  *(2**50); PB  *(10**15) . */
extern long long smp_get_llnum(const char * buf);

#endif
