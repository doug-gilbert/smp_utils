/*
 * Copyright (c) 2007-2011 Douglas Gilbert.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"

/* This is a Serial Attached SCSI (SAS) management protocol (SMP) utility
 * program.
 *
 * This utility issues a REPORT EXPANDER ROUTE TABLE LIST request and outputs
 * its response.
 */

static char * version_str = "1.09 20110805";    /* sync with sas2r15 */


static struct option long_options[] = {
        {"brief", no_argument, 0, 'b'},
        {"help", no_argument, 0, 'h'},
        {"hex", no_argument, 0, 'H'},
        {"index", required_argument, 0, 'i'},
        {"interface", required_argument, 0, 'I'},
        {"num", required_argument, 0, 'n'},
        {"phy", 1, 0, 'p'},
        {"sa", 1, 0, 's'},
        {"raw", 0, 0, 'r'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

struct opts_t {
    int do_brief;
    int do_hex;
    int start_rsa_index;
    int do_num;
    int phy_id;
    int do_raw;
    int verbose;
    int sa_given;
    unsigned long long sa;
};

static void
usage()
{
    fprintf(stderr, "Usage: "
          "smp_rep_exp_route_tbl  [--brief] [--help] [--hex] "
          "[--index=IN]\n"
          "                    [--interface=PARAMS] [--num=NUM] [--phy=ID] "
          "[--raw]\n"
          "                    [--sa=SAS_ADDR] ");
    fprintf(stderr,
          "[--verbose] [--version]\n"
          "                    <smp_device>[,<n>]\n"
          "  where:\n"
          "    --brief|-b           brief: abridge output\n"
          "    --help|-h            print out usage message\n"
          "    --hex|-H             print response in hexadecimal\n"
          "    --index=IN|-i IN     starting routed SAS address index "
          "(def: 0)\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --num=NUM|-n NUM     maximum number of descriptors to fetch "
          "(def: 62)\n"
          "    --phy=ID|-p ID       starting phy identifier within bitmap "
          "(def: 0)\n"
          "                         [should be a multiple of 48]\n"
          "    --raw|-r             output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading\n"
          "                                 '0x' or trailing 'h'). "
          "Depending on\n"
          "                                 the interface, may not be "
          "needed\n");
    fprintf(stderr,
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n\n"
          "Performs a SMP REPORT EXPANDER ROUTE TABLE LIST function. "
          "Each descriptor\nin the output contains: a routed SAS address, "
          "a 48 bit phy bitmap and a\nzone group\n"
          );
}

static void
dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

/* Returns 0 when successful, -1 for low level errors and > 0
   for other error categories. */
static int
do_rep_exp_rou_tbl(struct smp_target_obj * top, unsigned char * resp,
                   int max_resp_len, struct opts_t * optsp)
{
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ,
                               SMP_FN_REPORT_EXP_ROUTE_TBL_LIST, 0, 6,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, };
    struct smp_req_resp smp_rr;
    char b[256];
    char * cp;
    int len, res, k, dword_resp_len, act_resplen;

    dword_resp_len = (max_resp_len - 8) / 4;
    smp_req[2] = (dword_resp_len < 0x100) ? dword_resp_len : 0xff;
    smp_req[8] = ((optsp->do_num) >> 8) & 0xff;
    smp_req[9] = optsp->do_num & 0xff;
    smp_req[10] = ((optsp->start_rsa_index) >> 8) & 0xff;
    smp_req[11] = optsp->start_rsa_index & 0xff;
    smp_req[19] = optsp->phy_id;
    if (optsp->verbose) {
        fprintf(stderr, "    Report expander route table request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k) {
            if (0 == (k % 16))
                fprintf(stderr, "\n      ");
            else if (0 == (k % 8))
                fprintf(stderr, " ");
            fprintf(stderr, "%02x ", smp_req[k]);
        }
        fprintf(stderr, "\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = max_resp_len;
    smp_rr.response = resp;
    res = smp_send_req(top, &smp_rr, optsp->verbose);

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
    act_resplen = smp_rr.act_response_len;
    if ((act_resplen >= 0) && (act_resplen < 4)) {
        fprintf(stderr, "response too short, len=%d\n", act_resplen);
        return SMP_LIB_CAT_MALFORMED;
    }
    len = resp[3];
    if ((0 == len) && (0 == resp[2])) {
        len = smp_get_func_def_resp_len(resp[1]);
        if (len < 0) {
            len = 0;
            if (optsp->verbose > 0)
                fprintf(stderr, "unable to determine response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if ((act_resplen >= 0) && (len > act_resplen)) {
        if (optsp->verbose)
            fprintf(stderr, "actual response length [%d] less than deduced "
                    "length [%d]\n", act_resplen, len);
        len = act_resplen; 
    }
    if (optsp->do_hex || optsp->do_raw) {
        if (optsp->do_hex)
            dStrHex((const char *)resp, len, 1);
        else
            dStrRaw((const char *)resp, len);
        if (SMP_FRAME_TYPE_RESP != resp[0])
            return SMP_LIB_CAT_MALFORMED;
        if (resp[1] != smp_req[1])
            return SMP_LIB_CAT_MALFORMED;
        if (resp[2]) {
            if (optsp->verbose)
                fprintf(stderr, "Report expander route table result: %s\n",
                        smp_get_func_res_str(resp[2], sizeof(b), b));
            return resp[2];
        }
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
        fprintf(stderr, "Report expander route table result: %s\n", cp);
        return resp[2];
    }
    return 0;
}


int
main(int argc, char * argv[])
{
    int res, c, len, exp_cc, sphy_id, num_desc, desc_len;
    int k, j, err, off, exp_rtcc;
    long long sa_ll;
    unsigned long long ull;
    char i_params[256];
    char device_name[512];
    unsigned char resp[1020 + 8];
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 0;
    struct opts_t opts;

    memset(&opts, 0, sizeof(opts));
    opts.do_num = 62;   /* maximum fitting in one response */
    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "bhHi:I:n:p:rs:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'b':
            ++opts.do_brief;
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++opts.do_hex;
            break;
        case 'i':
           opts.start_rsa_index = smp_get_num(optarg);
           if ((opts.start_rsa_index < 0) || (opts.start_rsa_index > 65535)) {
                fprintf(stderr, "bad argument to '--index'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'n':
           opts.do_num = smp_get_num(optarg);
           if ((opts.do_num < 0) || (opts.do_num > 65535)) {
                fprintf(stderr, "bad argument to '--num'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'p':
           opts.phy_id = smp_get_num(optarg);
           if ((opts.phy_id < 0) || (opts.phy_id > 254)) {
                fprintf(stderr, "bad argument to '--phy', expect "
                        "value from 0 to 254\n");
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
            fprintf(stderr, "missing device name on command line\n    [Could "
                    "use environment variable SMP_UTILS_DEVICE instead]\n");
            usage();
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if ((cp = strchr(device_name, SMP_SUBVALUE_SEPARATOR))) {
        *cp = '\0';
        if (1 != sscanf(cp + 1, "%d", &subvalue)) {
            fprintf(stderr, "expected number after separator in SMP_DEVICE "
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

    res = smp_initiator_open(device_name, subvalue, i_params, opts.sa,
                             &tobj, opts.verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    ret = do_rep_exp_rou_tbl(&tobj, resp, sizeof(resp), &opts);
    if (ret)
        goto finish;
    if (opts.do_hex || opts.do_raw)
        goto finish;
    len = (resp[3] * 4) + 4;    /* length in bytes excluding CRC field */
    exp_cc = (resp[4] << 8) + resp[5];
    exp_rtcc = (resp[6] << 8) + resp[7];
    desc_len = resp[10];
    num_desc = resp[11];
    sphy_id = resp[19];
    printf("Report expander route table response header:\n");
    if (! opts.do_brief) {
        printf("  expander change count: %d\n", exp_cc);
        printf("  expander route table change count: %d\n", exp_rtcc);
        printf("  self configuring: %d\n", !!(resp[8] & 0x8));
        printf("  zone configuring: %d\n", !!(resp[8] & 0x4));
        printf("  configuring: %d\n", !!(resp[8] & 0x2));
        printf("  zone enabled: %d\n", !!(resp[8] & 0x1));
        printf("  expander route table descriptor length: %d dwords\n", desc_len);
    }
    printf("  number of expander route table descriptors: %d\n", num_desc);
    printf("  first routed SAS address index: %d\n",
           (resp[12] << 8) + resp[13]);
    printf("  last routed SAS address index: %d\n",
           (resp[14] << 8) + resp[15]);
    printf("  starting phy id: %d\n", sphy_id);

    if (len != (32 + (num_desc * (desc_len * 4)))) {
        fprintf(stderr, ">>> Response length of %d bytes doesn't match "
                "%d descriptors, each\n  of %d bytes plus a 32 byte "
                "header and 4 byte CRC\n", len + 4, num_desc, desc_len * 4);
        if (len < (32 + (num_desc * (desc_len * 4)))) {
            ret = SMP_LIB_CAT_MALFORMED;
            goto finish;
        }
    }
    for (k = 0, err = 0; k < num_desc; ++k) {
        off = 32 + (k * (desc_len * 4));
        printf("  descriptor index %d:\n", k);
        for (j = 0, ull = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= resp[off + j];
        }
        printf("    routed SAS address: 0x%llx\n", ull);
        printf("    phy bit map: 0x");
        for (j = 0; j < 6; ++j)
            printf("%02x", resp[off + 8 + j]);
        printf("\n");
        printf("    zone group: %d\n", resp[off + 15]);
    }

finish:
    res = smp_initiator_close(&tobj);
    if (res < 0) {
        if (0 == ret)
            return SMP_LIB_FILE_ERROR;
    }
    if (ret < 0)
        ret = SMP_LIB_CAT_OTHER;
    if (opts.verbose && ret)
        fprintf(stderr, "Exit status %d indicates error detected\n", ret);
    return ret;
}
