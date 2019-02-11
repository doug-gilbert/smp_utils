/*
 * Copyright (c) 2006-2019, Douglas Gilbert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"
#include "sg_unaligned.h"
#include "sg_pr2serr.h"

/* This is a Serial Attached SCSI (SAS) Serial Management Protocol (SMP)
 * utility.
 *
 * This utility issues a DISCOVER LIST function and outputs its response.
 *
 * First defined in SAS-2. From and including SAS-2.1 this function is
 * defined in the SPL series. The most recent SPL-5 draft is spl5r05.pdf .
 */

static const char * version_str = "1.48 20180725";    /* spl5r05 */

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
    bool do_adn;
    bool do_cap_phy;
    bool do_dsn;
    bool desc_type_given;
    bool ign_zp;
    bool num_given;
    bool do_1line;
    bool phy_id_given;
    bool do_raw;
    bool do_summary;
    int do_brief;
    int desc_type;              /* 0 -> full, 1 -> short format */
    int filter;
    int do_hex;
    int do_num;
    int phy_id;
    int verbose;
    uint64_t sa;
    const char * zpi_fn;
    FILE * zpi_filep;
};


static void
usage(void)
{
    pr2serr("Usage: "
            "smp_discover_list  [--adn] [--brief] [--cap] [--descriptor=TY]\n"
            "                          [--dsn] [--filter=FI] [--help] "
            "[--hex] "
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
            "    --dsn|-D             show device slot number in 1 line\n"
            "                         per phy output, if available\n"
            "    --filter=FI|-f FI    phy filter: 0 -> all (def); 1 -> "
            "expander\n"
            "                         attached; 2 -> expander "
            "or SAS SATA\n"
            "                         device; 3 -> SAS SATA (end) device\n"
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
dStrRaw(const uint8_t * str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

/* Returns the number of phys (from REPORT GENERAL response) and if
 * t2t_routingp is non-NULL places 'Table to Table Supported' bit where it
 * points. Returns -3 (or less) -> SMP_LIB errors negated (-4 - smp_err),
 * -1 for other errors. */
static int
get_num_phys(struct smp_target_obj * top, const struct opts_t * op,
             bool * t2t_routingp)
{
    int len, res, k, act_resplen;
    int ret = 0;
    char * cp;
    uint8_t smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_GENERAL, 0, 0,
                         0, 0, 0, 0};
    struct smp_req_resp smp_rr;
    uint8_t * rp = NULL;
    uint8_t * free_rp = NULL;
    char b[256];

    rp = smp_memalign(SMP_FN_REPORT_GENERAL_RESP_LEN, 0, &free_rp, false);
    if (NULL == rp) {
        pr2serr("%s: heap allocation problem\n", __func__);
        return SMP_LIB_RESOURCE_ERROR;
    }
    if (op->verbose) {
        pr2serr("    Report general request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            pr2serr("%02x ", smp_req[k]);
        pr2serr("\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = SMP_FN_REPORT_GENERAL_RESP_LEN;
    smp_rr.response = rp;
    res = smp_send_req(top, &smp_rr, op->verbose);

    if (res) {
        pr2serr("RG smp_send_req failed, res=%d\n", res);
        if (0 == op->verbose)
            pr2serr("    try adding '-v' option for more debug\n");
        ret = -1;
        goto fini;
    }
    if (smp_rr.transport_err) {
        pr2serr("RG smp_send_req transport_error=%d\n",
                smp_rr.transport_err);
        ret = -1;
        goto fini;
    }
    act_resplen = smp_rr.act_response_len;
    if ((act_resplen >= 0) && (act_resplen < 4)) {
        pr2serr("RG response too short, len=%d\n", act_resplen);
        ret = -4 - SMP_LIB_CAT_MALFORMED;
        goto fini;
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
        ret = -4 - SMP_LIB_CAT_MALFORMED;
        goto fini;
    }
    if (rp[1] != smp_req[1]) {
        pr2serr("RG Expected function code=0x%x, got=0x%x\n",
                smp_req[1], rp[1]);
        ret = -4 - SMP_LIB_CAT_MALFORMED;
        goto fini;
    }
    if (rp[2]) {
        if (op->verbose > 1) {
            cp = smp_get_func_res_str(rp[2], sizeof(b), b);
            pr2serr("Report General result: %s\n", cp);
        }
        ret = -4 - rp[2];
        goto fini;
    }
    if (t2t_routingp)
        *t2t_routingp = (len > 10) ? !!(0x80 & rp[10]) : false;
    ret = (len > 9) ? rp[9] : 0;
fini:
    if (free_rp)
        free(free_rp);
    return ret;
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
smp_get_plink_rate(int val, bool prog, int b_len, char * b)
{
    switch (val) {
    case 8:
        snprintf(b, b_len, "1.5 Gbps");
        return b;
    case 9:
        snprintf(b, b_len, "3 Gbps");
        return b;
    case 0xa:
        snprintf(b, b_len, "6 Gbps");
        return b;
    case 0xb:
        snprintf(b, b_len, "12 Gbps");
        return b;
    case 0xc:
        snprintf(b, b_len, "22.5 Gbps");
        return b;
    default:
        break;
    }
    if (prog && (0 == val))
        snprintf(b, b_len, "not programmable");
    else
        snprintf(b, b_len, "reserved [%d]", val);
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
    case 5:     /* hardware muxing made obsolete in spl5r01 */
        snprintf(b, b_len, "error in multiplexing (MUX) sequence");
        break;
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
    case 0xc: snprintf(b, b_len, "phy enabled, 22.5 Gbps"); break;
    default: snprintf(b, b_len, "reserved [%d]", val); break;
    }
    return b;
}

/* Returns 0 when successful, -1 for low level errors and > 0
   for other error categories. */
static int
do_discover_list(struct smp_target_obj * top, int sphy_id,
                 uint8_t * resp, int max_resp_len,
                 struct opts_t * op)
{
    uint8_t smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_DISCOVER_LIST, 0, 6,
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
            hex2stdout(resp, len, 1);
        else
            dStrRaw(resp, len);
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

static const char * g_name[] = {"G1", "G2", "G3", "G4", "G5"};
static const char * g_name_long[] =
        {"G1 (1.5 Gbps)", "G2 (3 Gbps)", "G3 (6 Gbps)", "G4 (12 Gbps)",
         "G5 (22.5 Gbps)"};

/* Taken from spl5r02 SNW-3 table 70 on page 199. Note that the "Requested
 * logical link rate" field became obsolete in spl5r01 when multiplexing
 * was removed. */
static void
decode_phy_cap(unsigned int p_cap, const struct opts_t * op)
{
    bool prev_nl;
    int k, skip;
    unsigned int g15_val, g;
    const char * cp;

    printf("    Tx SSC type: %d, Requested interleaved SPL: %d, [Req logical "
           "lr: 0x%x]\n", ((p_cap >> 30) & 0x1), (p_cap >> 28) & 0x3,
           (p_cap >> 24) & 0xf);
    prev_nl = true;
    g15_val = (p_cap >> 14) & 0x3ff;
    for (skip = 0, k = 4; k >= 0; --k) {
        cp = op->verbose ? g_name_long[4 - k] : g_name[4 - k];
        g = (g15_val >> (k * 2)) & 0x3;
        switch (g) {
        case 0:
            ++skip;
            break;
        case 1:
            printf("    %s: with SSC", cp);
            prev_nl = false;
            break;
        case 2:
            printf("    %s: without SSC", cp);
            prev_nl = false;
            break;
        case 3:
            printf("    %s: with+without SSC", cp);
            prev_nl = false;
            break;
        default:
            printf("    %s: g15_val=0x%x, k=%d", cp, g15_val, k);
            prev_nl = false;
            break;
        }
        if ((3 == k) && (0 == skip)) {
            printf("\n");
            skip = 2;
            prev_nl = true;
        }
        if ((1 == k) && (skip < 2)) {
            printf("\n");
            prev_nl = true;
        }
    }
    if (! prev_nl)
        printf("\n");
    printf("    Extended coefficient settings: %d\n", (p_cap >> 1) & 0x1);
}

/* long format: as described in (full, single) DISCOVER response
 * Returns 0 for okay, else -1 . */
static int
decode_desc0_multiline(const uint8_t * rp, int hdr_ecc, struct opts_t * op)
{
    unsigned int ui;
    int func_res, phy_id, ecc, adt, route_attr, len;
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
    ecc = sg_get_unaligned_be16(rp + 4);
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

    printf("  SAS address: 0x%" PRIx64 "\n", sg_get_unaligned_be64(rp + 16));
    printf("  attached SAS address: 0x%" PRIx64 "\n",
           sg_get_unaligned_be64(rp + 24));
    printf("  attached phy identifier: %d\n", rp[32]);
    if (0 == op->do_brief) {
        printf("  attached persistent capable: %d\n", !!(rp[33] & 0x80));
        printf("  attached power capable: %d\n", ((rp[33] >> 5) & 0x3));
        printf("  attached slumber capable: %d\n", !!(rp[33] & 0x10));
        printf("  attached partial capable: %d\n", !!(rp[33] & 0x8));
        printf("  attached inside ZPSDS persistent: %d\n", !!(rp[33] & 4));
        printf("  attached requested inside ZPSDS: %d\n", !!(rp[33] & 2));
        printf("  attached break_reply capable: %d\n", !!(rp[33] & 1));
        printf("  attached apta capable: %d\n", !!(rp[34] & 4));
        printf("  attached smp priority capable: %d\n", !!(rp[34] & 2));
        printf("  attached pwr_dis capable: %d\n", !!(rp[34] & 1));
        printf("  programmed minimum physical link rate: %s\n",
               smp_get_plink_rate(((rp[40] >> 4) & 0xf), true,
                                  sizeof(b), b));
        printf("  hardware minimum physical link rate: %s\n",
               smp_get_plink_rate((rp[40] & 0xf), false, sizeof(b), b));
        printf("  programmed maximum physical link rate: %s\n",
               smp_get_plink_rate(((rp[41] >> 4) & 0xf), true,
                                  sizeof(b), b));
        printf("  hardware maximum physical link rate: %s\n",
               smp_get_plink_rate((rp[41] & 0xf), false,
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
           smp_get_connector_type_str((rp[45] & 0x7f), true, sizeof(b), b));
    printf("  connector element index: %d\n", rp[46]);
    printf("  connector physical link: %d\n", rp[47]);
    printf("  phy power condition: %s\n",
           smp_get_phy_pwr_cond_str(((rp[48] & 0xc0) >> 6), sizeof(b), b));
    printf("  sas slumber capable: %d\n", !!(rp[48] & 0x8));
    printf("  sas partial capable: %d\n", !!(rp[48] & 0x4));
    printf("  sata slumber capable: %d\n", !!(rp[48] & 0x2));
    printf("  sata partial capable: %d\n", !!(rp[48] & 0x1));
    printf("  pwr_dis signal: %s\n",
           smp_get_pwr_dis_signal_str(((rp[49] & 0xc0) >> 6),
                                      sizeof(b), b));
    printf("  pwr_dis control capable: %d\n", (rp[49] & 0x30) >> 4);
    printf("  sas slumber enabled: %d\n", !!(rp[49] & 0x8));
    printf("  sas partial enabled: %d\n", !!(rp[49] & 0x4));
    printf("  sata slumber enabled: %d\n", !!(rp[49] & 0x2));
    printf("  sata partial enabled: %d\n", !!(rp[49] & 0x1));
    if (len > 59) {
        printf("  attached device name: 0x%" PRIx64 "\n",
               sg_get_unaligned_be64(rp + 52));
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
        printf("  self-configuration sas address: 0x%" PRIx64 "\n",
               sg_get_unaligned_be64(rp + 68));
        ui = sg_get_unaligned_be32(rp + 76);
        printf("  programmed phy capabilities: 0x%x\n", ui);
        if (op->do_cap_phy)
            decode_phy_cap(ui, op);
        ui = sg_get_unaligned_be32(rp + 80);
        printf("  current phy capabilities: 0x%x\n", ui);
        if (op->do_cap_phy)
            decode_phy_cap(ui, op);
        ui = sg_get_unaligned_be32(rp + 84);
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
        /* hardware muxing made obsolete in spl5r01 */
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
        ui = rp[109];
        printf("  device slot group number: ");
        if (255 == ui)
            printf("not available\n");
        else
            printf("%d\n", ui);
    }
    if (len > 115)
        printf("  device slot group output connector: %.6s\n", rp + 110);
    if (len > 117)
        printf("  STP buffer size: %u\n", sg_get_unaligned_be16(rp + 116));
    if (len > 118)
        printf("  Buffered phy burst size (KiB): %u\n", rp[118]);
    return 0;
}

/* short format: only DISCOVER LIST has this abridged 24 byte descriptor.
 * Returns 0 for okay, else -1 . */
static int
decode_desc1_multiline(const uint8_t * rp, bool z_enabled, struct opts_t * op)
{
    int func_res, phy_id, adt, route_attr;
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
    printf("  attached SAS address: 0x%" PRIx64 "\n",
           sg_get_unaligned_be64(rp + 12));
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
    printf("  Buffered phy burst size (KiB): %u\n", rp[20]);
    return 0;
}

/* Decodes either descriptor type into one line output. This is a
 * "per phy" function. Returns 0 for ok, 1 for ok plus zoning enabled and
  * seen ZG other than 1, else -1 (for problem) . */
static int
decode_1line(const uint8_t * rp, int len, int desc, bool z_enabled,
             int has_t2t, struct opts_t * op)
{
    bool plus;
    bool zg_not1 = true;
    int phy_id, off, negot, adt, route_attr, vp, asa_off;
    int func_res, aphy_id, a_init, a_target, z_group, iz_mask;
    uint64_t ull, adn;
    const char * cp;
    char b[256];
    char dsn[10] = "";

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

    if (op->do_dsn && (0 == desc) && (len > 108) && (0xff != rp[108]))
        sprintf(dsn, "  dsn=%d", rp[108]);

    switch (negot) {
    case 1:
        printf("  phy %3d:%s:disabled%s\n", phy_id, cp, dsn);
        return 0;
    case 2:
        printf("  phy %3d:%s:reset problem%s\n", phy_id, cp, dsn);
        return 0;
    case 3:
        printf("  phy %3d:%s:spinup hold%s\n", phy_id, cp, dsn);
        return 0;
    case 4:
        printf("  phy %3d:%s:port selector%s\n", phy_id, cp, dsn);
        return 0;
    case 5:
        printf("  phy %3d:%s:reset in progress%s\n", phy_id, cp, dsn);
        return 0;
    case 6:
        printf("  phy %3d:%s:unsupported phy attached%s\n", phy_id, cp, dsn);
        return 0;
    default:
        /* keep going */
        break;
    }
    if ((0 == op->verbose) && (0 == adt) && op->do_brief)
        return 0;
    ull = sg_get_unaligned_be64(rp + asa_off);
    if ((0 == adt) || (adt > 3)) {
        printf("  phy %3d:%s:attached:[0000000000000000:00]", phy_id, cp);
        if ((op->do_brief > 1) || op->do_adn) {
            printf("\n");
            return 0;
        }
        if (z_enabled && (1 != z_group)) {
            zg_not1 = true;
            printf("  ZG:%d", z_group);
        }
        if ('\0' != dsn[0])
             printf("%s", dsn);
        printf("\n");
        return (int)zg_not1;
    }
    if ((0 == desc) && op->do_adn) {
        adn = sg_get_unaligned_be64(rp + 52);
        printf("  phy %3d:%s:attached:[%016" PRIx64 ":%02d %016" PRIx64
               " %s%s", phy_id, cp, ull, aphy_id, adn,
               smp_short_attached_device_type[adt], (vp ? " V" : ""));
    } else
        printf("  phy %3d:%s:attached:[%016" PRIx64 ":%02d %s%s", phy_id, cp,
               ull, aphy_id, smp_short_attached_device_type[adt],
               (vp ? " V" : ""));
    if (a_init & 0xf) {
        off = 0;
        plus = false;
        off += snprintf(b + off, sizeof(b) - off, " i(");
        if (a_init & 0x8) {
            off += snprintf(b + off, sizeof(b) - off, "SSP");
            plus = true;
        }
        if (a_init & 0x4) {
            off += snprintf(b + off, sizeof(b) - off, "%sSTP",
                            (plus ? "+" : ""));
            plus = true;
        }
        if (a_init & 0x2) {
            off += snprintf(b + off, sizeof(b) - off, "%sSMP",
                            (plus ? "+" : ""));
            plus = true;
        }
        if (a_init & 0x1) {
            off += snprintf(b + off, sizeof(b) - off, "%sSATA",
                            (plus ? "+" : ""));
            plus = true;
        }
        printf("%s)", b);
    }
    if (a_target & 0xf) {
        off = 0;
        plus = false;
        off += snprintf(b + off, sizeof(b) - off, " t(");
        if (a_target & 0x80) {
            off += snprintf(b + off, sizeof(b) - off, "PORT_SEL");
            plus = true;
        }
        if (a_target & 0x8) {
            off += snprintf(b + off, sizeof(b) - off, "%sSSP",
                            (plus ? "+" : ""));
            plus = true;
        }
        if (a_target & 0x4) {
            off += snprintf(b + off, sizeof(b) - off, "%sSTP",
                            (plus ? "+" : ""));
            plus = true;
        }
        if (a_target & 0x2) {
            off += snprintf(b + off, sizeof(b) - off, "%sSMP",
                            (plus ? "+" : ""));
            plus = true;
        }
        if (a_target & 0x1) {
            off += snprintf(b + off, sizeof(b) - off, "%sSATA",
                            (plus ? "+" : ""));
            plus = true;
        }
        printf("%s)", b);
    }
    printf("]");
    if ((op->do_brief < 2) && (! op->do_adn)) {
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
        case 0xc:
            cp = "  22.5 Gbps";
            break;
        default:
            cp = "";
            break;
        }
        printf("%s", cp);
        if (z_enabled && (1 != z_group)) {
            zg_not1 = true;
            printf("  ZG:%d", z_group);
        }
        if ('\0' != dsn[0])
            printf("%s", dsn);
    }
    printf("\n");
    return (int)zg_not1;
}

static void
output_header_info(const uint8_t * rp, struct opts_t * op)
{
    bool z_enabled;
    int hdr_ecc, sphy_id;

    hdr_ecc = sg_get_unaligned_be16(rp + 4);
    sphy_id = rp[8];
    z_enabled = !!(rp[16] & 0x40);

    if (op->zpi_fn) {
        if (0 == op->do_brief) {
            fprintf(op->zpi_filep, "# Zone phy information from DISCOVER "
                    "LIST:\n");
            fprintf(op->zpi_filep, "#  expander change count: %d\n",
                    hdr_ecc);
            fprintf(op->zpi_filep, "#  starting phy id: %d\n", sphy_id);
            fprintf(op->zpi_filep, "#  maximum number of phys output: %d\n",
                    op->do_num);
            fprintf(op->zpi_filep, "#  zoning enabled: %d\n", (int)z_enabled);
            fprintf(op->zpi_filep, "#\n# Values below are in hex, phy_id in "
                    "first column, zone group in last\n");
        }
    } else {
        if (! op->do_1line) {
            printf("Discover list response header:\n");
            printf("  starting phy id: %d\n", sphy_id);
            printf("  number of discover list descriptors: %d\n", rp[9]);
        }
        if ((! op->do_1line) && (0 == op->do_brief)) {
            printf("  expander change count: %d\n", hdr_ecc);
            printf("  filter: %d\n", rp[10] & 0xf);
            printf("  descriptor type: %d\n", rp[11] & 0xf);
            printf("  discover list descriptor length: %d bytes\n",
                   rp[12] * 4);
            printf("  zoning supported: %d\n", !!(rp[16] & 0x80));
            printf("  zoning enabled: %d\n", (int)z_enabled);
            printf("  self configuring: %d\n", !!(rp[16] & 0x8));
            printf("  zone configuring: %d\n", !!(rp[16] & 0x4));
            printf("  configuring: %d\n", !!(rp[16] & 0x2));
            printf("  externally configurable route table: %d\n",
                   !!(rp[16] & 0x1));
            printf("  last self-configuration status descriptor index: %u\n",
                   sg_get_unaligned_be16(rp + 18));    /* sas2r12 */
            printf("  last phy event list descriptor index: %u\n",
                   sg_get_unaligned_be16(rp + 20));
        }
    }
}


int
main(int argc, char * argv[])
{
    bool has_t2t = false;
    bool no_more;
    bool z_enabled = false;
    bool zg_not1 = false;
    int res, c, len, hdr_ecc, num_desc, resp_filter, resp_desc_type;
    int desc_len, k, j, err, off, adt, fresult, num;
    int ret = 0;
    int subvalue = 0;
    int64_t sa_ll;
    char * cp;
    struct opts_t * op;
    char i_params[256];
    char device_name[512];
    const uint32_t resp_sz = 1020 + 8;
    uint8_t * resp = NULL;
    uint8_t * free_resp = NULL;
    struct smp_target_obj tobj;
    struct opts_t opts;

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
            op->do_adn = true;
            break;
        case 'b':
            ++op->do_brief;
            break;
        case 'c':
            op->do_cap_phy = true;
            break;
        case 'd':
           op->desc_type = smp_get_num(optarg);
           if ((op->desc_type < 0) || (op->desc_type > 15)) {
                pr2serr("bad argument to '--desc'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            op->desc_type_given = true;
            break;
        case 'D':
            op->do_dsn = true;
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
            op->ign_zp = true;
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
            op->num_given = true;
            break;
        case 'o':
            op->do_1line = true;
            break;
        case 'p':
           op->phy_id = smp_get_num(optarg);
           if ((op->phy_id < 0) || (op->phy_id > 254)) {
                pr2serr("bad argument to '--phy=', expect value from 0 to "
                        "254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            op->phy_id_given = true;
            break;
        case 'r':
            op->do_raw = true;
            break;
        case 's':
           sa_ll = smp_get_llnum_nomult(optarg);
           if (-1LL == sa_ll) {
                pr2serr("bad argument to '--sa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            op->sa = (uint64_t)sa_ll;
            break;
        case 'S':
            op->do_summary = true;
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
                    "environment variable SMP_UTILS_DEVICE instead]\n\n");
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
           sa_ll = smp_get_llnum_nomult(cp);
           if (-1LL == sa_ll) {
                pr2serr("bad value in environment variable "
                        "SMP_UTILS_SAS_ADDR\n");
                pr2serr("    use 0\n");
                sa_ll = 0;
            }
            op->sa = (uint64_t)sa_ll;
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
            op->do_dsn = false;
        }
    }
    if (! op->do_dsn) {
        cp = getenv("SMP_UTILS_DSN");
        if (cp)
            op->do_dsn = true;
    }
    if (op->do_summary || op->do_1line || op->num_given ||
        op->phy_id_given || op->zpi_fn)
        ;
    else
        op->do_summary = true;
    if (op->zpi_fn) {
        if (op->do_summary || op->desc_type_given || op->filter ||
            op->do_adn) {
            pr2serr("--zpi=FN clashes with --summary, --adn, --filter and "
                    "--descriptor=TY options\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
        if (! op->num_given)
            op->do_num = 254;
        op->do_1line = true;
        op->desc_type = 1;
        op->ign_zp = true;        /* set --ignore */
    }
    if (! op->desc_type_given) {
        op->desc_type = op->do_brief ? 1 : 0;
        if (op->do_adn || op->do_dsn)
            op->desc_type = 0;
    }
    if (op->do_summary) {
        ++op->do_brief;
        if ((! op->desc_type_given) && (! op->do_adn) && (! op->do_dsn))
            op->desc_type = 1;
        op->do_1line = true;
        op->do_num = 254;
    } else if (! (op->num_given || op->zpi_fn))
        op->do_num = 1;
    if (op->do_adn && (1 == op->desc_type)) {
        pr2serr("--adn and --descriptor=1 options clash since there is no "
                "'attached\ndevice name' field in the short format. "
                "Ignoring --adn .\n");
        op->do_adn = false;
    }
    resp = smp_memalign(resp_sz, 0, &free_resp, op->verbose > 3);
    if (NULL == resp) {
        pr2serr("Could not allocate %u bytes on heap\n", resp_sz);
        return SMP_LIB_RESOURCE_ERROR;
    }

    res = smp_initiator_open(device_name, subvalue, i_params, op->sa,
                             &tobj, op->verbose);
    if (res < 0) {
        ret = SMP_LIB_FILE_ERROR;
        goto err_out;
    }

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
    num = get_num_phys(&tobj, op, &has_t2t);
    if (num <= 0)
        num = op->do_num;
    else {
        if (op->phy_id >= num) {
            printf("Given phy_id=%d equals or exceeds number of phys (%d)\n",
                   op->phy_id, num);
            ret = 0;    /* could treat as error */
            goto err_out;
        }
        num -= op->phy_id;
        num = (num < op->do_num) ? num : op->do_num;
    }
    no_more = false;
    for (j = 0; (j < num) && (! no_more); j += num_desc) {
        memset(resp, 0, resp_sz);
        if ((op->phy_id + j) > 254) {
            ret = 0;    /* off the end so not error */
            break;
        }
        ret = do_discover_list(&tobj, op->phy_id + j, resp, resp_sz, op);
        if (ret) {
            if (SMP_FRES_NO_PHY == ret)
                ret = 0;    /* off the end so not error */
            break;
        }
        num_desc = resp[9];
        if ((0 == op->desc_type) && (num_desc < MAX_DLIST_LONG_DESCS))
            no_more = true;
        if ((1 == op->desc_type) && (num_desc < MAX_DLIST_SHORT_DESCS))
            no_more = true;
        if (op->do_hex || op->do_raw)
            continue;
        len = (resp[3] * 4) + 4;    /* length in bytes excluding CRC field */
        if ((0 == j) && ((! op->do_1line) || op->zpi_fn))
            output_header_info(resp, op);
        hdr_ecc = sg_get_unaligned_be16(resp + 4);
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
                res = decode_1line(resp + off, desc_len, resp_desc_type,
                                   z_enabled, has_t2t, op);
                if (res < 0)
                    ++err;
                else if (res > 0)
                    zg_not1 = true;
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
    if (zg_not1 && (0 == op->do_brief) && (NULL == op->zpi_fn))
        printf("Zoning %sabled\n", z_enabled ? "en" : "dis");

err_out:
    if (free_resp)
        free(free_resp);
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


