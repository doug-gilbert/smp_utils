/*
 * Copyright (c) 2006-2007 Douglas Gilbert.
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

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "smp_lib.h"

/* This is a Serial Attached SCSI (SAS) management protocol (SMP) utility
 * program.
 *
 * This utility issues a DISCOVER LIST function and outputs its response.
 */

static char * version_str = "1.06 20071001";    /* sas2r12 */


#define SMP_UTILS_TEST

#ifdef SMP_UTILS_TEST
static unsigned char tst1_resp[] = {
    0x41, 0x20, 0, 41, 0, 0, 0, 0, 0x0, 5, 0, 1,
    24, 0, 0, 0, 0x1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0x0, 0, 0x0, 0x0, 0x0, 0x0, 2, 0, 0, 0, 0, 0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0, 0, 0, 0,

    0x1, 0, 0x11, 0x9, 0x0, 0x8, 2, 0x20, 0, 0, 7, 13,
    0x51, 0x11, 0x22, 0x33, 0x39, 0x88, 0x77, 0x66,
    0, 0, 0, 0,

    0x2, 0, 0x20, 0x9, 0x0, 0x2, 2, 0, 0, 0, 4, 31,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x33,
    0, 0, 0, 0,

    0x3, 0, 0x20, 0x9, 0x0, 0x2, 2, 0, 0, 0, 5, 37,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x33,
    0, 0, 0, 0,

    0x4, 0, 0x10, 0x8, 0x0, 0x1, 2, 0, 0, 0, 6, 7,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x38,
    0, 0, 0, 0,

    0, 0, 0, 0};

static unsigned char tst2_resp[] = {
    0x41, 0x20, 0, 23, 0, 0, 0, 0, 0x2, 2, 0, 1,
    24, 0, 0, 0, 0x1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0x2, 0, 0x20, 0x9, 0x0, 0x2, 2, 0, 0, 0, 4, 31,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x33,
    0, 0, 0, 0,

    0x3, 0, 0x20, 0x9, 0x0, 0x2, 2, 0, 0, 0, 5, 37,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x33,
    0, 0, 0, 0,

    0, 0, 0, 0};

static unsigned char tst3_resp[] = {
    0x41, 0x20, 0, 23, 0, 0, 0, 0, 0x0, 2, 0, 1,
    24, 0, 0, 0, 0x1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0x2, 0x16, 0x20, 0x9, 0x0, 0x2, 2, 0, 0, 0, 4, 31,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x33,
    0, 0, 0, 0,

    0x3, 0, 0x20, 0x9, 0x0, 0x2, 2, 0, 0, 0, 5, 37,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x33,
    0, 0, 0, 0,

    0, 0, 0, 0};

static unsigned char tst4_resp[] = {
    0x41, 0x20, 0, 49, 0, 0, 0, 0, 0x0, 2, 0, 0,
    76, 0, 0, 0, 0x1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0x41, 0x10, 0x0, 18, 0, 0, 0, 0,
    0, 0x1, 0, 0, 0x10, 0x9, 0x0, 0x8,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x22,
    0x51, 0x11, 0x22, 0x33, 0x39, 0x88, 0x77, 0x66,
    0x7, 0x0, 0, 0, 0, 0, 0, 0,
    0x88, 0x99, 13, 0x87, 0x2, 0x10, 1, 2,
    0, 0, 0, 0,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x20,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0x41, 0x10, 0x0, 18, 0, 0, 0, 0,
    0, 0x2, 0, 0, 0x20, 0x9, 0x0, 0x2,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x22,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x33,
    0x4, 0x0, 0, 0, 0, 0, 0, 0,
    0x88, 0x99, 31, 0x7, 0x2, 0x2, 3, 4,
    0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0};

static unsigned char tst5_resp[] = {
    0x41, 0x20, 0, 59, 0, 0, 0, 0, 0x0, 2, 0, 0,
    96, 0, 0, 0, 0x1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0x41, 0x10, 0x0, 0x17, 0, 0, 0, 0,
    0, 0x0, 0, 0, 0x11, 0x9, 0x0, 0x8,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x22,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x33,
    0x7, 0x0, 0, 0, 0, 0, 0, 0,
    0x88, 0x99, 13, 0x87, 0x2, 0x21, 2, 3,
    0, 0, 0, 0,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x20,
    0x77, 0, 0, 0, 0, 0, 0, 0,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x22, 0x99,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2a, 0x1,

    0x41, 0x10, 0x0, 0x17, 0, 0, 0, 0,
    0, 0x1, 0, 0, 0x21, 0x9, 0x0, 0x2,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x27, 0x22,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x27, 0x33,
    0x7, 0x0, 0, 0, 0, 0, 0, 0,
    0x88, 0x99, 13, 0x87, 0x2, 0x21, 2, 3,
    0, 0, 0, 0,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x27, 0x20,
    0x77, 0, 0, 0, 0, 0, 0, 0,
    0x50, 0x0, 0x33, 0x44, 0x41, 0x11, 0x27, 0x99,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2a, 0x1,

    0, 0, 0, 0};

#endif



static struct option long_options[] = {
        {"brief", 0, 0, 'b'},
        {"descriptor", 1, 0, 'd'},
        {"filter", 1, 0, 'f'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"ignore", 0, 0, 'i'},
        {"interface", 1, 0, 'I'},
        {"list", 0, 0, 'l'},
        {"num", 1, 0, 'n'},
        {"one", 0, 0, 'o'},
        {"phy", 1, 0, 'p'},
        {"sa", 1, 0, 's'},
        {"raw", 0, 0, 'r'},
#ifdef SMP_UTILS_TEST
        {"test", 1, 0, 't'},
#endif
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

struct opts_t {
    int do_brief;
    int desc_type;
    int filter;
    int do_list;
    int do_hex;
    int ign_zp;
    int do_num;
    int do_one;
    int phy_id;
    int do_raw;
    int do_test;
    int verbose;
    int sa_given;
    unsigned long long sa;
};

static void
usage()
{
    fprintf(stderr, "Usage: "
          "smp_discover_list  [--brief] [--descriptor=TY] [--filter=FI] "
          "[--help]\n"
          "                    [--hex] [--ignore] [--interface=PARAMS] "
          "[--list]\n"
          "                    [--num=NUM] [--one] [--phy=ID] [--raw] "
          "[--sa=SAS_ADDR]\n");
#ifdef SMP_UTILS_TEST
    fprintf(stderr,
          "                    [--test=TE] [--verbose] [--version] "
          "<smp_device>[,<n>]\n");
#else
    fprintf(stderr,
          "                    [--verbose] [--version] "
          "<smp_device>[,<n>]\n");
#endif
    fprintf(stderr,
          "  where:\n"
          "    --brief|-b           brief: abridge output\n"
          "    --descriptor=TY|-d TY    descriptor type\n"
          "                         0 -> long (as in DISCOVER); 1 -> "
          "short (24 byte)\n"
          "    --filter=FI|-f FI    phy filter: 0 -> all (def); 1 -> "
          "expander\n"
          "                         attached; 2 -> expander "
          "or end device\n"
          "    --help|-h            print out usage message\n"
          "    --hex|-H             print response in hexadecimal\n"
          "    --ignore|-i          sets the Ignore Zone Group bit\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --list|-l            output attribute=value, 1 per line\n"
          "    --num=NUM|-n NUM     maximum number of descriptors to fetch "
          "(def: 1)\n"
          "    --one|-o             one line output per response "
          "descriptor\n"
          "    --phy=ID|-p ID       phy identifier (def: 0) [starting "
          "phy id]\n"
          "    --raw|-r             output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading '0x'\n"
          "                         or trailing 'h'). Depending on "
          "the interface, may\n"
          "                         not be needed\n");
#ifdef SMP_UTILS_TEST
    fprintf(stderr,
          "    --test=TE|-t TE      test responses (def: 0 (non-test "
          "mode))\n");
#endif
    fprintf(stderr,
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n\n"
          "Performs a SMP DISCOVER LIST function\n"
          );

}

static void
dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

static char * smp_attached_device_type[] = {
    "no device attached",
    "end device",
    "expander device",
    "expander device (fanout)",
    "reserved [4]",
    "reserved [5]",
    "reserved [6]",
    "reserved [7]",
};

static char * smp_short_attached_device_type[] = {
    "",         /* was "no " */
    "",         /* was "end" */
    "exp",
    "fex",     /* obsolete in sas2r05a */
    "res",
    "res",
    "res",
    "res",
};

static char *
smp_get_plink_rate(int val, int prog, int b_len, char * b)
{
    switch (val) {
    case 0:
        snprintf(b, b_len, "not programmable");
        break;
    case 8:
        snprintf(b, b_len, "1.5 Gbps");
        break;
    case 9:
        snprintf(b, b_len, "3 Gbps");
        break;
    case 0xa:
        snprintf(b, b_len, "6 Gbps");
        break;
    default:
        if (prog && (0 == val))
            snprintf(b, b_len, "not programmable");
        else
            snprintf(b, b_len, "reserved [%d]", val);
        break;
    }
    return b;
}

static char *
smp_get_reason(int val, int b_len, char * b)
{
    switch (val) {
    case 0: snprintf(b, b_len, "unknown"); break;
    case 1: snprintf(b, b_len, "power on"); break;
    case 2: snprintf(b, b_len, "hard reset");
         break;
    case 3: snprintf(b, b_len, "SMP phy control requested");
         break;
    case 4: snprintf(b, b_len, "loss of dword synchronization"); break;
    case 5: snprintf(b, b_len, "error in multiplexing sequence"); break;
    case 6: snprintf(b, b_len, "I_T nexus loss timeout STP/SATA"); break;
    case 7: snprintf(b, b_len, "break timeout timer expired"); break;
    case 8: snprintf(b, b_len, "phy test function stopped"); break;
    case 9: snprintf(b, b_len, "expander reduced functionality"); break;
    default: snprintf(b, b_len, "reserved [%d]", val); break;
    }
    return b;
}

static char *
smp_get_neg_xxx_link_rate(int val, int b_len, char * b)
{
    switch (val) {
    case 0: snprintf(b, b_len, "phy enabled; unknown"); break;
    case 1: snprintf(b, b_len, "phy disabled"); break;
    case 2: snprintf(b, b_len, "phy enabled; speed negotiation failed");
         break;
    case 3: snprintf(b, b_len, "phy enabled; SATA spinup hold state");
        break;
    case 4: snprintf(b, b_len, "phy enabled; port selector"); break;
    case 5: snprintf(b, b_len, "phy enabled; reset in progress"); break;
    case 6: snprintf(b, b_len, "phy enabled; unsupported phy attached");
        break;
    case 8: snprintf(b, b_len, "phy enabled; 1.5 Gbps"); break;
    case 9: snprintf(b, b_len, "phy enabled; 3 Gbps"); break;
    case 0xa: snprintf(b, b_len, "phy enabled; 6 Gbps"); break;
    default: snprintf(b, b_len, "reserved [%d]", val); break;
    }
    return b;
}

static char *
find_sas_connector_type(int conn_type, char * buff, int buff_len)
{
    switch (conn_type) {
    case 0x0:
        snprintf(buff, buff_len, "No information");
        break;
    case 0x1:
        snprintf(buff, buff_len, "SAS 4x receptacle (SFF-8470) "
                 "[max 4 phys]");
        break;
    case 0x2:
        snprintf(buff, buff_len, "Mini SAS 4x receptacle (SFF-8088) "
                 "[max 4 phys]");
        break;
    case 0xf:
        snprintf(buff, buff_len, "Vendor specific external connector");
        break;
    case 0x10:
        snprintf(buff, buff_len, "SAS 4i plug (SFF-8484) [max 4 phys]");
        break;
    case 0x11:
        snprintf(buff, buff_len, "Mini SAS 4i receptacle (SFF-8087) "
                 "[max 4 phys]");
        break;
    case 0x20:
        snprintf(buff, buff_len, "SAS Drive backplane receptacle (SFF-8482) "
                 "[max 2 phys]");
        break;
    case 0x21:
        snprintf(buff, buff_len, "SATA host plug [max 1 phy]");
        break;
    case 0x22:
        snprintf(buff, buff_len, "SAS Drive plug (SFF-8482) [max 2 phys]");
        break;
    case 0x23:
        snprintf(buff, buff_len, "SATA device plug [max 1 phy]");
        break;
    case 0x2f:
        snprintf(buff, buff_len, "SAS virtual connector [max 1 phy]");
        break;
    case 0x3f:
        snprintf(buff, buff_len, "Vendor specific internal connector");
        break;
    default:
        if (conn_type < 0x10)
            snprintf(buff, buff_len, "unknown external connector type: 0x%x",
                     conn_type);
        else if (conn_type < 0x20)
            snprintf(buff, buff_len, "unknown internal wide connector type: "
                     "0x%x", conn_type);
        else if (conn_type < 0x30)
            snprintf(buff, buff_len, "unknown internal connector to end "
                     "device, type: 0x%x", conn_type);
        else if (conn_type < 0x70)
            snprintf(buff, buff_len, "reserved connector type: 0x%x",
                     conn_type);
        else if (conn_type < 0x80)
            snprintf(buff, buff_len, "vendor specific connector type: 0x%x",
                     conn_type);
        else    /* conn_type is a 7 bit field, so this is impossible */
            snprintf(buff, buff_len, "unexpected connector type: 0x%x",
                     conn_type);
        break;
    }
    return buff;
}

/* Returns 0 when successful, -1 for low level errors and > 0
   for other error categories. */
static int
do_discover_list(struct smp_target_obj * top, unsigned char * resp,
                 int max_resp_len, struct opts_t * optsp)
{
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_DISCOVER_LIST, 0, 6,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, };
    struct smp_req_resp smp_rr;
    char b[256];
    char * cp;
    int len, res, k;

    smp_req[8] = optsp->phy_id;
    smp_req[9] = optsp->do_num;
    smp_req[10] = optsp->filter & 0xf;
    if (optsp->ign_zp)
        smp_req[10] |= 0x80;
    smp_req[11] = optsp->desc_type & 0xf;
    if (optsp->verbose) {
        fprintf(stderr, "    Discover list request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            fprintf(stderr, "%02x ", smp_req[k]);
        fprintf(stderr, "\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = max_resp_len;
    smp_rr.response = resp;
    if (0 == optsp->do_test)
        res = smp_send_req(top, &smp_rr, optsp->verbose);
    else {
#ifdef SMP_UTILS_TEST
        memset(resp, 0, max_resp_len);
        if (1 == optsp->do_test)
            memcpy(resp, tst1_resp, sizeof(tst1_resp));
        else if (2 == optsp->do_test)
            memcpy(resp, tst2_resp, sizeof(tst2_resp));
        else if (3 == optsp->do_test)
            memcpy(resp, tst3_resp, sizeof(tst3_resp));
        else if (4 == optsp->do_test)
            memcpy(resp, tst4_resp, sizeof(tst4_resp));
        else if (5 == optsp->do_test)
            memcpy(resp, tst5_resp, sizeof(tst5_resp));
        else
            fprintf(stderr, ">>> test %d not supported\n", optsp->do_test);
#else
        fprintf(stderr, ">>> test %d not supported\n", optsp->do_test);
#endif
        smp_rr.act_response_len = -1;
        res = 0;
    }

    if (res) {
        fprintf(stderr, "smp_send_req failed, res=%d\n", res);
        if (0 == optsp->verbose)
            fprintf(stderr, "    try adding '-v' option for more debug\n");
        return -1;
    }
    if (smp_rr.transport_err) {
        fprintf(stderr, "smp_send_req transport_error=%d\n",
                smp_rr.transport_err);
        return -1;
    }
    if ((smp_rr.act_response_len >= 0) && (smp_rr.act_response_len < 4)) {
        fprintf(stderr, "response too short, len=%d\n",
                smp_rr.act_response_len);
        return SMP_LIB_CAT_MALFORMED;
    }
    len = resp[3];
    if (0 == len) {
        len = smp_get_func_def_resp_len(resp[1]);
        if (len < 0) {
            len = 0;
            if (optsp->verbose > 0)
                fprintf(stderr, "unable to determine response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if (optsp->do_hex || optsp->do_raw) {
        if (optsp->do_hex)
            dStrHex((const char *)resp, len, 1);
        else
            dStrRaw((const char *)resp, len);
        if (SMP_FRAME_TYPE_RESP != resp[0])
            return SMP_LIB_CAT_MALFORMED;
        if (resp[1] != smp_req[1])
            return SMP_LIB_CAT_MALFORMED;
        if (resp[2])
            return resp[2];
        return 0;
    }
    if (SMP_FRAME_TYPE_RESP != resp[0]) {
        fprintf(stderr, "expected SMP frame response type, got=0x%x\n",
                resp[0]);
        return SMP_LIB_CAT_MALFORMED;
    }
    if (resp[1] != smp_req[1]) {
        fprintf(stderr, "Expected function code=0x%x, got=0x%x\n",
                smp_req[1], resp[1]);
        return SMP_LIB_CAT_MALFORMED;
    }
    if (resp[2]) {
        cp = smp_get_func_res_str(resp[2], sizeof(b), b);
        fprintf(stderr, "Discover list result: %s\n", cp);
        return resp[2];
    }
    return 0;
}

/* long format: similar to DISCOVER response */
static int
decode_desc0_multiline(const unsigned char * resp, int offset,
                       int hdr_ecc, struct opts_t * optsp)
{
    const unsigned char *rp;
    unsigned long long ull;
    int func_res, phy_id, ecc, adt, route_attr, len, j;
    char b[256];

    rp = resp + offset;
    phy_id = rp[9];
    func_res = rp[2];
    len = 4 + (rp[3] * 4);        /* length in bytes, excluding 4 byte CRC */
    printf("  phy identifier: %d\n", phy_id);
    if (func_res) {
        printf("  >>> function result: %s\n",
               smp_get_func_res_str(func_res, sizeof(b), b));
        return -1;
    }
    ecc = (rp[4] << 8) + rp[5];
    if ((0 != ecc) && (hdr_ecc != ecc))
        printf("  >>> expander change counts differ, header: %d, this phy: "
        "%d\n", hdr_ecc, ecc);
    adt = ((0x70 & rp[12]) >> 4);
    if (adt < 8)
        printf("  attached device type: %s\n", smp_attached_device_type[adt]);
    if ((optsp->do_brief > 1) && (0 == adt))
        return 0;
    if (0 == optsp->do_brief)
        printf("  attached reason: %s\n",
               smp_get_reason(0xf & rp[12], sizeof(b), b));

    printf("  negotiated logical link rate: %s\n",
           smp_get_neg_xxx_link_rate(0xf & rp[13], sizeof(b), b));
    printf("  attached initiator: ssp=%d stp=%d smp=%d sata_host=%d\n",
           !!(rp[14] & 8), !!(rp[14] & 4), !!(rp[14] & 2), (rp[14] & 1));
    if (0 == optsp->do_brief)
        printf("  attached sata port selector: %d\n", !!(rp[15] & 0x80));
    printf("  attached target: ssp=%d stp=%d smp=%d sata_device=%d\n",
           !!(rp[15] & 8), !!(rp[15] & 4), !!(rp[15] & 2), (rp[15] & 1));

    ull = 0;
    for (j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= rp[16 + j];
    }
    printf("  SAS address: 0x%llx\n", ull);
    ull = 0;
    for (j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= rp[24 + j];
    }
    printf("  attached SAS address: 0x%llx\n", ull);
    printf("  attached phy identifier: %d\n", rp[32]);
    if (0 == optsp->do_brief) {
        printf("  attached inside ZPSDS persistent: %d\n", rp[33] & 4);
        printf("  attached requested inside ZPSDS: %d\n", rp[33] & 2);
        printf("  attached break_reply capable: %d\n", rp[33] & 1);
        printf("  programmed minimum physical link rate: %s\n",
               smp_get_plink_rate(((rp[40] >> 4) & 0xf), 1,
                                  sizeof(b), b));
        printf("  hardware minimum physical link rate: %s\n",
               smp_get_plink_rate((rp[40] & 0xf), 0, sizeof(b), b));
        printf("  programmed maximum physical link rate: %s\n",
               smp_get_plink_rate(((rp[41] >> 4) & 0xf), 1,
                                  sizeof(b), b));
        printf("  hardware maximum physical link rate: %s\n",
               smp_get_plink_rate((rp[41] & 0xf), 0,
                                  sizeof(b), b));
        printf("  phy change count: %d\n", rp[42]);
        printf("  virtual phy: %d\n", !!(rp[43] & 0x80));
        printf("  partial pathway timeout value: %d us\n",
               (rp[43] & 0xf));
    }
    route_attr = (0xf & rp[44]);
    switch (route_attr) {
    case 0: snprintf(b, sizeof(b), "direct"); break;
    case 1: snprintf(b, sizeof(b), "subtractive"); break;
    case 2: snprintf(b, sizeof(b), "table"); break;
    default: snprintf(b, sizeof(b), "reserved [%d]", route_attr); break;
    }
    printf("  routing attribute: %s\n", b);
    if (optsp->do_brief)
        return 0;
    printf("  connector type: %s\n",
           find_sas_connector_type((rp[45] & 0x7f), b, sizeof(b)));
    printf("  connector element index: %d\n", rp[46]);
    printf("  connector physical link: %d\n", rp[47]);
    if (len > 59) {
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= rp[52 + j];
        }
        printf("  attached device name: 0x%llx\n", ull);
        printf("  requested inside ZPSDS changed by expander: %d\n",
               !!(rp[60] & 0x40));
        printf("  inside ZPSDS persistent: %d\n", !!(rp[60] & 0x20));
        printf("  requested inside ZPSDS: %d\n", !!(rp[60] & 0x10));
        /* printf("  zone address resolved: %d\n", !!(rp[60] & 0x8)); */
        printf("  zone group persistent: %d\n", !!(rp[60] & 0x4));
        printf("  inside ZPSDS: %d\n", !!(rp[60] & 0x2));
        printf("  zoning enabled: %d\n", !!(rp[60] & 0x1));
        printf("  zone group: %d\n", rp[63]);
        if (len < 76)
            return 0;
        printf("  self-configuration status: %d\n", rp[64]);
        printf("  self-configuration level completed: %d\n", rp[65]);
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= rp[68 + j];
        }
        printf("  self-configuration sas address: 0x%llx\n", ull);
    }
    if (len > 95) {
        printf("  reason: %s\n",
               smp_get_reason((0xf0 & rp[94]) >> 4, sizeof(b), b));
        printf("  negotiated physical link rate: %s\n",
               smp_get_neg_xxx_link_rate(0xf & rp[94], sizeof(b), b));
        printf("  hardware muxing supported: %d\n", !!(rp[95] & 0x1));
    }
    if (len > 107) {
        printf("  default inside ZPSDS persistent: %d\n", !!(rp[96] & 0x20));
        printf("  default requested inside ZPSDS: %d\n", !!(rp[96] & 0x10));
        printf("  default zone group persistent: %d\n", !!(rp[96] & 0x4));
        printf("  default zoning enabled: %d\n", !!(rp[96] & 0x1));
        printf("  default zone group: %d\n", rp[99]);
        printf("  saved inside ZPSDS persistent: %d\n", !!(rp[100] & 0x20));
        printf("  saved requested inside ZPSDS: %d\n", !!(rp[100] & 0x10));
        printf("  saved zone group persistent: %d\n", !!(rp[100] & 0x4));
        printf("  saved zoning enabled: %d\n", !!(rp[100] & 0x1));
        printf("  saved zone group: %d\n", rp[103]);
        printf("  shadow inside ZPSDS persistent: %d\n", !!(rp[104] & 0x20));
        printf("  shadow requested inside ZPSDS: %d\n", !!(rp[104] & 0x10));
        printf("  shadow zone group persistent: %d\n", !!(rp[104] & 0x4));
        printf("  shadow zone group: %d\n", rp[107]);
    }
    return 0;
}

/* short format: parts if DISCOVER response compressed into 24 bytes */
static int
decode_desc1_multiline(const unsigned char * resp, int offset,
                       struct opts_t * optsp)
{
    const unsigned char *rp;
    unsigned long long ull;
    int func_res, phy_id, adt, route_attr, j;
    char b[256];

    rp = resp + offset;
    phy_id = rp[0];
    func_res = rp[1];
    printf("  phy identifier: %d\n", phy_id);
    if (func_res) {
        printf("  >>> function result: %s\n",
               smp_get_func_res_str(func_res, sizeof(b), b));
        return -1;
    }
    adt = ((0x70 & rp[2]) >> 4);
    if (adt < 8)
        printf("  attached device type: %s\n", smp_attached_device_type[adt]);
    if ((optsp->do_brief > 1) && (0 == adt))
        return 0;
    if (0 == optsp->do_brief)
        printf("  attached reason: %s\n",
               smp_get_reason(0xf & rp[2], sizeof(b), b));
    printf("  negotiated logical link rate: %s\n",
           smp_get_neg_xxx_link_rate(0xf & rp[3], sizeof(b), b));

    printf("  attached initiator: ssp=%d stp=%d smp=%d sata_host=%d\n",
           !!(rp[4] & 8), !!(rp[4] & 4), !!(rp[4] & 2), (rp[4] & 1));
    if (0 == optsp->do_brief)
        printf("  attached sata port selector: %d\n", !!(rp[5] & 0x80));
    printf("  attached target: ssp=%d stp=%d smp=%d sata_device=%d\n",
           !!(rp[5] & 8), !!(rp[5] & 4), !!(rp[5] & 2), (rp[5] & 1));

    if (0 == optsp->do_brief)
        printf("  virtual phy: %d\n", !!(rp[6] & 0x80));
    ull = 0;
    for (j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= rp[12 + j];
    }
    printf("  attached SAS address: 0x%llx\n", ull);
    printf("  attached phy identifier: %d\n", rp[10]);
    if (0 == optsp->do_brief)
        printf("  phy change count: %d\n", rp[11]);
    route_attr = (0xf & rp[6]);
    switch (route_attr) {
    case 0: snprintf(b, sizeof(b), "direct"); break;
    case 1: snprintf(b, sizeof(b), "subtractive"); break;
    case 2: snprintf(b, sizeof(b), "table"); break;
    default: snprintf(b, sizeof(b), "reserved [%d]", route_attr); break;
    }
    printf("  routing attribute: %s\n", b);
    if (optsp->do_brief)
        return 0;
    printf("  reason: %s\n", smp_get_reason(0xf & (rp[7] >> 4),
                                            sizeof(b), b));
    printf("  zone group: %d\n", rp[8]);
    printf("  inside ZPSDS persistent: %d\n", !!(rp[9] & 0x20));
    printf("  requested inside ZPSDS: %d\n", !!(rp[9] & 0x10));
    /* printf("  zone address resolved: %d\n", !!(rp[9] & 0x8)); */
    printf("  zone group persistent: %d\n", !!(rp[9] & 0x4));
    printf("  inside ZPSDS: %d\n", !!(rp[9] & 0x2));
    return 0;
}

static int
decode_1line(const unsigned char * resp, int offset, int desc)
{
    const unsigned char *rp;
    unsigned long long ull;
    int phy_id, j, off, plus, negot, adt, route_attr, vp, asa_off;
    int func_res, aphy_id, a_init, a_target;
    char b[256];
    const char * cp;

    rp = resp + offset;
    switch (desc) {
    case 0:
        phy_id = rp[9];
        func_res = rp[2];
        adt = ((0x70 & rp[12]) >> 4);
        negot = rp[13] & 0xf;
        route_attr = rp[44] & 0xf;
        vp = rp[43] & 0x80;
        asa_off = 24;
        aphy_id = rp[32];
        a_init = rp[14];
        a_target = rp[15];
        break;
    case 1:
        phy_id = rp[0];
        func_res = rp[1];
        adt = ((0x70 & rp[2]) >> 4);
        negot = rp[3] & 0xf;
        route_attr = rp[6] & 0xf;
        vp = !!(rp[6] & 0x80);
        asa_off = 12;
        aphy_id = rp[10];
        a_init = rp[4];
        a_target = rp[5];
        break;
    default:
        printf("  Unknown descriptor type %d\n", desc);
        return -1;
    }
    if (func_res) {
        printf("  phy %3d: function result: %s\n", phy_id,
               smp_get_func_res_str(func_res, sizeof(b), b));
        return -1;
    }
    switch (route_attr) {
    case 0:
        cp = "D";
        break;
    case 1:
        cp = "S";
        break;
    case 2:
        cp = "T";
        break;
    default:
        cp = "R";
        break;
    }
    if (1 == negot) {
        printf("  phy %3d:%s:disabled\n", phy_id, cp);
        return 0;
    } else if (2 == negot) {
        printf("  phy %3d:%s:reset problem\n", phy_id, cp);
        return 0;
    } else if (3 == negot) {
        printf("  phy %3d:%s:spinup hold\n", phy_id, cp);
        return 0;
    } else if (4 == negot) {
        printf("  phy %3d:%s:port selector\n", phy_id, cp);
        return 0;
    } else if (5 == negot) {
        printf("  phy %3d:%s:reset in progress\n", phy_id, cp);
        return 0;
    }
    ull = 0;
    for (j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= rp[asa_off + j];
    }
    if ((0 == adt) || (adt > 3)) {
        printf("  phy %3d:%s:attached:[0000000000000000:00]\n", phy_id, cp);
        return 0;
    }
    printf("  phy %3d:%s:attached:[%016llx:%02d %s%s", phy_id, cp, ull,
           aphy_id, smp_short_attached_device_type[adt], (vp ? " V" : ""));
    if (a_init & 0xf) {
        off = 0;
        plus = 0;
        off += snprintf(b + off, sizeof(b) - off, " i(");
        if (a_init & 0x8) {
            off += snprintf(b + off, sizeof(b) - off, "SSP");
            ++plus;
        }
        if (a_init & 0x4) {
            off += snprintf(b + off, sizeof(b) - off, "%sSTP",
                            (plus ? "+" : ""));
            ++plus;
        }
        if (a_init & 0x2) {
            off += snprintf(b + off, sizeof(b) - off, "%sSMP",
                            (plus ? "+" : ""));
            ++plus;
        }
        if (a_init & 0x1) {
            off += snprintf(b + off, sizeof(b) - off, "%sSATA",
                            (plus ? "+" : ""));
            ++plus;
        }
        printf("%s)", b);
    }
    if (a_target & 0xf) {
        off = 0;
        plus = 0;
        off += snprintf(b + off, sizeof(b) - off, " t(");
        if (a_target & 0x80) {
            off += snprintf(b + off, sizeof(b) - off, "PORT_SEL");
            ++plus;
        }
        if (a_target & 0x8) {
            off += snprintf(b + off, sizeof(b) - off, "%sSSP",
                            (plus ? "+" : ""));
            ++plus;
        }
        if (a_target & 0x4) {
            off += snprintf(b + off, sizeof(b) - off, "%sSTP",
                            (plus ? "+" : ""));
            ++plus;
        }
        if (a_target & 0x2) {
            off += snprintf(b + off, sizeof(b) - off, "%sSMP",
                            (plus ? "+" : ""));
            ++plus;
        }
        if (a_target & 0x1) {
            off += snprintf(b + off, sizeof(b) - off, "%sSATA",
                            (plus ? "+" : ""));
            ++plus;
        }
        printf("%s)", b);
    }
    printf("]");
    switch(negot) {
    case 8:
        cp = "  1.5 Gbps";
        break;
    case 9:
        cp = "  3 Gbps";
        break;
    case 0xa:
        cp = "  6 Gbps";
        break;
    default:
        cp = "";
        break;
    }
    printf("%s\n", cp);
    return 0;
}


int
main(int argc, char * argv[])
{
    int res, c, len, hdr_ecc, sphy_id, num_desc, resp_filter, resp_desc_type;
    int desc_len, k, err, off;
    long long sa_ll;
    char i_params[256];
    char device_name[512];
    unsigned char resp[1024];
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 0;
    struct opts_t opts;

    memset(&opts, 0, sizeof(opts));
    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "bd:f:hHiI:ln:op:rs:t:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'b':
            ++opts.do_brief;
            break;
        case 'd':
           opts.desc_type = smp_get_num(optarg);
           if ((opts.desc_type < 0) || (opts.desc_type > 15)) {
                fprintf(stderr, "bad argument to '--desc'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'f':
           opts.filter = smp_get_num(optarg);
           if ((opts.filter < 0) || (opts.filter > 15)) {
                fprintf(stderr, "bad argument to '--filter'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++opts.do_hex;
            break;
        case 'i':
            ++opts.ign_zp;
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'l':
            ++opts.do_list;
            break;
        case 'n':
           opts.do_num = smp_get_num(optarg);
           if (opts.do_num < 0) {
                fprintf(stderr, "bad argument to '--num'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'o':
            ++opts.do_one;
            break;
        case 'p':
           opts.phy_id = smp_get_num(optarg);
           if ((opts.phy_id < 0) || (opts.phy_id > 127)) {
                fprintf(stderr, "bad argument to '--phy'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'r':
            ++opts.do_raw;
            break;
        case 's':
           sa_ll = smp_get_llnum(optarg);
           if (-1LL == sa_ll) {
                fprintf(stderr, "bad argument to '--sa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            opts.sa = (unsigned long long)sa_ll;
            if (opts.sa > 0)
                ++opts.sa_given;
            break;
#ifdef SMP_UTILS_TEST
        case 't':
           opts.do_test = smp_get_num(optarg);
           if ((opts.do_test < 0) || (opts.do_test > 127)) {
                fprintf(stderr, "bad argument to '--test'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
#endif
        case 'v':
            ++opts.verbose;
            break;
        case 'V':
            fprintf(stderr, "version: %s\n", version_str);
            return 0;
        default:
            fprintf(stderr, "unrecognised switch code %c [0x%x] ??\n", c, c);
            usage();
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if (optind < argc) {
        if ('\0' == device_name[0]) {
            strncpy(device_name, argv[optind], sizeof(device_name) - 1);
            device_name[sizeof(device_name) - 1] = '\0';
            ++optind;
        }
        if (optind < argc) {
            for (; optind < argc; ++optind)
                fprintf(stderr, "Unexpected extra argument: %s\n",
                        argv[optind]);
            usage();
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if (0 == device_name[0]) {
        cp = getenv("SMP_UTILS_DEVICE");
        if (cp)
            strncpy(device_name, cp, sizeof(device_name) - 1);
        else {
            fprintf(stderr, "missing device name!\n    [Could use "
                    "environment variable SMP_UTILS_DEVICE instead]\n");
            usage();
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if ((cp = strchr(device_name, ','))) {
        *cp = '\0';
        if (1 != sscanf(cp + 1, "%d", &subvalue)) {
            fprintf(stderr, "expected number after comma in SMP_DEVICE "
                    "name\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if (0 == opts.sa) {
        cp = getenv("SMP_UTILS_SAS_ADDR");
        if (cp) {
           sa_ll = smp_get_llnum(cp);
           if (-1LL == sa_ll) {
                fprintf(stderr, "bad value in environment variable "
                        "SMP_UTILS_SAS_ADDR\n");
                fprintf(stderr, "    use 0\n");
                sa_ll = 0;
            }
            opts.sa = (unsigned long long)sa_ll;
        }
    }
    if (opts.sa > 0) {
        if (! smp_is_naa5(opts.sa)) {
            fprintf(stderr, "SAS (target) address not in naa-5 format "
                    "(may need leading '0x')\n");
            if ('\0' == i_params[0]) {
                fprintf(stderr, "    use '--interface=' to override\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
        }
    }

    if (0 == opts.do_test) {
        res = smp_initiator_open(device_name, subvalue, i_params, opts.sa,
                                 &tobj, opts.verbose);
        if (res < 0)
            return SMP_LIB_FILE_ERROR;
    }

    ret = do_discover_list(&tobj, resp, sizeof(resp), &opts);
    if (ret)
        goto finish;
    if (opts.do_hex || opts.do_raw)
        goto finish;
    len = (resp[3] * 4) + 4;    /* length in bytes excluding CRC field */
    hdr_ecc = (resp[4] << 8) + resp[5];
    sphy_id = resp[8];
    num_desc = resp[9];
    if (! opts.do_one) {
        printf("Discover list response header:\n");
        printf("  starting phy id: %d\n", sphy_id);
        printf("  number of descriptors: %d\n", num_desc);
    }
    resp_filter = resp[10] & 0xf;
    if (opts.filter != resp_filter)
        fprintf(stderr, ">>> Requested filter was %d, got %d\n", opts.filter,
                resp_filter);
    resp_desc_type = resp[11] & 0xf;
    if (opts.desc_type != resp_desc_type)
        fprintf(stderr, ">>> Requested descriptor type was %d, got %d\n",
                opts.desc_type, resp_desc_type);
    desc_len = resp[12];
    if ((! opts.do_one) && (0 == opts.do_brief)) {
        printf("  expander change count: %d\n", hdr_ecc);
        printf("  filter: %d\n", resp_filter);
        printf("  descriptor type: %d\n", resp_desc_type);
        printf("  descriptor length (each, in bytes): %d\n", desc_len);
        printf("  zoning supported: %d\n", !!(resp[16] & 0x80));
        printf("  zoning enabled: %d\n", !!(resp[16] & 0x40));
        printf("  configuring: %d\n", !!(resp[16] & 0x2));
        printf("  externally configurable route table: %d\n",
               !!(resp[16] & 0x1));
        printf("  last self-configuration status descriptor index: %d\n",
               (resp[18] << 8) + resp[19]);    /* sas2r12 */
        printf("  last phy event list descriptor index: %d\n",
               (resp[20] << 8) + resp[21]);
    }
    if (len != (48 + (num_desc * desc_len))) {
        fprintf(stderr, ">>> Response length of %d bytes doesn't match "
                "%d descriptors, each\n  of %d bytes plus a 48 byte "
                "header and 4 byte CRC\n", len + 4, num_desc, desc_len);
        if (len < (48 + (num_desc * desc_len))) {
            ret = SMP_LIB_CAT_MALFORMED;
            goto finish;
        }
    }
    for (k = 0, err = 0; k < num_desc; ++k) {
        off = 48 + (k * desc_len);
        if (opts.do_one) {
            if (decode_1line(resp, off, resp_desc_type))
                ++err;
        } else {
            printf("descriptor %d:\n", k + 1);
            if (0 == resp_desc_type) {
                if (decode_desc0_multiline(resp, off, hdr_ecc, &opts))
                    ++err;
            } else if (1 == resp_desc_type) {
                if (decode_desc1_multiline(resp, off, &opts))
                    ++err;
            } else
                ++err;
        }
    }

finish:
    if (0 == opts.do_test) {
        res = smp_initiator_close(&tobj);
        if (res < 0) {
            if (0 == ret)
                return SMP_LIB_FILE_ERROR;
        }
    }
    return (ret >= 0) ? ret : SMP_LIB_CAT_OTHER;
}