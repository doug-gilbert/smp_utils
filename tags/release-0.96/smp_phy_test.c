/*
 * Copyright (c) 2006-2011 Douglas Gilbert.
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
#include <ctype.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "smp_lib.h"

/* This is a Serial Attached SCSI (SAS) management protocol (SMP) utility
 * program.
 *
 * This utility issues a PHY TEST FUNCTION function and outputs its response.
 */

static char * version_str = "1.08 20110505"; /* sync with sas2r15 */


static struct option long_options[] = {
        {"control", 1, 0, 'c'},
        {"dwords", 1, 0, 'd'},
        {"expected", 1, 0, 'E'},
        {"function", 1, 0, 'f'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"interface", 1, 0, 'I'},
        {"linkrate", 1, 0, 'l'},
        {"pattern", 1, 0, 'P'},
        {"phy", 1, 0, 'p'},
        {"sa", 1, 0, 's'},
        {"sata", 0, 0, 't'},
        {"spread", 1, 0, 'S'},
        {"raw", 0, 0, 'r'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

static void
usage()
{
    fprintf(stderr, "Usage: "
          "smp_phy_test [--control=CO] [--dwords=DW] [--expected=EX]\n"
          "                    [--function=FN] [--help] [--hex] "
          "[--interface=PARAMS]\n"
          "                    [--linkrate=LR] [--pattern=PA] [--phy=ID]\n"
          "                    [--raw] [--sa=SAS_ADDR] [--sata] "
          "[--spread=SP]\n"
          "                    [--verbose] [--version] SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --control=CO|-c CO     phy test pattern dwords control "
          "(def: 0)\n"
          "    --dwords=DW|-d DW      phy test pattern dwords (def:0)\n"
          "    --expected=EX|-E EX    set expected expander change count "
          "to EX\n"
          "    --function=FN|-f FN    phy test function (def:0 -> stop)\n"
          "    --help|-h              print out usage message\n"
          "    --hex|-H               print response in hexadecimal\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --linkrate=LR|-l LR    physical link rate (def: 9 -> "
          "3 Gbps)\n"
          "    --pattern=PA|-P PA     phy test pattern (def: 2 -> "
          "CJTPAT)\n"
          "    --phy=ID|-p ID         phy identifier (def: 0)\n"
          "    --raw|-r               output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading\n"
          "                                 '0x' or trailing 'h'). Depending "
          "on the\n"
          "                                 interface, may not be needed\n"
          "    --sata|-t              set phy test function SATA bit "
          "(def: 0)\n"
          "    --spread=SC|-S SC      set phy test function SCC to SC "
          "(def: 0)\n"
          "    --verbose|-v           increase verbosity\n"
          "    --version|-V           print version string and exit\n\n"
          "Performs a SMP PHY TEST FUNCTION function\n"
          );
}

static void
dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

int
main(int argc, char * argv[])
{
    int res, c, k, len;
    int do_control = 0;
    unsigned long long dwords = 0;
    int expected_cc = 0;
    int do_function = 0;
    int do_hex = 0;
    int linkrate = 9;   /* 3 Gbps */
    int pattern = 2;    /* CJTPAT */
    int phy_id = 0;
    int do_raw = 0;
    int do_sata = 0;
    int do_ssc = 0;
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_PHY_TEST_FUNCTION,
        0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
    unsigned char smp_resp[128];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 0;

    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "c:d:E:f:hHI:l:p:P:rs:S:tvV",
                        long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'c':
            do_control = smp_get_num(optarg);
            if ((do_control < 0) || (do_control > 255)) {
                fprintf(stderr, "bad argument to '--control', expect "
                        "value from 0 to 255\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'd':
            if (0 == strcmp("-1", optarg))
                dwords = 0xffffffffffffffffULL;
            else {
                sa_ll = smp_get_llnum(optarg);
                if (-1LL == sa_ll) {
                    fprintf(stderr, "bad argument to '--dwords'\n");
                    return SMP_LIB_SYNTAX_ERROR;
                }
                dwords = (unsigned long long)sa_ll;
            }
            break;
        case 'E':
            expected_cc = smp_get_num(optarg);
            if ((expected_cc < 0) || (expected_cc > 65535)) {
                fprintf(stderr, "bad argument to '--expected'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'f':
            do_function = smp_get_num(optarg);
            if ((do_function < 0) || (do_function > 255)) {
                fprintf(stderr, "bad argument to '--function', expect "
                        "value from 0 to 255\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++do_hex;
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'l':
            linkrate = smp_get_num(optarg);
            if ((linkrate < 0) || (linkrate > 15)) {
                fprintf(stderr, "bad argument to '--linkrate', expect "
                        "value from 0 to 15\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'p':
            phy_id = smp_get_num(optarg);
            if ((phy_id < 0) || (phy_id > 254)) {
                fprintf(stderr, "bad argument to '--phy', expect "
                        "value from 0 to 254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'P':
            pattern = smp_get_num(optarg);
            if ((pattern < 0) || (pattern > 255)) {
                fprintf(stderr, "bad argument to '--pattern', expect "
                        "value from 0 to 255\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'r':
            ++do_raw;
            break;
        case 's':
            sa_ll = smp_get_llnum(optarg);
            if (-1LL == sa_ll) {
                fprintf(stderr, "bad argument to '--sa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            sa = (unsigned long long)sa_ll;
            break;
        case 'S':
            do_ssc = smp_get_num(optarg);
            if ((do_ssc < 0) || (do_ssc > 3)) {
                fprintf(stderr, "bad argument to '--spread', expect "
                        "value from 0 to 3\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 't':
            ++do_sata;
            break;
        case 'v':
            ++verbose;
            break;
        case 'V':
            fprintf(stderr, "version: %s\n", version_str);
            return 0;
        default:
            fprintf(stderr, "unrecognised switch code 0x%x ??\n", c);
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
    if ((cp = strchr(device_name, ','))) {
        *cp = '\0';
        if (1 != sscanf(cp + 1, "%d", &subvalue)) {
            fprintf(stderr, "expected number after comma in SMP_DEVICE "
                    "name\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if (0 == sa) {
        cp = getenv("SMP_UTILS_SAS_ADDR");
        if (cp) {
           sa_ll = smp_get_llnum(cp);
           if (-1LL == sa_ll) {
                fprintf(stderr, "bad value in environment variable "
                        "SMP_UTILS_SAS_ADDR\n");
                fprintf(stderr, "    use 0\n");
                sa_ll = 0;
            }
            sa = (unsigned long long)sa_ll;
        }
    }
    if (sa > 0) {
        if (! smp_is_naa5(sa)) {
            fprintf(stderr, "SAS (target) address not in naa-5 format "
                    "(may need leading '0x')\n");
            if ('\0' == i_params[0]) {
                fprintf(stderr, "    use '--interface=' to override\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
        }
    }

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    smp_req[4] = (expected_cc >> 8) & 0xff;
    smp_req[5] = expected_cc & 0xff;
    smp_req[9] = phy_id;
    smp_req[10] = do_function;
    smp_req[11] = pattern;
    smp_req[15] = linkrate & 0xf;
    smp_req[15] |= (do_ssc << 4) & 0x30;
    smp_req[15] |= (do_sata << 6) & 0x40;
    smp_req[19] = do_control;
    for (k = 0; k < 8; ++k, dwords >>= 8)
        smp_req[20 + 7 - k] = dwords & 0xff;
    if (verbose) {
        fprintf(stderr, "    Phy test function request: ");
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
    smp_rr.max_response_len = sizeof(smp_resp);
    smp_rr.response = smp_resp;
    res = smp_send_req(&tobj, &smp_rr, verbose);

    if (res) {
        fprintf(stderr, "smp_send_req failed, res=%d\n", res);
        if (0 == verbose)
            fprintf(stderr, "    try adding '-v' option for more debug\n");
        ret = -1;
        goto err_out;
    }
    if (smp_rr.transport_err) {
        fprintf(stderr, "smp_send_req transport_error=%d\n",
                smp_rr.transport_err);
        ret = -1;
        goto err_out;
    }
    if ((smp_rr.act_response_len >= 0) && (smp_rr.act_response_len < 4)) {
        fprintf(stderr, "response too short, len=%d\n", smp_rr.act_response_len);
        ret = SMP_LIB_CAT_MALFORMED;
        goto err_out;
    }
    len = smp_resp[3];
    if ((0 == len) && (0 == smp_resp[2])) {
        len = smp_get_func_def_resp_len(smp_resp[1]);
        if (len < 0) {
            len = 0;
            if (verbose > 0)
                fprintf(stderr, "unable to determine response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if (do_hex || do_raw) {
        if (do_hex)
            dStrHex((const char *)smp_resp, len, 1);
        else
            dStrRaw((const char *)smp_resp, len);
        if (SMP_FRAME_TYPE_RESP != smp_resp[0])
            ret = SMP_LIB_CAT_MALFORMED;
        else if (smp_resp[1] != smp_req[1])
            ret = SMP_LIB_CAT_MALFORMED;
        else if (smp_resp[2]) {
            if (verbose)
                fprintf(stderr, "Phy test function result: %s\n",
                        smp_get_func_res_str(smp_resp[2], sizeof(b), b));
            ret = smp_resp[2];
        }
        goto err_out;
    }
    if (SMP_FRAME_TYPE_RESP != smp_resp[0]) {
        fprintf(stderr, "expected SMP frame response type, got=0x%x\n", smp_resp[0]);
        ret = SMP_LIB_CAT_MALFORMED;
        goto err_out;
    }
    if (smp_resp[1] != smp_req[1]) {
        fprintf(stderr, "Expected function code=0x%x, got=0x%x\n", smp_req[1], smp_resp[1]);
        ret = SMP_LIB_CAT_MALFORMED;
        goto err_out;
    }
    if (smp_resp[2]) {
        cp = smp_get_func_res_str(smp_resp[2], sizeof(b), b);
        fprintf(stderr, "Phy test function result: %s\n", cp);
        ret = smp_resp[2];
        goto err_out;
    }

err_out:
    res = smp_initiator_close(&tobj);
    if (res < 0) {
        fprintf(stderr, "close error: %s\n", safe_strerror(errno));
        if (0 == ret)
            return SMP_LIB_FILE_ERROR;
    }
    if (ret < 0)
        ret = SMP_LIB_CAT_OTHER;
    if (verbose && ret)
        fprintf(stderr, "Exit status %d indicates error detected\n", ret);
    return ret;
}
