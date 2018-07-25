/*
 * Copyright (c) 2006-2018, Douglas Gilbert
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
 * This utility issues a REPORT ROUTE INFORMATION function and outputs its
 * response.
 */

static const char * version_str = "1.16 20180725";

#define REP_ROUTE_INFO_RESP_LEN 44

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"index", required_argument, 0, 'i'},
    {"interface", required_argument, 0, 'I'},
    {"multiple", no_argument, 0, 'm'},
    {"num", required_argument, 0, 'n'},
    {"phy", required_argument, 0, 'p'},
    {"raw", no_argument, 0, 'r'},
    {"sa", required_argument, 0, 's'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"zero", no_argument, 0, 'z'},
    {0, 0, 0, 0},
};


static void
usage(void)
{
    pr2serr("Usage: smp_rep_route_info [--help] [--hex] [--index=IN] "
            "[--interface=PARAMS]\n"
            "                          [--multiple] [--num=NUM] [--phy=ID] "
            "[--raw]\n"
            "                          [--sa=SAS_ADDR] [--verbose] "
            "[--version]\n"
            "                          [--zero] SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --help|-h         print out usage message\n"
            "    --hex|-H          print response in hexadecimal\n"
            "    --index=IN|-i IN    expander route index (def: 0)\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --multiple|-m     query multiple indexes, output 1 "
            "line for each\n"
            "    --num=NUM|-n NUM  number of indexes to examine when '-m' "
            "is given\n"
            "    --phy=ID|-p ID    phy identifier (def: 0)\n"
            "    --raw|-r          output response in binary\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending on\n"
            "                                 the interface, may not be "
            "needed\n"
            "    --verbose|-v      increase verbosity\n"
            "    --version|-V      print version string and exit\n"
            "    --zero|-z         zero Allocated Response Length "
            "field,\n"
            "                      may be required prior to SAS-2\n\n"
            "Performs a SMP REPORT ROUTE INFORMATION function\n"
           );
}

static void
dStrRaw(const uint8_t * str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

/* Returns 0 for success and sets *resp_len to length of response in
 * bytes excluding trailing CRC. Returns SMP_LIB errors or
 * -1 for other errors.
 */
static int
do_rep_route(struct smp_target_obj * top, int phy_id, int index,
             uint8_t * resp, int max_resp_len, int * resp_len,
             bool do_zero, int do_hex, bool do_raw, int verbose)
{
    int len, res, k, act_resplen;
    char * cp;
    uint8_t smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_ROUTE_INFO, 0, 0,
                         0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0};
    char b[256];
    struct smp_req_resp smp_rr;

    if (! do_zero) {     /* SAS-2 or later */
        len = (max_resp_len - 8) / 4;
        smp_req[2] = (len < 0x100) ? len : 0xff; /* Allocated Response Len */
        smp_req[3] = 2; /* Request Length: in dwords */
    }
    sg_put_unaligned_be16(index, smp_req + 6);
    smp_req[9] = phy_id;
    if (verbose) {
        pr2serr("    Report route information request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            pr2serr("%02x ", smp_req[k]);
        pr2serr("\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = max_resp_len;
    smp_rr.response = resp;
    res = smp_send_req(top, &smp_rr, verbose);

    if (res) {
        pr2serr("smp_send_req failed, res=%d\n", res);
        if (0 == verbose)
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
            if (verbose > 1)
                pr2serr("unable to determine response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if ((act_resplen >= 0) && (len > act_resplen)) {
        if (verbose)
            pr2serr("actual response length [%d] less than deduced length "
                    "[%d]\n", act_resplen, len);
        len = act_resplen;
    }
    if (do_hex || do_raw) {
        if (do_hex)
            hex2stdout(resp, len, 1);
        else
            dStrRaw(resp, len);
        if (SMP_FRAME_TYPE_RESP != resp[0])
            return SMP_LIB_CAT_MALFORMED;
        if (resp[1] != smp_req[1])
            return SMP_LIB_CAT_MALFORMED;
        if (resp[2]) {
            if (verbose)
                pr2serr("Report route information result: %s\n",
                        smp_get_func_res_str(resp[2], sizeof(b), b));
            return resp[2];
        }
        if (resp_len)
            *resp_len = len;
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
        if (verbose > 0) {
            cp = smp_get_func_res_str(resp[2], sizeof(b), b);
            pr2serr("Report route information result: %s\n", cp);
        }
        return resp[2];
    }
    if (resp_len)
        *resp_len = len;
    return 0;
}

#define MAX_NUM_INDEXES 16384
#define MAX_ADJACENT_DISABLED 4

static int
do_multiple(struct smp_target_obj * top, int phy_id, int index, int num_ind,
            bool do_zero, int do_hex, bool do_raw, int verbose)
{
    bool disabled;
    bool first = true;
    int res, len, k, num, adj_dis;
    uint8_t smp_resp[REP_ROUTE_INFO_RESP_LEN];

    num = num_ind ? (index + num_ind) : MAX_NUM_INDEXES;
    for (adj_dis = 0, k = index; k < num; ++k) {
        res = do_rep_route(top, phy_id, k, smp_resp, sizeof(smp_resp),
                           &len, do_zero, do_hex, do_raw, verbose);
        if (SMP_FRES_NO_INDEX == res)
            return 0;   /* expected, end condition */
        if (res)
            return res;
        if (first && (! do_raw)) {
            first = false;
            printf("Route table for phy_id: %d\n", phy_id);
        }
        if (do_hex || do_raw)
            continue;
        disabled = !!(smp_resp[12] & 0x80);
        if ((0 == num_ind) && disabled) {
            if (++adj_dis >= MAX_ADJACENT_DISABLED) {
                if (verbose > 2)
                    pr2serr("number of 'adjacent disables' exceeded at "
                            "index=%d\n", k);
                break;
            }
        }
        if (disabled)
            continue;
        adj_dis = 0;
        printf("  Index: %d    Routed SAS address: 0x%" PRIx64 "\n", k,
               sg_get_unaligned_be64(smp_resp + 16));
    }
    return 0;
}

static int
do_single(struct smp_target_obj * top, int phy_id, int index, bool do_zero,
          int do_hex, bool do_raw, int verbose)
{
    int res, len;
    uint8_t smp_resp[REP_ROUTE_INFO_RESP_LEN];

    res = do_rep_route(top, phy_id, index, smp_resp, sizeof(smp_resp),
                       &len, do_zero, do_hex, do_raw, verbose);
    if (res)
        return res;
    if (do_hex || do_raw)
        return 0;

    printf("Report route information response:\n");
    res = sg_get_unaligned_be16(smp_resp + 4);
    if (verbose || (res > 0))
        printf("  expander change count: %d\n", res);
    printf("  expander route index: %d\n",
           sg_get_unaligned_be16(smp_resp + 6));
    printf("  phy identifier: %d\n", smp_resp[9]);
    printf("  expander route entry disabled: %d\n", !!(smp_resp[12] & 0x80));
    if ((! (smp_resp[12] & 0x80)) || (verbose > 0)) {
        printf("  routed SAS address: 0x%" PRIx64 "\n",
               sg_get_unaligned_be64(smp_resp + 16));
    }
    return 0;
}


int
main(int argc, char * argv[])
{
    bool do_raw = false;
    bool do_zero = false;
    bool multiple = false;
    int res, c;
    int do_hex = 0;
    int do_num = 0;
    int er_ind = 0;
    int phy_id = 0;
    int ret = 0;
    int subvalue = 0;
    int verbose = 0;
    int64_t sa_ll;
    uint64_t sa = 0;
    char * cp;
    char i_params[256];
    char device_name[512];
    char b[256];
    struct smp_target_obj tobj;

    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "hHi:I:mn:p:rs:vVz", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++do_hex;
            break;
        case 'i':
           er_ind = smp_get_num(optarg);
           if ((er_ind < 0) || (er_ind > 65535)) {
                pr2serr("bad argument to '--index', expect value from 0 to "
                        "65535\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'm':
            multiple = true;
            break;
        case 'n':
           do_num = smp_get_num(optarg);
           if ((do_num < 0) || (do_num > 16382)) {
                pr2serr("bad argument to '--num', expect value from 0 to "
                        "16382\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'p':
           phy_id = smp_get_num(optarg);
           if ((phy_id < 0) || (phy_id > 254)) {
                pr2serr("bad argument to '--phy', expect value from 0 to "
                        "254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'r':
            do_raw = true;
            break;
        case 's':
           sa_ll = smp_get_llnum_nomult(optarg);
           if (-1LL == sa_ll) {
                pr2serr("bad argument to '--sa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            sa = (uint64_t)sa_ll;
            break;
        case 'v':
            ++verbose;
            break;
        case 'V':
            pr2serr("version: %s\n", version_str);
            return 0;
        case 'z':
            do_zero = true;
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
    if (0 == sa) {
        cp = getenv("SMP_UTILS_SAS_ADDR");
        if (cp) {
           sa_ll = smp_get_llnum_nomult(cp);
           if (-1LL == sa_ll) {
                pr2serr("bad value in environment variable "
                        "SMP_UTILS_SAS_ADDR\n    use 0\n");
                sa_ll = 0;
            }
            sa = (uint64_t)sa_ll;
        }
    }
    if (sa > 0) {
        if (! smp_is_naa5(sa)) {
            pr2serr("SAS (target) address not in naa-5 format (may need "
                    "leading '0x')\n");
            if ('\0' == i_params[0]) {
                pr2serr("    use '--interface=' to override\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
        }
    }
    if (verbose > 2)
            pr2serr("  phy_id=%d  expander_route_index=%d\n", phy_id, er_ind);

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    if (multiple)
        ret = do_multiple(&tobj, phy_id, er_ind, do_num, do_zero, do_hex,
                          do_raw, verbose);
    else
        ret = do_single(&tobj, phy_id, er_ind, do_zero, do_hex, do_raw,
                        verbose);

    if ((0 == verbose) && ret) {
        if (SMP_LIB_CAT_MALFORMED == ret)
            pr2serr("Report route information malformed response\n");
        else {
            cp = smp_get_func_res_str(ret, sizeof(b), b);
            pr2serr("Report route information result: %s\n", cp);
        }
    }

    res = smp_initiator_close(&tobj);
    if (res < 0) {
        pr2serr("close error: %s\n", safe_strerror(errno));
        if (0 == ret)
            return SMP_LIB_FILE_ERROR;
    }
    if (ret < 0)
        ret = SMP_LIB_CAT_OTHER;
    if (verbose && ret)
        pr2serr("Exit status %d indicates error detected\n", ret);
    return ret;
}


