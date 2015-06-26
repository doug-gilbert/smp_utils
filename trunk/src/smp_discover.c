/*
 * Copyright (c) 2006-2015 Douglas Gilbert.
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
 * This utility issues a DISCOVER function and outputs its response.
 *
 * First defined in SAS-1. From and including SAS-2.1 this function is
 * defined in the SPL series. The most recent SPL-3 draft is spl3r07.pdf .
 */

static const char * version_str = "1.50 20150626";    /* spl4r02 */


#define SMP_FN_DISCOVER_RESP_LEN 124
#define SMP_FN_REPORT_GENERAL_RESP_LEN 76


struct opts_t {
    int do_adn;
    int do_brief;
    int do_cap_phy;
    int do_dsn;
    int do_hex;
    int ign_zp;
    int do_list;
    int multiple;
    int do_my;
    int do_num;
    int phy_id;
    int phy_id_given;
    int do_raw;
    int do_summary;
    int verbose;
    int do_zero;
    int sa_given;
    unsigned long long sa;
};

static struct option long_options[] = {
        {"adn", no_argument, 0, 'A'},
        {"brief", no_argument, 0, 'b'},
        {"cap", no_argument, 0, 'c'},
        {"dsn", no_argument, 0, 'D'},
        {"help", no_argument, 0, 'h'},
        {"hex", no_argument, 0, 'H'},
        {"ignore", no_argument, 0, 'i'},
        {"interface", required_argument, 0, 'I'},
        {"list", no_argument, 0, 'l'},
        {"multiple", no_argument, 0, 'm'},
        {"my", no_argument, 0, 'M'},
        {"num", required_argument, 0, 'n'},
        {"phy", required_argument, 0, 'p'},
        {"sa", required_argument, 0, 's'},
        {"summary", no_argument, 0, 'S'},
        {"raw", no_argument, 0, 'r'},
        {"verbose", no_argument, 0, 'v'},
        {"version", no_argument, 0, 'V'},
        {"zero", no_argument, 0, 'z'},
        {0, 0, 0, 0},
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
          "smp_discover [--adn] [--brief] [--cap] [--dsn] [--help] [--hex]\n"
          "                    [--ignore] [--interface=PARAMS] [--list] "
          "[--multiple]\n"
          "                    [--my] [--num=NUM] [--phy=ID] [--raw] "
          "[--sa=SAS_ADDR]\n"
          "                    [--summary] [--verbose] [--version] "
          "[--zero]\n"
          "                    SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --adn|-A             output attached device name in one "
          "line per\n"
          "                         phy mode (i.e. with --multiple)\n"
          "    --brief|-b           less output, can be used multiple "
          "times\n"
          "    --cap|-c             decode phy capabilities bits\n"
          "    --dsn|-D             show device slot number in 1 line\n"
          "                         per phy output, if available\n"
          "    --help|-h            print out usage message\n"
          "    --hex|-H             print response in hexadecimal\n"
          "    --ignore|-i          sets the Ignore Zone Group bit; "
          "will show\n"
          "                         phys otherwise hidden by zoning\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --list|-l            output attribute=value, 1 per line\n"
          "    --multiple|-m        query multiple phys, output 1 line "
          "for each\n"
          "                         if given twice, full output for each "
          "phy\n"
          "    --my|-M              output my (expander's) SAS address\n"
          "    --num=NUM|-n NUM     number of phys to fetch when '-m' "
          "is given\n"
          "                         (def: 0 -> the rest)\n"
          "    --phy=ID|-p ID       phy identifier [or starting phy id]\n"
          "    --raw|-r             output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading\n"
          "                                 '0x' or trailing 'h'). Depending "
          "on\n"
          "                                 the interface, may not be "
          "needed\n"
          "    --summary|-S         query phys, output 1 line for each "
          "active one,\n"
          "                         equivalent to '--multiple --brief' "
          "('-mb').\n"
          "                         This option is assumed if '--phy=ID' "
          "not given\n"
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n"
          "    --zero|-z            zero Allocated Response Length "
          "field,\n"
          "                         may be required prior to SAS-2\n\n"
          "Sends one or more SMP DISCOVER functions. If '--phy=ID' not "
          "given then\n'--summary' is assumed. The '--summary' option "
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
has_table2table_routing(struct smp_target_obj * top, const struct opts_t * op)
{
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_GENERAL,
                               0, 0, 0, 0, 0, 0};
    struct smp_req_resp smp_rr;
    unsigned char rp[SMP_FN_REPORT_GENERAL_RESP_LEN];
    char b[256];
    char * cp;
    int len, res, k, act_resplen;

    memset(rp, 0, sizeof(rp));
    if (op->verbose) {
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
    res = smp_send_req(top, &smp_rr, op->verbose);

    if (res) {
        pr2serr("RG smp_send_req failed, res=%d\n", res);
        if (0 == op->verbose)
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
            if (op->verbose > 1)
                pr2serr("unable to determine RG response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if ((act_resplen >= 0) && (len > act_resplen)) {
        if (op->verbose)
            pr2serr("actual RG response length [%d] less than deduced "
                    "length [%d]\n", act_resplen, len);
        len = act_resplen;
    }
    /* ignore --hex and --raw */
    if (SMP_FRAME_TYPE_RESP != rp[0]) {
        pr2serr("RG expected SMP frame response type, got=0x%x\n",
                rp[0]);
        return 0;
    }
    if (rp[1] != smp_req[1]) {
        pr2serr("RG Expected function code=0x%x, got=0x%x\n",
                smp_req[1], rp[1]);
        return 0;
    }
    if (rp[2]) {
        if (op->verbose > 1) {
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
    "SAS or SATA device",         /* was 'end device' */
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
    "fex",      /* obsolete in sas2r05a */
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
    case 0x3:
        snprintf(buff, buff_len, "QSFP+ receptacle (SFF-8436) "
                "[max 4 phys]");
        break;
    case 0x4:
        snprintf(buff, buff_len, "Mini SAS 4x active receptacle (SFF-8088) "
                "[max 4 phys]");
        break;
    case 0x5:
        snprintf(buff, buff_len, "Mini SAS HD 4x receptacle (SFF-8644) "
                "[max 4 phys]");
        break;
    case 0x6:
        snprintf(buff, buff_len, "Mini SAS HD 8x receptacle (SFF-8644) "
                "[max 8 phys]");
        break;
    case 0x7:
        snprintf(buff, buff_len, "Mini SAS HD 16x receptacle (SFF-8644) "
                "[max 16 phys]");
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
    case 0x12:
        snprintf(buff, buff_len, "Mini SAS HD 4i receptacle (SFF-8643) "
                "[max 4 phys]");
        break;
    case 0x13:
        snprintf(buff, buff_len, "Mini SAS HD 8i receptacle (SFF-8643) "
                "[max 8 phys]");
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
    case 0x24:
        snprintf(buff, buff_len, "Micro SAS receptacle "
                "[max 2 phys]");
        break;
    case 0x25:
        snprintf(buff, buff_len, "Micro SATA device plug "
                "[max 1 phy]");
        break;
    case 0x26:
        snprintf(buff, buff_len, "Micro SAS plug "
                "[max 2 phys]");
        break;
    case 0x27:
        snprintf(buff, buff_len, "Micro SAS/SATA plug "
                "[max 2 phys]");
        break;
    case 0x28:
        snprintf(buff, buff_len, "12 Gb/s SAS Drive backplane receptacle "
                "(SFF-8680) [max 2 phys]");
        break;
    case 0x29:
        snprintf(buff, buff_len, "12Gb/s SAS Drive Plug (SFF-8680) "
                "[max 2 phys]");
        break;
    case 0x2a:
        snprintf(buff, buff_len, "Multifunction 12 Gb/s 6x Unshielded "
                "receptacle (SFF-8639) [max 6 phys]");
        break;
    case 0x2b:
        snprintf(buff, buff_len, "Multifunction 12 Gb/s 6x Unshielded plug "
                "(SFF-8639) [max 6 phys]");
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
        else if (conn_type < 0x3f)
            snprintf(buff, buff_len, "unknown internal connector"
                     ", type: 0x%x", conn_type);
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

/* Returns length of response in bytes, excluding the CRC on success,
   -3 (or less) -> SMP_LIB errors negated ('-4 - smp_err),
   -1 for other errors */
static int
do_discover(struct smp_target_obj * top, int disc_phy_id,
            unsigned char * resp, int max_resp_len,
            int silence_err_report, const struct opts_t * op)
{
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_DISCOVER, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct smp_req_resp smp_rr;
    char b[256];
    char * cp;
    int len, res, k, act_resplen;

    memset(resp, 0, max_resp_len);
    if (! op->do_zero) {     /* SAS-2 or later */
        len = (max_resp_len - 8) / 4;
        smp_req[2] = (len < 0x100) ? len : 0xff; /* Allocated Response Len */
        smp_req[3] = 2; /* Request Length: in dwords */
    }
    if (op->ign_zp)
        smp_req[8] |= 0x1;
    smp_req[9] = disc_phy_id;
    if (op->verbose) {
        pr2serr("    Discover request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            pr2serr("%02x ", smp_req[k]);
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
        return -4 - SMP_LIB_CAT_MALFORMED;
    }
    len = resp[3];
    if ((0 == len) && (0 == resp[2])) {
        len = smp_get_func_def_resp_len(resp[1]);
        if (len < 0) {
            len = 0;
            if (op->verbose > 1)
                pr2serr("unable to determine response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if ((act_resplen >= 0) && (len > act_resplen)) {
        if (op->verbose)
            pr2serr("actual response length [%d] less than deduced length "
                    "[%d]\n", act_resplen, len);
        len = act_resplen;
    }
    if (op->do_hex || op->do_raw) {
        if (op->do_hex)
            dStrHex((const char *)resp, len, 1);
        else
            dStrRaw((const char *)resp, len);
        if (SMP_FRAME_TYPE_RESP != resp[0])
            return -4 - SMP_LIB_CAT_MALFORMED;
        if (resp[1] != smp_req[1])
            return -4 - SMP_LIB_CAT_MALFORMED;
        if (resp[2]) {
            if (op->verbose)
                pr2serr("Discover result: %s\n",
                        smp_get_func_res_str(resp[2], sizeof(b), b));
            return -4 - resp[2];
        }
        return len;
    }
    if (SMP_FRAME_TYPE_RESP != resp[0]) {
        pr2serr("expected SMP frame response type, got=0x%x\n", resp[0]);
        return -4 - SMP_LIB_CAT_MALFORMED;
    }
    if (resp[1] != smp_req[1]) {
        pr2serr("Expected function code=0x%x, got=0x%x\n", smp_req[1],
                resp[1]);
        return -4 - SMP_LIB_CAT_MALFORMED;
    }
    if (resp[2]) {
        if ((op->verbose > 0) || (! silence_err_report)) {
            cp = smp_get_func_res_str(resp[2], sizeof(b), b);
            pr2serr("Discover result: %s\n", cp);
        }
        return -4 - resp[2];
    }
    return len;
}

/* Note that the inner attributes are output in alphabetical order. */
/* N.B. This function has not been kept up to date. */
static int
print_single_list(const unsigned char * rp, int len, int show_exp_cc,
                  int do_brief)
{
    int res, j, sas2;
    unsigned long long ull;

    sas2 = !! (rp[3]);
    if (sas2 && show_exp_cc && (! do_brief)) {
        res = (rp[4] << 8) + rp[5];
        printf("expander_cc=%d\n", res);
    }
    printf("phy_id=%d\n", rp[9]);
    if (! do_brief) {
        if (sas2)
            printf("  att_br_cap=%d\n", !!(0x1 & rp[33]));
        if (len > 59) {
            for (ull = 0, j = 0; j < 8; ++j) {
                if (j > 0)
                    ull <<= 8;
                ull |= rp[52 + j];
            }
            printf("  att_dev_name=0x%llx\n", ull);
        }
    }
    printf("  att_dev_type=%d\n", (0x70 & rp[12]) >> 4);
    if (sas2 && (! do_brief)) {
        printf("  att_iz_per=%d\n", !!(0x4 & rp[33]));
        printf("  att_pa_cap=%d\n", !!(0x8 & rp[33]));
    }
    printf("  att_phy_id=%d\n", rp[32]);
    if (sas2 && (! do_brief)) {
        printf("  att_pow_cap=%d\n", (rp[33] >> 5) & 0x3);
        printf("  att_pwr_dis_cap=%d\n", !!(rp[34] & 1));
        printf("  att_reason=%d\n", (0xf & rp[12]));
        printf("  att_req_iz=%d\n", !!(0x2 & rp[33]));
    }
    for (ull = 0, j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= rp[24 + j];
    }
    printf("  att_sas_addr=0x%llx\n", ull);
    printf("  att_sata_dev=%d\n", !! (0x1 & rp[15]));
    printf("  att_sata_host=%d\n", !! (0x1 & rp[14]));
    printf("  att_sata_ps=%d\n", !! (0x80 & rp[15]));
    if (sas2 && (! do_brief))
        printf("  att_sl_cap=%d\n", !!(0x10 & rp[33]));
    printf("  att_smp_init=%d\n", !! (0x2 & rp[14]));
    printf("  att_smp_targ=%d\n", !! (0x2 & rp[15]));
    printf("  att_ssp_init=%d\n", !! (0x8 & rp[14]));
    printf("  att_ssp_targ=%d\n", !! (0x8 & rp[15]));
    printf("  att_stp_init=%d\n", !! (0x4 & rp[14]));
    printf("  att_stp_targ=%d\n", !! (0x4 & rp[15]));
    if (! do_brief) {
        if (sas2 || (rp[45] & 0x7f)) {
            printf("  conn_elem_ind=%d\n", rp[46]);
            printf("  conn_p_link=%d\n", rp[47]);
            printf("  conn_type=%d\n", (0x7f & rp[45]));
        }
        if (len > 109) {
            printf("  dev_slot_num=%d\n", rp[108]);
            printf("  dev_slot_grp_num=%d\n", rp[109]);
        }
    }
    if (! do_brief) {
        printf("  hw_max_p_lrate=%d\n", (0xf & rp[41]));
        printf("  hw_min_p_lrate=%d\n", (0xf & rp[40]));
        if (len > 95)
            printf("  hw_mux_sup=%d\n", (!! (rp[95] & 0x1)));
    }

    if (! do_brief) {
        printf("  iz=%d\n", !! (0x2 & rp[60]));
        printf("  iz_pers=%d\n", !! (0x20 & rp[60]));
    }
    printf("  neg_log_lrate=%d\n", (0xf & rp[13]));
    if (! do_brief) {
        if (len > 95) {
            printf("  neg_phy_lrate=%d\n", (0xf & rp[94]));
            printf("  opt_m_en=%d\n", (!! (rp[95] & 0x4)));
        }
        printf("  phy_cc=%d\n", rp[42]);
        printf("  phy_power_cond=%d\n", ((0xc0 & rp[48]) >> 6));
        printf("  pp_timeout=%d\n", (0xf & rp[43]));
        printf("  pr_max_p_lrate=%d\n", ((0xf0 & rp[41]) >> 4));
        printf("  pr_min_p_lrate=%d\n", ((0xf0 & rp[40]) >> 4));
        if (sas2) {
            printf("  pwr_dis_ctl_cap=%d\n", (rp[49] & 0x30) >> 4);
            printf("  pwr_dis_sig=%d\n", (rp[49] & 0xc0) >> 6);
        }
    }
    if ((! do_brief) && (len > 95))
            printf("  reason=%d\n", (0xf0 & rp[94]) >> 4);
    if (! do_brief) {
        printf("  req_iz=%d\n", !! (0x10 & rp[60]));
        printf("  req_iz_cbe=%d\n", !! (0x40 & rp[60]));
    }
    printf("  routing_attr=%d\n", rp[44] & 0xf);

    for (ull = 0, j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= rp[16 + j];
    }
    printf("  sas_addr=0x%llx\n", ull);
    if (! do_brief) {
        printf("  sas_pa_cap=%d\n", !!(0x4 & rp[48]));
        printf("  sas_pa_en=%d\n", !!(0x4 & rp[49]));
        printf("  sas_pow_cap=%d\n", (rp[48] >> 4) & 0x3);
        printf("  sas_sl_cap=%d\n", !!(0x8 & rp[48]));
        printf("  sas_sl_en=%d\n", !!(0x8 & rp[49]));
        printf("  sata_pa_cap=%d\n", !!(0x1 & rp[48]));
        printf("  sata_pa_en=%d\n", !!(0x1 & rp[49]));
        printf("  sata_sl_cap=%d\n", !!(0x2 & rp[48]));
        printf("  sata_sl_en=%d\n", !!(0x2 & rp[49]));
        printf("  stp_buff_tsmall=%d\n", !!(0x10 & rp[15]));
    }

    printf("  virt_phy=%d\n", !!(0x80 & rp[43]));
    if (! do_brief) {
        printf("  zg=%d\n", rp[63]);
        printf("  zg_pers=%d\n", !! (0x4 & rp[60]));
        printf("  zoning_en=%d\n", !! (0x1 & rp[60]));
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

static int
print_single(const unsigned char * rp, int len, int just1,
             const struct opts_t * op)
{
    int j, sas2, res;
    unsigned int ui;
    unsigned long long ull;
    char b[256];

    ull = 0;
    if (len > 23) {
        /* fetch my (expander's) SAS addrss */
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= rp[16 + j];
        }
    }
    if (just1)
        printf("Discover response%s:\n", (op->do_brief ? " (brief)" : ""));
    else
        printf("phy identifier: %d\n", rp[9]);
    sas2 = !! (rp[3]);
    res = (rp[4] << 8) + rp[5];
    if ((sas2 && (! op->do_brief)) || (op->verbose > 3)) {
        if (op->verbose || (res > 0))
            printf("  expander change count: %d\n", res);
    }
    if (just1)
        printf("  phy identifier: %d\n", rp[9]);
    res = ((0x70 & rp[12]) >> 4);
    if (res < 8)
        printf("  attached SAS device type: %s\n",
               smp_attached_device_type[res]);
    if ((op->do_brief > 1) && (0 == res))
        return 0;
    if (sas2 || (op->verbose > 3))
        printf("  attached reason: %s\n",
               smp_get_reason(0xf & rp[12], sizeof(b), b));

    printf("  negotiated logical link rate: %s\n",
           smp_get_neg_xxx_link_rate(0xf & rp[13], sizeof(b), b));

    printf("  attached initiator: ssp=%d stp=%d smp=%d sata_host=%d\n",
           !!(rp[14] & 8), !!(rp[14] & 4), !!(rp[14] & 2), (rp[14] & 1));
    if (0 == op->do_brief) {
        printf("  attached sata port selector: %d\n",
               !!(rp[15] & 0x80));
        printf("  STP buffer too small: %d\n", !!(rp[15] & 0x10));
    }
    printf("  attached target: ssp=%d stp=%d smp=%d sata_device=%d\n",
           !!(rp[15] & 8), !!(rp[15] & 4), !!(rp[15] & 2), (rp[15] & 1));

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
        if (sas2 || (op->verbose > 3)) {
            printf("  attached persistent capable: %d\n", !!(rp[33] & 0x80));
            printf("  attached power capable: %d\n", ((rp[33] >> 5) & 0x3));
            printf("  attached slumber capable: %d\n", !!(rp[33] & 0x10));
            printf("  attached partial capable: %d\n", !!(rp[33] & 0x8));
            printf("  attached inside ZPSDS persistent: %d\n",
                   !!(rp[33] & 4));
            printf("  attached requested inside ZPSDS: %d\n", !!(rp[33] & 2));
            printf("  attached break_reply capable: %d\n", !!(rp[33] & 1));
            printf("  attached pwr_dis capable: %d\n", !!(rp[34] & 1));
        }
        printf("  programmed minimum physical link rate: %s\n",
               smp_get_plink_rate(((rp[40] >> 4) & 0xf), 1, sizeof(b), b));
        printf("  hardware minimum physical link rate: %s\n",
               smp_get_plink_rate((rp[40] & 0xf), 0, sizeof(b), b));
        printf("  programmed maximum physical link rate: %s\n",
               smp_get_plink_rate(((rp[41] >> 4) & 0xf), 1, sizeof(b), b));
        printf("  hardware maximum physical link rate: %s\n",
               smp_get_plink_rate((rp[41] & 0xf), 0, sizeof(b), b));
        printf("  phy change count: %d\n", rp[42]);
        printf("  virtual phy: %d\n", !!(rp[43] & 0x80));
        printf("  partial pathway timeout value: %d us\n", (rp[43] & 0xf));
    }
    res = (0xf & rp[44]);
    switch (res) {
    case 0: snprintf(b, sizeof(b), "direct"); break;
    case 1: snprintf(b, sizeof(b), "subtractive"); break;
    case 2: snprintf(b, sizeof(b), "table"); break;
    default: snprintf(b, sizeof(b), "reserved [%d]", res); break;
    }
    printf("  routing attribute: %s\n", b);
    if (op->do_brief) {
        if ((len > 63) && !!(rp[60] & 0x1))
            printf("  zone group: %d\n", rp[63]);
        return 0;
    }
    if (sas2 || (rp[45] & 0x7f)) {
        printf("  connector type: %s\n",
               find_sas_connector_type((rp[45] & 0x7f), b, sizeof(b)));
        printf("  connector element index: %d\n", rp[46]);
        printf("  connector physical link: %d\n", rp[47]);
        printf("  phy power condition: %d\n", (rp[48] & 0xc0) >> 6);
        printf("  sas power capable: %d\n", (rp[48] >> 4) & 0x3);
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
    }
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


/* Output (multiline) for a single phy. Return 0 on success, positive error
 * number suitable for exit status if problems. */
static int
do_single(struct smp_target_obj * top, const struct opts_t * op)
{
    unsigned char rp[SMP_FN_DISCOVER_RESP_LEN];
    unsigned long long ull;
    int j, len, ret;

    /* If do_discover() works, returns response length (less CRC bytes) */
    len = do_discover(top, op->phy_id, rp, sizeof(rp), 0, op);
    if (len < 0)
        ret = (len < -2) ? (-4 - len) : len;
    else
        ret = 0;
    if (op->do_hex || op->do_raw)
        return ret;
    ull = 0;
    if (len > 23) {
        /* fetch my (expander's) SAS addrss */
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= rp[16 + j];
        }
    }
    if (op->do_my) {
        printf("0x%llx\n", ull);
        if ((ull > 0) && (SMP_FRES_PHY_VACANT == ret))
            return 0;
        else
            return ret;
    }
    if (ret) {
        if (SMP_FRES_PHY_VACANT == ret)
            printf("  phy identifier: %d  inaccessible (phy vacant)\n",
                   op->phy_id);
        return ret;
    }
    if (op->do_list)
        return print_single_list(rp, len, 1, op->do_brief);
    else
        return print_single(rp, len, 1, op);
}

#define MAX_PHY_ID 254

/* Calls do_discover() multiple times. Summarizes info into one
 * line per phy. Returns 0 if ok, else function result. */
static int
do_multiple(struct smp_target_obj * top, const struct opts_t * op)
{
    unsigned char rp[SMP_FN_DISCOVER_RESP_LEN];
    unsigned long long ull, adn;
    unsigned long long expander_sa;
    int ret, len, k, j, num, off, plus, negot, adt, zg;
    char b[256];
    char dsn[10] = "";
    int first = 1;
    int checked_rg = 0;
    int has_t2t = 0;
    const char * cp;

    expander_sa = 0;
    num = op->do_num ? (op->phy_id + op->do_num) : MAX_PHY_ID;
    for (k = op->phy_id; k < num; ++k) {
        len = do_discover(top, k, rp, sizeof(rp), 1, op);
        if (len < 0)
            ret = (len < -2) ? (-4 - len) : len;
        else
            ret = 0;
        if (SMP_FRES_NO_PHY == ret)
            return 0;   /* expected, end condition */
        else if (SMP_FRES_PHY_VACANT == ret) {
            printf("  phy %3d: inaccessible (phy vacant)\n", k);
            continue;
        } else if (ret)
            return ret;
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= rp[16 + j];
        }
        if (0 == expander_sa)
            expander_sa = ull;
        else {
            if (ull != expander_sa) {
                if (ull > 0) {
                    pr2serr(">> expander's SAS address is changing?? "
                            "phy_id=%d, was=%llxh, now=%llxh\n", rp[9],
                            expander_sa, ull);
                    expander_sa = ull;
                } else if (op->verbose)
                    pr2serr(">> expander's SAS address shown as 0 at "
                            "phy_id=%d\n", rp[9]);
            }
        }
        if (first && (! op->do_raw)) {
            first = 0;
            if (op->sa_given && (op->sa != expander_sa))
                printf("  <<< Warning: reported expander address is not the "
                       "one requested >>>\n");
#if 0
            /* for compatibility with smp_discover_list which does not
             * know its own SAS address with short descriptors */
            printf("Device <%016llx>, expander:\n", expander_sa);
#endif
        }
        if (op->do_hex || op->do_raw)
            continue;

        if (op->do_list) {
            print_single_list(rp, len, 0, op->do_brief);
            continue;
        }
        if (op->multiple > 1) {
            print_single(rp, len, 0, op);
            continue;
        }
        adt = ((0x70 & rp[12]) >> 4);
        /* attached SAS device type: 0-> none, 1-> device, 2-> expander,
         * 3-> fanout expnader (obsolete), rest-> reserved */
        if ((op->do_brief > 1) && (0 == adt))
            continue;

        negot = rp[13] & 0xf;
        switch(rp[44] & 0xf) {
        case 0:
            cp = "D";
            break;
        case 1:
            cp = "S";
            break;
        case 2:
            if (! checked_rg) {
                ++checked_rg;
                has_t2t = has_table2table_routing(top, op);
            }
            /* table routing phy when expander does t2t is Universal */
            cp = has_t2t ? "U" : "T";
            break;
        default:
            cp = "R";
            break;
        }

        if (op->do_dsn && (len > 108) && (0xff != rp[108]))
            sprintf(dsn, "  dsn=%d", rp[108]);

        switch (negot) {
        case 1:
            printf("  phy %3d:%s:disabled%s\n", rp[9], cp, dsn);
            continue;   /* N.B. not break; finished with this line/phy */
        case 2:
            printf("  phy %3d:%s:reset problem%s\n", rp[9], cp, dsn);
            continue;
        case 3:
            printf("  phy %3d:%s:spinup hold%s\n", rp[9], cp, dsn);
            continue;
        case 4:
            printf("  phy %3d:%s:port selector%s\n", rp[9], cp, dsn);
            continue;
        case 5:
            printf("  phy %3d:%s:reset in progress%s\n", rp[9], cp, dsn);
            continue;
        case 6:
            printf("  phy %3d:%s:unsupported phy attached%s\n", rp[9], cp,
                   dsn);
            continue;
        default:
            /* keep going in this loop, probably attached to something */
            break;
        }
        if ((op->do_brief > 0) && (0 == adt))
            continue;
        if (k != rp[9])
            pr2serr(">> requested phy_id=%d differs from response phy=%d\n",
                    k, rp[9]);
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= rp[24 + j];
        }
        if ((0 == adt) || (adt > 3)) {
            printf("  phy %3d:%s:attached:[0000000000000000:00]", k, cp);
            if ((op->do_brief > 1) || op->do_adn || (len < 64)) {
                printf("\n");
                continue;
            }
            zg = rp[63];
            /* zoning_enabled and a zone_group other than 1 */
            if ((rp[60] & 0x1) && (1 != zg))
                printf("  ZG:%d", zg);
            if ('\0' != dsn[0])
                printf("%s", dsn);
            printf("\n");
            continue;
        }
        if (op->do_adn && (len > 59)) {
            adn = 0;
            for (j = 0; j < 8; ++j) {
                if (j > 0)
                    adn <<= 8;
                adn |= rp[52 + j];
            }
            printf("  phy %3d:%s:attached:[%016llx:%02d %016llx %s%s", k, cp,
                   ull, rp[32], adn, smp_short_attached_device_type[adt],
                   ((rp[43] & 0x80) ? " V" : ""));
        } else
            printf("  phy %3d:%s:attached:[%016llx:%02d %s%s", k, cp, ull,
                   rp[32], smp_short_attached_device_type[adt],
                   ((rp[43] & 0x80) ? " V" : ""));
        if (rp[14] & 0xf) {
            off = 0;
            plus = 0;
            off += snprintf(b + off, sizeof(b) - off, " i(");
            if (rp[14] & 0x8) {
                off += snprintf(b + off, sizeof(b) - off, "SSP");
                ++plus;
            }
            if (rp[14] & 0x4) {
                off += snprintf(b + off, sizeof(b) - off, "%sSTP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (rp[14] & 0x2) {
                off += snprintf(b + off, sizeof(b) - off, "%sSMP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (rp[14] & 0x1) {
                off += snprintf(b + off, sizeof(b) - off, "%sSATA",
                                (plus ? "+" : ""));
                ++plus;
            }
            printf("%s)", b);
        }
        if (rp[15] & 0xf) {
            off = 0;
            plus = 0;
            off += snprintf(b + off, sizeof(b) - off, " t(");
            if (rp[15] & 0x80) {
                off += snprintf(b + off, sizeof(b) - off, "PORT_SEL");
                ++plus;
            }
            if (rp[15] & 0x8) {
                off += snprintf(b + off, sizeof(b) - off, "%sSSP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (rp[15] & 0x4) {
                off += snprintf(b + off, sizeof(b) - off, "%sSTP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (rp[15] & 0x2) {
                off += snprintf(b + off, sizeof(b) - off, "%sSMP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (rp[15] & 0x1) {
                off += snprintf(b + off, sizeof(b) - off, "%sSATA",
                                (plus ? "+" : ""));
                ++plus;
            }
            printf("%s)", b);
        }
        if ((op->do_brief > 1) || op->do_adn) {
            printf("]");
            if ('\0' != dsn[0])
                printf("%s", dsn);
            printf("\n");
            continue;
        } else
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
        case 0xb:
            cp = "  12 Gbps";
            break;
        default:
            cp = "";
            break;
        }
        printf("%s", cp);
        if (len > 63) {
            zg = rp[63];
            if ((rp[60] & 0x1) && (1 != zg))
                printf("  ZG:%d", zg);
        }
        if ('\0' != dsn[0])
            printf("%s", dsn);
        printf("\n");
    }
    return 0;
}


int
main(int argc, char * argv[])
{
    int res, c;
    long long sa_ll;
    char i_params[256];
    char device_name[512];
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 0;
    struct opts_t opts;
    struct opts_t * op;

    op = &opts;
    memset(op, 0, sizeof(opts));
    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "AbcDhHiI:lmMn:p:rs:SvVz", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'A':
            ++op->do_adn;
            break;
        case 'b':
            ++op->do_brief;
            break;
        case 'D':
            ++op->do_dsn;
            break;
        case 'c':
            ++op->do_cap_phy;
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
            ++op->do_list;
            break;
        case 'm':
            ++op->multiple;
            break;
        case 'M':
            ++op->do_my;
            break;
        case 'n':
           op->do_num = smp_get_num(optarg);
           if (op->do_num < 0) {
                pr2serr("bad argument to '--num'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
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
        case 'v':
            ++op->verbose;
            break;
        case 'V':
            pr2serr("version: %s\n", version_str);
            return 0;
        case 'S':
            ++op->do_summary;
            break;
        case 'z':
            ++op->do_zero;
            break;
        default:
            pr2serr("unrecognised switch code 0x%x ??\n", c);
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
                pr2serr("Unexpected extra argument: %s\n",
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
    if (0 == op->do_dsn) {
        cp = getenv("SMP_UTILS_DSN");
        if (cp)
            ++op->do_dsn;
    }
    if (op->do_my) {
        op->multiple = 0;
        op->do_summary = 0;
        op->do_num = 1;
    } else if ((0 == op->do_summary) && (0 == op->multiple) &&
               (0 == op->do_num) && (0 == op->phy_id_given))
        ++op->do_summary;
    if (op->do_summary) {
        ++op->do_brief;
        op->multiple = 1;
    }

    res = smp_initiator_open(device_name, subvalue, i_params, op->sa,
                             &tobj, op->verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    if (op->multiple)
        ret = do_multiple(&tobj, op);
    else
        ret = do_single(&tobj, op);
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
