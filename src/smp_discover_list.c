/*
 * Copyright (c) 2006-2014 Douglas Gilbert.
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
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"

/* This is a Serial Attached SCSI (SAS) Serial Management Protocol (SMP)
 * utility.
 *
 * This utility issues a DISCOVER LIST function and outputs its response.
 *
 * First defined in SAS-2. From and including SAS-2.1 this function is
 * defined in the SPL series. The most recent SPL-3 draft is spl3r07.pdf .
 */

static const char * version_str = "1.35 20141007";    /* spl4r01 */

#define MAX_DLIST_SHORT_DESCS 40
#define MAX_DLIST_LONG_DESCS 8
#define SMP_FN_REPORT_GENERAL_RESP_LEN 76

static struct option long_options[] = {
        {"adn", no_argument, 0, 'A'},
        {"brief", no_argument, 0, 'b'},
        {"cap", no_argument, 0, 'c'},
        {"descriptor", required_argument, 0, 'd'},
        {"dsn", no_argument, 0, 'D'},
        {"filter", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {"hex", no_argument, 0, 'H'},
        {"ignore", no_argument, 0, 'i'},
        {"interface", required_argument, 0, 'I'},
        {"list", no_argument, 0, 'l'},    /* placeholder, not implemented */
        {"num", required_argument, 0, 'n'},
        {"one", no_argument, 0, 'o'},
        {"phy", required_argument, 0, 'p'},
        {"sa", required_argument, 0, 's'},
        {"summary", no_argument, 0, 'S'},
        {"raw", no_argument, 0, 'r'},
        {"verbose", no_argument, 0, 'v'},
        {"version", no_argument, 0, 'V'},
        {"zpi", required_argument, 0, 'Z'},
        {0, 0, 0, 0},
};

struct opts_t {
    int do_adn;
    int do_brief;
    int do_cap_phy;
    int do_dsn;
    int desc_type;              /* 0 -> full, 1 -> short format */
    int desc_type_given;
    int filter;
    int do_hex;
    int ign_zp;
    int do_num;
    int num_given;
    int do_1line;
    int phy_id;
    int phy_id_given;
    int do_raw;
    int do_summary;
    int verbose;
    int sa_given;
    unsigned long long sa;
    const char * zpi_fn;
    FILE * zpi_filep;
};


#ifdef __GNUC__
static int pr2serr(const char * fmt, ...)
        __attribute__ ((format (printf, 1, 2)));
#else
static int pr2serr(const char * fmt, ...);
#endif


static int
pr2serr(const char * fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vfprintf(stderr, fmt, args);
    va_end(args);
    return n;
}

static void
usage(void)
{
    pr2serr("Usage: "
          "smp_discover_list  [--adn] [--brief] [--cap] [--descriptor=TY]\n"
          "                          [--dsn] [--filter=FI] [--help] [--hex] "
          "[--ignore]\n"
          "                          [--interface=PARAMS] [--num=NUM] "
          "[--one]\n"
          "                          [--phy=ID] [--raw] [--sa=SAS_ADDR] "
          "[--summary]\n"
          "                          [--verbose] [--version] [--zpi=FN]\n"
          "                          <smp_device>[,<n>]\n");
    pr2serr(
          "  where:\n"
          "    --adn|-A             output attached device name in one "
          "line per\n"
          "                         phy mode (i.e. with --one)\n"
          "    --brief|-b           brief: less output, can be used "
          "multiple times\n"
          "    --cap|-c             decode phy capabilities bits\n"
          "    --descriptor=TY|-d TY    descriptor type:\n"
          "                         0 -> long (as in DISCOVER); 1 -> "
          "short (24 byte)\n"
          "                         default is 1 if --brief given, "
          "else default is 0\n"
          "    --dsn|-D             show device slot number in 1 line "
          "per phy\n"
          "                         output, if available\n"
          "    --filter=FI|-f FI    phy filter: 0 -> all (def); 1 -> "
          "expander\n"
          "                         attached; 2 -> expander "
          "or SAS device\n"
          "    --help|-h            print out usage message\n"
          "    --hex|-H             print response in hexadecimal\n"
          "    --ignore|-i          sets the Ignore Zone Group bit; "
          "will show\n"
          "                         phys otherwise hidden by zoning\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --num=NUM|-n NUM     maximum number of descriptors to fetch "
          "(def: 1)\n"
          "    --one|-o             one line output per response "
          "descriptor (phy)\n"
          "    --phy=ID|-p ID       phy identifier [or starting phy id]\n"
          "    --raw|-r             output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading\n"
          "                                 '0x' or trailing 'h'). "
          "Depending on\n"
          "                                 the interface, may not be "
          "needed\n");
    pr2serr(
          "    --summary|-S         output 1 line per active phy; "
          "typically\n"
          "                         equivalent to: '-o -d 1 -n 254 -b' .\n"
          "                         This option is assumed if '--phy=ID' "
          "not given\n"
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n"
          "    --zpi=FN|-Z FN       FN is file that zone phy information "
          "will be\n"
          "                         written to (for "
          "smp_conf_zone_phy_info)\n\n"
          "Performs one or more SMP DISCOVER LIST functions. If '--phy=ID' "
          "not given\nthen '--summary' is assumed. The '--summary' option "
          "shows the disposition\nof each active expander phy in table "
          "form.\n"
          );
}

static void
dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

/* Returns 1 if 'Table to Table Supported' bit set on REPORT GENERAL
 * respone. Returns 0 otherwise. */
static int
has_table2table_routing(struct smp_target_obj * top,
                        const struct opts_t * optsp)
{
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_GENERAL,
                               0, 0, 0, 0, 0, 0};
    struct smp_req_resp smp_rr;
    unsigned char rp[SMP_FN_REPORT_GENERAL_RESP_LEN];
    char b[256];
    char * cp;
    int len, res, k, act_resplen;

    memset(rp, 0, sizeof(rp));
    if (optsp->verbose) {
        pr2serr("    Report general request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            pr2serr("%02x ", smp_req[k]);
        pr2serr("\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = sizeof(rp);
    smp_rr.response = rp;
    res = smp_send_req(top, &smp_rr, optsp->verbose);

    if (res) {
        pr2serr("RG smp_send_req failed, res=%d\n", res);
        if (0 == optsp->verbose)
            pr2serr("    try adding '-v' option for more debug\n");
        return 0;
    }
    if (smp_rr.transport_err) {
        pr2serr("RG smp_send_req transport_error=%d\n",
                smp_rr.transport_err);
        return 0;
    }
    act_resplen = smp_rr.act_response_len;
    if ((act_resplen >= 0) && (act_resplen < 4)) {
        pr2serr("RG response too short, len=%d\n", act_resplen);
        return 0;
    }
    len = rp[3];
    if ((0 == len) && (0 == rp[2])) {
        len = smp_get_func_def_resp_len(rp[1]);
        if (len < 0) {
            len = 0;
            if (optsp->verbose > 1)
                pr2serr("unable to determine RG response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if ((act_resplen >= 0) && (len > act_resplen)) {
        if (optsp->verbose)
            pr2serr("actual RG response length [%d] less than deduced "
                    "length [%d]\n", act_resplen, len);
        len = act_resplen;
    }
    /* ignore --hex and --raw */
    if (SMP_FRAME_TYPE_RESP != rp[0]) {
        pr2serr("RG expected SMP frame response type, got=0x%x\n", rp[0]);
        return 0;
    }
    if (rp[1] != smp_req[1]) {
        pr2serr("RG Expected function code=0x%x, got=0x%x\n", smp_req[1],
                rp[1]);
        return 0;
    }
    if (rp[2]) {
        if (optsp->verbose > 1) {
            cp = smp_get_func_res_str(rp[2], sizeof(b), b);
            pr2serr("Report General result: %s\n", cp);
        }
        return 0;
    }
    return (len > 10) ? !!(0x80 & rp[10]) : 0;
}


/* Since spl4r01 these are 'attached SAS device type's */
static const char * smp_attached_device_type[] = {
    "no device attached",
    "SAS or SATA device",         /* used to be called "end device" */
    "expander device",            /* was 'edge expander' in SAS-1.1 */
    "expander device (fanout)",   /* marked as obsolete in SAS-2.0 */
    "reserved [4]",
    "reserved [5]",
    "reserved [6]",
    "reserved [7]",
};

static const char * smp_short_attached_device_type[] = {
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
    case 0xb:
        snprintf(b, b_len, "12 Gbps");
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
    case 5: snprintf(b, b_len, "error in multiplexing (MUX) sequence"); break;
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
    case 8: snprintf(b, b_len, "phy enabled, 1.5 Gbps"); break;
    case 9: snprintf(b, b_len, "phy enabled, 3 Gbps"); break;
    case 0xa: snprintf(b, b_len, "phy enabled, 6 Gbps"); break;
    case 0xb: snprintf(b, b_len, "phy enabled, 12 Gbps"); break;
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
do_discover_list(struct smp_target_obj * top, int sphy_id,
                 unsigned char * resp, int max_resp_len,
                 struct opts_t * op)
{
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_DISCOVER_LIST, 0, 6,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, };
    struct smp_req_resp smp_rr;
    char b[256];
    char * cp;
    int len, res, k, dword_resp_len, mnum_desc, act_resplen;

    dword_resp_len = (max_resp_len - 8) / 4;
    smp_req[2] = (dword_resp_len < 0x100) ? dword_resp_len : 0xff;
    smp_req[8] = sphy_id;
    mnum_desc = op->do_num;
    if ((0 == op->desc_type) && (mnum_desc > MAX_DLIST_LONG_DESCS))
        mnum_desc = MAX_DLIST_LONG_DESCS;
    if ((1 == op->desc_type) && (mnum_desc > MAX_DLIST_SHORT_DESCS))
        mnum_desc = MAX_DLIST_SHORT_DESCS;
    smp_req[9] = mnum_desc;
    smp_req[10] = op->filter & 0xf;
    if (op->ign_zp)
        smp_req[10] |= 0x80;
    smp_req[11] = op->desc_type & 0xf;
    if (op->verbose) {
        pr2serr("    Discover list request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k) {
            if (0 == (k % 16))
                pr2serr("\n      ");
            else if (0 == (k % 8))
                pr2serr(" ");
            pr2serr("%02x ", smp_req[k]);
        }
        pr2serr("\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = max_resp_len;
    smp_rr.response = resp;

    res = smp_send_req(top, &smp_rr, op->verbose);
    if (res) {
        pr2serr("smp_send_req failed, res=%d\n", res);
        if (0 == op->verbose)
            pr2serr("    try adding '-v' option for more debug\n");
        return -1;
    }
    if (smp_rr.transport_err) {
        pr2serr("smp_send_req transport_error=%d\n",
                smp_rr.transport_err);
        return -1;
    }
    act_resplen = smp_rr.act_response_len;
    if ((act_resplen >= 0) && (act_resplen < 4)) {
        pr2serr("response too short, len=%d\n", act_resplen);
        return SMP_LIB_CAT_MALFORMED;
    }
    len = resp[3];
    if ((0 == len) && (0 == resp[2])) {
        len = smp_get_func_def_resp_len(resp[1]);
        if (len < 0) {
            len = 0;
            if (op->verbose > 0)
                pr2serr("unable to determine response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if ((act_resplen >= 0) && (len > act_resplen)) {
        if (op->verbose)
            pr2serr("actual response length [%d] less than "
                    "deduced length [%d]\n", act_resplen, len);
        len = act_resplen;
    }
    if (op->do_hex || op->do_raw) {
        if (op->do_hex)
            dStrHex((const char *)resp, len, 1);
        else
            dStrRaw((const char *)resp, len);
        if (SMP_FRAME_TYPE_RESP != resp[0])
            return SMP_LIB_CAT_MALFORMED;
        if (resp[1] != smp_req[1])
            return SMP_LIB_CAT_MALFORMED;
        if (resp[2]) {
            if (op->verbose)
                pr2serr("Discover list result: %s\n",
                        smp_get_func_res_str(resp[2], sizeof(b), b));
            return resp[2];
        }
        return 0;
    }
    if (SMP_FRAME_TYPE_RESP != resp[0]) {
        pr2serr("expected SMP frame response type, got=0x%x\n", resp[0]);
        return SMP_LIB_CAT_MALFORMED;
    }
    if (resp[1] != smp_req[1]) {
        pr2serr("Expected function code=0x%x, got=0x%x\n", smp_req[1],
                resp[1]);
        return SMP_LIB_CAT_MALFORMED;
    }
    if (resp[2]) {
        if (SMP_FRES_NO_PHY != resp[2]) {
            cp = smp_get_func_res_str(resp[2], sizeof(b), b);
            pr2serr("Discover list result: %s\n", cp);
        }
        return resp[2];
    }
    return 0;
}

static const char * g_name[] = {"G1", "G2", "G3", "G4"};
static const char * g_name_long[] =
        {"G1 (1.5 Gbps)", "G2 (3 Gbps)", "G3 (6 Gbps)", "G4 (12 Gbps)"};

static void
decode_phy_cap(unsigned int p_cap, const struct opts_t * op)
{
    int g14_byte, k, skip, g, prev_nl;
    const char * cp;

    printf("    Tx SSC type: %d, Requested logical link rate: 0x%x\n",
           ((p_cap >> 30) & 0x1), ((p_cap >> 24) & 0xf));
    prev_nl = 1;
    g14_byte = (p_cap >> 16) & 0xff;
    for (skip = 0, k = 3; k >= 0; --k) {
        cp = op->verbose ? g_name_long[3 - k] : g_name[3 - k];
        g = (g14_byte >> (k * 2)) & 0x3;
        switch (g) {
        case 0:
            ++skip;
            break;
        case 1:
            printf("    %s: with SSC", cp);
            prev_nl = 0;
            break;
        case 2:
            printf("    %s: without SSC", cp);
            prev_nl = 0;
            break;
        case 3:
            printf("    %s: with+without SSC", cp);
            prev_nl = 0;
            break;
        default:
            printf("    %s: g14_byte=0x%x, k=%d", cp, g14_byte, k);
            prev_nl = 0;
            break;
        }
        if ((2 == k) && (0 == skip)) {
            printf("\n");
            skip = 2;
            prev_nl = 1;
        }
        if ((1 == k) && (skip < 2)) {
            printf("\n");
            prev_nl = 1;
        }
    }
    if (! prev_nl)
        printf("\n");
}

/* long format: as described in (full, single) DISCOVER response
 * Returns 0 for okay, else -1 . */
static int
decode_desc0_multiline(const unsigned char * rp, int hdr_ecc,
                       struct opts_t * op)
{
    unsigned long long ull;
    unsigned int ui;
    int func_res, phy_id, ecc, adt, route_attr, len, j;
    char b[256];

    phy_id = rp[9];
    func_res = rp[2];
    len = 4 + (rp[3] * 4);        /* length in bytes, excluding 4 byte CRC */
    printf("  phy identifier: %d\n", phy_id);
    if (SMP_FRES_PHY_VACANT == func_res) {
        printf("  inaccessible (phy vacant)\n");
        return 0;
    } else if (func_res) {
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
        printf("  attached SAS device type: %s\n",
               smp_attached_device_type[adt]);
    if ((op->do_brief > 1) && (0 == adt))
        return 0;
    if (0 == op->do_brief)
        printf("  attached reason: %s\n",
               smp_get_reason(0xf & rp[12], sizeof(b), b));

    printf("  negotiated logical link rate: %s\n",
           smp_get_neg_xxx_link_rate(0xf & rp[13], sizeof(b), b));
    printf("  attached initiator: ssp=%d stp=%d smp=%d sata_host=%d\n",
           !!(rp[14] & 8), !!(rp[14] & 4), !!(rp[14] & 2), (rp[14] & 1));
    if (0 == op->do_brief) {
        printf("  attached sata port selector: %d\n", !!(rp[15] & 0x80));
        printf("  STP buffer too small: %d\n", !!(rp[15] & 0x10));
    }
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
    if (0 == op->do_brief) {
        printf("  attached persistent capable: %d\n", !!(rp[33] & 0x80));
        printf("  attached power capable: %d\n", ((rp[33] >> 5) & 0x3));
        printf("  attached slumber capable: %d\n", !!(rp[33] & 0x10));
        printf("  attached partial capable: %d\n", !!(rp[33] & 0x8));
        printf("  attached inside ZPSDS persistent: %d\n", !!(rp[33] & 4));
        printf("  attached requested inside ZPSDS: %d\n", !!(rp[33] & 2));
        printf("  attached break_reply capable: %d\n", !!(rp[33] & 1));
        printf("  attached pwr_dis capable: %d\n", !!(rp[34] & 1));
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
    if (op->do_brief) {
        if ((len > 59) && (rp[60] & 0x1))
            printf("  zone group: %d\n", rp[63]);
        return 0;
    }
    printf("  connector type: %s\n",
           find_sas_connector_type((rp[45] & 0x7f), b, sizeof(b)));
    printf("  connector element index: %d\n", rp[46]);
    printf("  connector physical link: %d\n", rp[47]);
    printf("  phy power condition: %d\n", (rp[48] & 0xc0) >> 6);
    printf("  sas slumber capable: %d\n", !!(rp[48] & 0x8));
    printf("  sas partial capable: %d\n", !!(rp[48] & 0x4));
    printf("  sata slumber capable: %d\n", !!(rp[48] & 0x2));
    printf("  sata partial capable: %d\n", !!(rp[48] & 0x1));
    printf("  pwr_dis signal: %d\n", (rp[49] & 0xc0) >> 6);
    printf("  pwr_dis control capable: %d\n", (rp[49] & 0x30) >> 4);
    printf("  sas slumber enabled: %d\n", !!(rp[49] & 0x8));
    printf("  sas partial enabled: %d\n", !!(rp[49] & 0x4));
    printf("  sata slumber enabled: %d\n", !!(rp[49] & 0x2));
    printf("  sata partial enabled: %d\n", !!(rp[49] & 0x1));
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
        printf("  self-configuration levels completed: %d\n", rp[65]);
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= rp[68 + j];
        }
        printf("  self-configuration sas address: 0x%llx\n", ull);
        ui = 0;
        for (j = 0; j < 4; ++j) {
            if (j > 0)
                ui <<= 8;
            ui |= rp[76 + j];
        }
        printf("  programmed phy capabilities: 0x%x\n", ui);
        if (op->do_cap_phy)
            decode_phy_cap(ui, op);
        ui = 0;
        for (j = 0; j < 4; ++j) {
            if (j > 0)
                ui <<= 8;
            ui |= rp[80 + j];
        }
        printf("  current phy capabilities: 0x%x\n", ui);
        if (op->do_cap_phy)
            decode_phy_cap(ui, op);
        ui = 0;
        for (j = 0; j < 4; ++j) {
            if (j > 0)
                ui <<= 8;
            ui |= rp[84 + j];
        }
        printf("  attached phy capabilities: 0x%x\n", ui);
        if (op->do_cap_phy)
            decode_phy_cap(ui, op);
    }
    if (len > 95) {
        printf("  reason: %s\n",
               smp_get_reason((0xf0 & rp[94]) >> 4, sizeof(b), b));
        printf("  negotiated physical link rate: %s\n",
               smp_get_neg_xxx_link_rate(0xf & rp[94], sizeof(b), b));
        printf("  optical mode enabled: %d\n", !!(rp[95] & 0x4));
        printf("  negotiated SSC: %d\n", !!(rp[95] & 0x2));
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
        /* 'shadow zoning enabled' added in spl2r03 */
        printf("  shadow zoning enabled: %d\n", !!(rp[104] & 0x1));
        printf("  shadow zone group: %d\n", rp[107]);
    }
    if (len > 109) {
        printf("  device slot number: %d\n", rp[108]);
        printf("  device slot group number: %d\n", rp[109]);
    }
    if (len > 115)
        printf("  device slot group output connector: %.6s\n", rp + 110);
    if (len > 117)
        printf("  STP buffer size: %d\n", (rp[116] << 8) + rp[117]);
    return 0;
}

/* short format: only DISCOVER LIST has this abridged 24 byte descriptor.
 * Returns 0 for okay, else -1 . */
static int
decode_desc1_multiline(const unsigned char * rp, int z_enabled,
                       struct opts_t * op)
{
    unsigned long long ull;
    int func_res, phy_id, adt, route_attr, j;
    char b[256];

    phy_id = rp[0];
    func_res = rp[1];
    printf("  phy identifier: %d\n", phy_id);
    if (SMP_FRES_PHY_VACANT == func_res) {
        printf("  inaccessible (phy vacant)\n");
        return 0;
    } else if (func_res) {
        printf("  >>> function result: %s\n",
               smp_get_func_res_str(func_res, sizeof(b), b));
        return -1;
    }
    adt = ((0x70 & rp[2]) >> 4);
    if (adt < 8)
        printf("  attached SAS device type: %s\n",
               smp_attached_device_type[adt]);
    if ((op->do_brief > 1) && (0 == adt))
        return 0;
    if (0 == op->do_brief)
        printf("  attached reason: %s\n",
               smp_get_reason(0xf & rp[2], sizeof(b), b));
    printf("  negotiated logical link rate: %s\n",
           smp_get_neg_xxx_link_rate(0xf & rp[3], sizeof(b), b));

    printf("  attached initiator: ssp=%d stp=%d smp=%d sata_host=%d\n",
           !!(rp[4] & 8), !!(rp[4] & 4), !!(rp[4] & 2), (rp[4] & 1));
    if (0 == op->do_brief)
        printf("  attached sata port selector: %d\n", !!(rp[5] & 0x80));
    printf("  attached target: ssp=%d stp=%d smp=%d sata_device=%d\n",
           !!(rp[5] & 8), !!(rp[5] & 4), !!(rp[5] & 2), (rp[5] & 1));

    if (0 == op->do_brief)
        printf("  virtual phy: %d\n", !!(rp[6] & 0x80));
    ull = 0;
    for (j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= rp[12 + j];
    }
    printf("  attached SAS address: 0x%llx\n", ull);
    printf("  attached phy identifier: %d\n", rp[10]);
    if (0 == op->do_brief)
        printf("  phy change count: %d\n", rp[11]);
    route_attr = (0xf & rp[6]);
    switch (route_attr) {
    case 0: snprintf(b, sizeof(b), "direct"); break;
    case 1: snprintf(b, sizeof(b), "subtractive"); break;
    case 2: snprintf(b, sizeof(b), "table"); break;
    default: snprintf(b, sizeof(b), "reserved [%d]", route_attr); break;
    }
    printf("  routing attribute: %s\n", b);
    if (op->do_brief) {
        if (z_enabled)
            printf("  zone group: %d\n", rp[8]);
        return 0;
    }
    printf("  reason: %s\n", smp_get_reason(0xf & (rp[7] >> 4),
                                            sizeof(b), b));
    printf("  negotiated physical link rate: %s\n",
           smp_get_neg_xxx_link_rate(0xf & rp[7], sizeof(b), b));
    printf("  zone group: %d\n", rp[8]);
    printf("  inside ZPSDS persistent: %d\n", !!(rp[9] & 0x20));
    printf("  requested inside ZPSDS: %d\n", !!(rp[9] & 0x10));
    /* printf("  zone address resolved: %d\n", !!(rp[9] & 0x8)); */
    printf("  zone group persistent: %d\n", !!(rp[9] & 0x4));
    printf("  inside ZPSDS: %d\n", !!(rp[9] & 0x2));
    return 0;
}

/* Decodes either descriptor type into one line output. This is a
 * "per phy" function. Returns 0 for ok, 1 for ok plus zoning enabled and
  * seen ZG other than 1, else -1 (for problem) . */
static int
decode_1line(const unsigned char * rp, int len, int desc,
             int z_enabled, int has_t2t, struct opts_t * op)
{
    unsigned long long ull, adn;
    int phy_id, j, off, plus, negot, adt, route_attr, vp, asa_off;
    int func_res, aphy_id, a_init, a_target, z_group, iz_mask;
    int zg_not1 = 0;
    char b[256];
    const char * cp;

    switch (desc) {
    case 0:     /* longer descriptor */
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
        z_group = rp[63];
        iz_mask = rp[60];
        break;
    case 1:     /* abridged 24 byte descriptor [short] */
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
        z_group = rp[8];
        iz_mask = rp[9];
        break;
    default:
        pr2serr("  Unknown descriptor type %d\n", desc);
        return -1;
    }
    if (op->zpi_fn) {
        if (func_res && (SMP_FRES_PHY_VACANT != func_res)) {
            pr2serr("  >>> function result: %s\n",
                    smp_get_func_res_str(func_res, sizeof(b), b));
            return -1;
        }
        snprintf(b, sizeof(b) - 1, "%x,%x,0,%x\n", phy_id, iz_mask & 0x34,
                 z_group);
        b[sizeof(b) - 1] = '\0';
        fprintf(op->zpi_filep, "%s", b);
        return 0;
    }
    if (SMP_FRES_PHY_VACANT == func_res) {
        printf("  phy %3d: inaccessible (phy vacant)\n", phy_id);
        return 0;
    } else if (func_res) {
        printf("  phy %3d: function result: %s\n", phy_id,
               smp_get_func_res_str(func_res, sizeof(b), b));
        return -1;
    }
    if ((0 == op->verbose) && (0 == adt) && (op->do_brief > 1))
        return 0;

    switch (route_attr) {
    case 0:
        cp = "D";
        break;
    case 1:
        cp = "S";
        break;
    case 2:     /* table routing phy when expander does t2t is Universal */
        cp = has_t2t ? "U" : "T";
        break;
    default:
        cp = "R";
        break;
    }
    switch (negot) {
    case 1:
        printf("  phy %3d:%s:disabled\n", phy_id, cp);
        return 0;
    case 2:
        printf("  phy %3d:%s:reset problem\n", phy_id, cp);
        return 0;
    case 3:
        printf("  phy %3d:%s:spinup hold\n", phy_id, cp);
        return 0;
    case 4:
        printf("  phy %3d:%s:port selector\n", phy_id, cp);
        return 0;
    case 5:
        printf("  phy %3d:%s:reset in progress\n", phy_id, cp);
        return 0;
    case 6:
        printf("  phy %3d:%s:unsupported phy attached\n", phy_id, cp);
        return 0;
    default:
        /* keep going */
        break;
    }
    if ((0 == op->verbose) && (0 == adt) && op->do_brief)
        return 0;
    ull = 0;
    for (j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= rp[asa_off + j];
    }
    if ((0 == adt) || (adt > 3)) {
        printf("  phy %3d:%s:attached:[0000000000000000:00]", phy_id, cp);
        if ((op->do_brief > 1) || op->do_adn) {
            printf("\n");
            return 0;
        }
        if (z_enabled && (1 != z_group)) {
            ++zg_not1;
            printf("  ZG:%d", z_group);
        }
        if (op->do_dsn && (0 == desc) && (len > 108) && (0xff != rp[108]))
             printf("  dsn=%d", rp[108]);
        printf("\n");
        return !! zg_not1;
    }
    if ((0 == desc) && op->do_adn) {
        adn = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                adn <<= 8;
            adn |= rp[52 + j];
        }
        printf("  phy %3d:%s:attached:[%016llx:%02d %016llx %s%s", phy_id,
               cp, ull, aphy_id, adn, smp_short_attached_device_type[adt],
               (vp ? " V" : ""));
    } else
        printf("  phy %3d:%s:attached:[%016llx:%02d %s%s", phy_id, cp, ull,
               aphy_id, smp_short_attached_device_type[adt],
               (vp ? " V" : ""));
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
    if ((op->do_brief < 2) && (0 == op->do_adn)) {
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
        case 0xb:
            cp = "  12 Gbps";
            break;
        default:
            cp = "";
            break;
        }
        printf("%s", cp);
        if (z_enabled && (1 != z_group)) {
            ++zg_not1;
            printf("  ZG:%d", z_group);
        }
        if (op->do_dsn && (0 == desc) && (len > 108) && (0xff != rp[108]))
            printf("  dsn=%d", rp[108]);
    }
    printf("\n");
    return !! zg_not1;
}

static void
output_header_info(const unsigned char * resp, struct opts_t * op)
{
    int hdr_ecc, sphy_id, z_enabled ;

    hdr_ecc = (resp[4] << 8) + resp[5];
    sphy_id = resp[8];
    z_enabled = !!(resp[16] & 0x40);

    if (op->zpi_fn) {
        if (0 == op->do_brief) {
            fprintf(op->zpi_filep, "# Zone phy information from DISCOVER "
                    "LIST:\n");
            fprintf(op->zpi_filep, "#  expander change count: %d\n",
                    hdr_ecc);
            fprintf(op->zpi_filep, "#  starting phy id: %d\n", sphy_id);
            fprintf(op->zpi_filep, "#  maximum number of phys output: %d\n",
                    op->do_num);
            fprintf(op->zpi_filep, "#  zoning enabled: %d\n", z_enabled);
            fprintf(op->zpi_filep, "#\n# Values below are in hex, phy_id in "
                    "first column, zone group in last\n");
        }
    } else {
        if (! op->do_1line) {
            printf("Discover list response header:\n");
            printf("  starting phy id: %d\n", sphy_id);
            printf("  number of discover list descriptors: %d\n", resp[9]);
        }
        if ((! op->do_1line) && (0 == op->do_brief)) {
            printf("  expander change count: %d\n", hdr_ecc);
            printf("  filter: %d\n", resp[10] & 0xf);
            printf("  descriptor type: %d\n", resp[11] & 0xf);
            printf("  discover list descriptor length: %d bytes\n",
                   resp[12] * 4);
            printf("  zoning supported: %d\n", !!(resp[16] & 0x80));
            printf("  zoning enabled: %d\n", z_enabled);
            printf("  self configuring: %d\n", !!(resp[16] & 0x8));
            printf("  zone configuring: %d\n", !!(resp[16] & 0x4));
            printf("  configuring: %d\n", !!(resp[16] & 0x2));
            printf("  externally configurable route table: %d\n",
                   !!(resp[16] & 0x1));
            printf("  last self-configuration status descriptor index: %d\n",
                   (resp[18] << 8) + resp[19]);    /* sas2r12 */
            printf("  last phy event list descriptor index: %d\n",
                   (resp[20] << 8) + resp[21]);
        }
    }
}


int
main(int argc, char * argv[])
{
    int res, c, len, hdr_ecc, num_desc, resp_filter, resp_desc_type;
    int desc_len, k, j, no_more, err, off, adt, fresult;
    long long sa_ll;
    char i_params[256];
    char device_name[512];
    unsigned char resp[1020 + 8];
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int z_enabled = 0;
    int zg_not1 = 0;
    int checked_rg = 0;
    int has_t2t = 0;
    int ret = 0;
    struct opts_t opts;
    struct opts_t * op;

    op = &opts;
    memset(op, 0, sizeof(opts));
    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "Abcd:Df:hHiI:ln:op:rs:SvVZ:",
                        long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'A':
            ++op->do_adn;
            break;
        case 'b':
            ++op->do_brief;
            break;
        case 'c':
            ++op->do_cap_phy;
            break;
        case 'd':
           op->desc_type = smp_get_num(optarg);
           if ((op->desc_type < 0) || (op->desc_type > 15)) {
                pr2serr("bad argument to '--desc'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            ++op->desc_type_given;
            break;
        case 'D':
            ++op->do_dsn;
            break;
        case 'f':
           op->filter = smp_get_num(optarg);
           if ((op->filter < 0) || (op->filter > 15)) {
                pr2serr("bad argument to '--filter'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++op->do_hex;
            break;
        case 'i':
            ++op->ign_zp;
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'l':
            /* just ignore, placeholder */
            break;
        case 'n':
           op->do_num = smp_get_num(optarg);
           if ((op->do_num < 0) || (op->do_num > 254)) {
                pr2serr("bad argument to '--num', expect value from 0 to "
                        "254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            ++op->num_given;
            break;
        case 'o':
            ++op->do_1line;
            break;
        case 'p':
           op->phy_id = smp_get_num(optarg);
           if ((op->phy_id < 0) || (op->phy_id > 254)) {
                pr2serr("bad argument to '--phy', expect value from 0 to "
                        "254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            ++op->phy_id_given;
            break;
        case 'r':
            ++op->do_raw;
            break;
        case 's':
           sa_ll = smp_get_llnum(optarg);
           if (-1LL == sa_ll) {
                pr2serr("bad argument to '--sa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            op->sa = (unsigned long long)sa_ll;
            if (op->sa > 0)
                ++op->sa_given;
            break;
        case 'S':
            ++op->do_summary;
            break;
        case 'v':
            ++op->verbose;
            break;
        case 'V':
            pr2serr("version: %s\n", version_str);
            return 0;
        case 'Z':
            op->zpi_fn = optarg;
            break;
        default:
            pr2serr("unrecognised switch code %c [0x%x] ??\n", c, c);
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
                pr2serr("Unexpected extra argument: %s\n", argv[optind]);
            usage();
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if (0 == device_name[0]) {
        cp = getenv("SMP_UTILS_DEVICE");
        if (cp)
            strncpy(device_name, cp, sizeof(device_name) - 1);
        else {
            pr2serr("missing device name on command line\n    [Could use "
                    "environment variable SMP_UTILS_DEVICE instead]\n");
            usage();
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if ((cp = strchr(device_name, SMP_SUBVALUE_SEPARATOR))) {
        *cp = '\0';
        if (1 != sscanf(cp + 1, "%d", &subvalue)) {
            pr2serr("expected number after separator in SMP_DEVICE name\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if (0 == op->sa) {
        cp = getenv("SMP_UTILS_SAS_ADDR");
        if (cp) {
           sa_ll = smp_get_llnum(cp);
           if (-1LL == sa_ll) {
                pr2serr("bad value in environment variable "
                        "SMP_UTILS_SAS_ADDR\n");
                pr2serr("    use 0\n");
                sa_ll = 0;
            }
            op->sa = (unsigned long long)sa_ll;
        }
    }
    if (op->sa > 0) {
        if (! smp_is_naa5(op->sa)) {
            pr2serr("SAS (target) address not in naa-5 format (may need "
                    "leading '0x')\n");
            if ('\0' == i_params[0]) {
                pr2serr("    use '--interface=' to override\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
        }
    }
    if (op->desc_type_given && op->desc_type) { /* asked for short format */
        if (op->do_dsn) {
            pr2serr("warning: --dsn option ignored when --desc_type=1\n");
            op->do_dsn = 0;
        }
    }
    if (0 == op->do_dsn) {
        cp = getenv("SMP_UTILS_DSN");
        if (cp)
            ++op->do_dsn;
    }
    if ((0 == op->do_summary) && (0 == op->do_1line) &&
        (! op->num_given) && (0 == op->phy_id_given) &&
        (NULL == op->zpi_fn))
        ++op->do_summary;
    if (op->zpi_fn) {
        if (op->do_summary || op->desc_type_given || op->filter ||
            op->do_adn) {
            pr2serr("--zpi=FN clashes with --summary, --adn, --filter and "
                    "--descriptor=TY options\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
        if (! op->num_given)
            op->do_num = 254;
        op->do_1line = 1;
        op->desc_type = 1;
        op->ign_zp = 1;        /* set --ignore */
    }
    if (0 == op->desc_type_given) {
        op->desc_type = op->do_brief ? 1 : 0;
        if (op->do_adn || op->do_dsn)
            op->desc_type = 0;
    }
    if (op->do_summary) {
        ++op->do_brief;
        if ((0 == op->desc_type_given) && (0 == op->do_adn) &&
            (0 == op->do_dsn))
            op->desc_type = 1;
        op->do_1line = 1;
        op->do_num = 254;
    } else if (! (op->num_given || op->zpi_fn))
        op->do_num = 1;
    if (op->do_adn && (1 == op->desc_type)) {
        pr2serr("--adn and --descriptor=1 options clash since there is no "
                "'attached\ndevice name' field in the short format. "
                "Ignoring --adn .\n");
        op->do_adn = 0;
    }

    res = smp_initiator_open(device_name, subvalue, i_params, op->sa,
                             &tobj, op->verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    if (op->zpi_fn) {
        if ((1 == strlen(op->zpi_fn)) && (0 == strcmp("-", op->zpi_fn)))
            op->zpi_filep = stdout;
        else {
            op->zpi_filep  = fopen(op->zpi_fn, "w");
            if (NULL == op->zpi_filep) {
                pr2serr("unable to open %s, error: %s\n", op->zpi_fn,
                        safe_strerror(errno));
                ret = SMP_LIB_FILE_ERROR;
                goto err_out;
            }
        }
    }

    no_more = 0;
    for (j = 0; (j < op->do_num) && (! no_more); j += num_desc) {
        memset(resp, 0, sizeof(resp));
        if ((op->phy_id + j) > 254) {
            ret = 0;    /* off the end so not error */
            break;
        }
        ret = do_discover_list(&tobj, op->phy_id + j, resp, sizeof(resp), op);
        if (ret) {
            if (SMP_FRES_NO_PHY == ret)
                ret = 0;    /* off the end so not error */
            break;
        }
        num_desc = resp[9];
        if ((0 == op->desc_type) && (num_desc < MAX_DLIST_LONG_DESCS))
            no_more = 1;
        if ((1 == op->desc_type) && (num_desc < MAX_DLIST_SHORT_DESCS))
            no_more = 1;
        if (op->do_hex || op->do_raw)
            continue;
        len = (resp[3] * 4) + 4;    /* length in bytes excluding CRC field */
        if ((0 == j) && ((! op->do_1line) || op->zpi_fn))
            output_header_info(resp, op);
        hdr_ecc = (resp[4] << 8) + resp[5];
        z_enabled = !!(resp[16] & 0x40);
        resp_filter = resp[10] & 0xf;
        if (op->filter != resp_filter)
            pr2serr(">>> Requested phy filter was %d, got %d\n", op->filter,
                    resp_filter);
        resp_desc_type = resp[11] & 0xf;
        if (op->desc_type != resp_desc_type)
            pr2serr(">>> Requested descriptor type was %d, got %d\n",
                    op->desc_type, resp_desc_type);
        desc_len = resp[12] * 4;
        if (len != (48 + (num_desc * desc_len))) {
            pr2serr(">>> Response length of %d bytes doesn't match "
                    "%d descriptors, each\n  of %d bytes plus a 48 byte "
                    "header and 4 byte CRC\n", len + 4, num_desc, desc_len);
            if (len < (48 + (num_desc * desc_len))) {
                ret = SMP_LIB_CAT_MALFORMED;
                break;
            }
        }
        for (k = 0, err = 0; k < num_desc; ++k) {
            off = 48 + (k * desc_len);
            if (op->do_1line) {
                if (! checked_rg) {
                    ++checked_rg;
                    has_t2t = has_table2table_routing(&tobj, op);
                }
                res = decode_1line(resp + off, desc_len, resp_desc_type,
                                   z_enabled, has_t2t, op);
                if (res < 0)
                    ++err;
                else if (res > 0)
                    ++zg_not1;
            } else {
                fresult = resp[off + 2];    /* function result, 0 -> ok */
                if (0 == resp_desc_type) {
                    adt = (resp[off + 12] >> 4) & 7;
                    if ((0 == op->do_brief) || adt || fresult) {
                        printf("descriptor %d:\n", j + k);
                        if (decode_desc0_multiline(resp + off, hdr_ecc, op))
                            ++err;
                    }
                } else if (1 == resp_desc_type) {
                    adt = (resp[off + 2] >> 4) & 7;
                    if ((0 == op->do_brief) || adt || fresult) {
                        printf("descriptor %d:\n", j + k);
                        if (decode_desc1_multiline(resp + off, z_enabled, op))
                            ++err;
                    }
                } else
                    ++err;
            }
        }
        if (err) {
            if (op->verbose)
               pr2serr(">>> %d error%s detected\n", err,
                       ((1 == err) ? "" : "s"));
            if (0 == ret)
                ret = SMP_LIB_CAT_OTHER;
        }
    }
    if ((zg_not1) && (0 == op->do_brief) && (NULL == op->zpi_fn))
        printf("Zoning %sabled\n", z_enabled ? "en" : "dis");

err_out:
    if (op->zpi_filep && (stdout != op->zpi_filep))
        fclose(op->zpi_filep);
    res = smp_initiator_close(&tobj);
    if (res < 0) {
        if (0 == ret)
            return SMP_LIB_FILE_ERROR;
    }
    if (ret < 0)
        ret = SMP_LIB_CAT_OTHER;
    if (op->verbose && ret)
        pr2serr("Exit status %d indicates error detected\n", ret);
    return ret;
}
