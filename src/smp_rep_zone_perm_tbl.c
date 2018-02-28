/*
 * Copyright (c) 2011-2018, Douglas Gilbert
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"
#include "sg_unaligned.h"

/* This is a Serial Attached SCSI (SAS) Serial Management Protocol (SMP)
 * utility.
 *
 * This utility issues a REPORT ZONE PERMISSION TABLE function and outputs
 * its response.
 */

static const char * version_str = "1.10 20180212";

#define SMP_FN_REPORT_ZONE_PERMISSION_TBL_RESP_LEN (1020 + 4 + 4)
#define DEF_MAX_NUM_DESC 63

static const char * decode_rtype[] = {
        "current",
        "shadow",
        "saved",
        "default",
};

static const char * decode_numzg[] = {
        "128",
        "256",
        "?",
        "? ?",
};

static int numzg_blen[] = {
        16,
        32,
        0,
        0,
};

static struct option long_options[] = {
        {"append", no_argument, 0, 'a'},
        {"bits", required_argument, 0, 'B'},
        {"help", no_argument, 0, 'h'},
        {"hex", no_argument, 0, 'H'},
        {"interface", required_argument, 0, 'I'},
        {"multiple", no_argument, 0, 'm'},
        {"num", required_argument, 0, 'n'},
        {"nocomma", no_argument, 0, 'N'},
        {"permf", required_argument, 0, 'P'},
        {"raw", no_argument, 0, 'r'},
        {"report", required_argument, 0, 'R'},
        {"sa", required_argument, 0, 's'},
        {"start", required_argument, 0, 'f'},
        {"verbose", no_argument, 0, 'v'},
        {"version", no_argument, 0, 'V'},
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
    pr2serr("Usage: smp_rep_zone_perm_tbl [--append] [--bits=COL] [--help] "
            "[--hex]\n"
            "                             [--interface=PARAMS] [--multiple] "
            "[--nocomma]\n"
            "                             [--num=MD] [--permf=FN] [--raw] "
            "[--report=RT]\n"
            "                             [--sa=SAS_ADDR] [--start=SS] "
            "[--verbose]\n"
            "                             [--version] SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --append|-a          append to FN with '--permf' option\n"
            "    --bits=COL|-B COL    output table as bit array with COL "
            "columns\n"
            "                         and ZP[0,0] top left (def: output byte "
            "array)\n"
            "    --help|-h            print out usage message\n"
            "    --hex|-H             print response in hexadecimal\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --multiple|-m        issue multiple function requests until "
            "all\n"
            "                         available descriptors (from SS) are "
            "read\n"
            "    --nocomma|-N         output descriptors as long string of "
            "hex\n"
            "                         (default: bytes comma separated)\n"
            "    --num=MD|-n MD       maximum number of descriptors in one "
            "response\n"
            "                         (default: 63)\n"
            "    --permf=FN|-P FN     write descriptors to file FN (default: "
            "write\n"
            "                         to stdout)\n"
            "    --raw|-r             output response in binary\n"
            "    --report=RT|-R RT    report type (default: 0). 0 -> current;"
            "\n"
            "                         1 -> shadow; 2 -> saved; 3 -> default\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending on\n"
            "                                 the interface, may not be "
            "needed\n"
            "    --start=SS|-f SS     starting (first) source zone group "
            "(default: 0)\n"
            "    --verbose|-v         increase verbosity\n"
            "    --version|-V         print version string and exit\n\n"
            "Perform one or more SMP REPORT ZONE PERMISSION TABLE functions\n"
           );
}

static void
dStrRaw(const uint8_t * str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}


int
main(int argc, char * argv[])
{
    bool do_append = false;
    bool do_raw = false;
    bool first;
    bool mndesc_given = false;
    bool multiple = false;
    bool nocomma = false;
    int res, c, k, j, m, len, desc_len, num_desc, numzg, max_sszg;
    int desc_per_resp, rtype, act_resplen;
    int bits_col = 0;
    int do_hex = 0;
    int mndesc = DEF_MAX_NUM_DESC;
    int report_type = 0;
    int ret = 0;
    int sszg = 0;
    int subvalue = 0;
    int verbose = 0;
    int64_t sa_ll;
    uint64_t sa = 0;
    char * cp;
    uint8_t * descp;
    FILE * foutp = stdout;
    const char * permf = NULL;
    char i_params[256];
    char device_name[512];
    char b[256];
    uint8_t smp_req[12];
    uint8_t smp_resp[SMP_FN_REPORT_ZONE_PERMISSION_TBL_RESP_LEN];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;

    memset(device_name, 0, sizeof device_name);
    memset(smp_resp, 0, sizeof smp_resp);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "aB:f:hHI:mn:NP:rR:s:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
            do_append = true;
            break;
        case 'B':
           bits_col = smp_get_num(optarg);
           if ((bits_col < 1) || (bits_col > 256)) {
                pr2serr("bad argument to '--bits=', expect 1 to 256\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'f':       /* note: maps to '--start=SS' option */
           sszg = smp_get_num(optarg);
           if ((sszg < 0) || (sszg > 255)) {
                pr2serr("bad argument to '--start=', expect 0 to 255\n");
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
        case 'n':
           mndesc = smp_get_num(optarg);
           if ((mndesc < 0) || (mndesc > 63)) {
                pr2serr("bad argument to '--num=', expect 0 to 63\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            if (0 == mndesc)
                mndesc = DEF_MAX_NUM_DESC;
            else
                mndesc_given = true;
            break;
        case 'm':
            multiple = true;
            break;
        case 'N':
            nocomma = true;
            break;
        case 'P':
           permf = optarg;
            break;
        case 'r':
            do_raw = true;
            break;
        case 'R':
           report_type = smp_get_num(optarg);
           if ((report_type < 0) || (report_type > 3)) {
                pr2serr("bad argument to '--report=', expect 0 to 3\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
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
    if (multiple && mndesc_given) {
        pr2serr("--multiple and --num clash, give one or the other\n");
        return SMP_LIB_SYNTAX_ERROR;
    }

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;
    if (permf) {
        if ((1 == strlen(permf)) && (0 == strcmp("-", permf)))
            ;
        else {
            foutp = fopen(permf, (do_append ? "a" : "w"));
            if (NULL == foutp) {
                pr2serr("unable to open %s, error: %s\n", permf,
                        safe_strerror(errno));
                ret = SMP_LIB_FILE_ERROR;
                goto err_out;
            }
        }
    }
    max_sszg = 256;
    desc_per_resp = 63;

    for (j = sszg, first = true; j < max_sszg; j +=  desc_per_resp) {
        memset(smp_req, 0, sizeof smp_req);
        smp_req[0] = SMP_FRAME_TYPE_REQ;
        smp_req[1] = SMP_FN_REPORT_ZONE_PERMISSION_TBL;
        len = (sizeof(smp_resp) - 8) / 4;
        smp_req[2] = (len < 0x100) ? len : 0xff; /* Allocated Response Len */
        smp_req[3] = 0x1;
        smp_req[4] = report_type & 0x3;
        smp_req[6] = j & 0xff;
        numzg = max_sszg - j;
        if (desc_per_resp < numzg)
            numzg = desc_per_resp;
        if (mndesc < numzg)
            numzg = mndesc;
        smp_req[7] = numzg & 0xff;
        if (verbose) {
            pr2serr("    Report zone permission table request: ");
            for (k = 0; k < (int)sizeof(smp_req); ++k)
                pr2serr("%02x ", smp_req[k]);
            pr2serr("\n");
        }
        memset(&smp_rr, 0, sizeof(smp_rr));
        smp_rr.request_len = sizeof(smp_req);
        smp_rr.request = smp_req;
        smp_rr.max_response_len = sizeof(smp_resp);
        smp_rr.response = smp_resp;
        res = smp_send_req(&tobj, &smp_rr, verbose);

        if (res) {
            pr2serr("smp_send_req failed, res=%d\n", res);
            if (0 == verbose)
                pr2serr("    try adding '-v' option for more debug\n");
            ret = -1;
            goto err_out;
        }
        if (smp_rr.transport_err) {
            pr2serr("smp_send_req transport_error=%d\n", smp_rr.transport_err);
            ret = -1;
            goto err_out;
        }
        act_resplen = smp_rr.act_response_len;
        if ((act_resplen >= 0) && (act_resplen < 4)) {
            pr2serr("response too short, len=%d\n", act_resplen);
            ret = SMP_LIB_CAT_MALFORMED;
            goto err_out;
        }
        len = smp_resp[3];
        if ((0 == len) && (0 == smp_resp[2])) {
            len = smp_get_func_def_resp_len(smp_resp[1]);
            if (len < 0) {
                len = 0;
                if (verbose > 0)
                    pr2serr("unable to determine response length\n");
            }
        }
        len = 4 + (len * 4);    /* length in bytes, excluding 4 byte CRC */
        if ((act_resplen >= 0) && (len > act_resplen)) {
            if (verbose)
                pr2serr("actual response length [%d] less than deduced "
                        "length [%d]\n", act_resplen, len);
            len = act_resplen;
        }
        if (do_hex || do_raw) {
            if (do_hex)
                hex2stdout(smp_resp, len, 1);
            else
                dStrRaw(smp_resp, len);
            if (SMP_FRAME_TYPE_RESP != smp_resp[0])
                ret = SMP_LIB_CAT_MALFORMED;
            if (smp_resp[1] != smp_req[1])
                ret = SMP_LIB_CAT_MALFORMED;
            if (smp_resp[2]) {
                ret = smp_resp[2];
                if (verbose)
                    pr2serr("Report zone permission table result: %s\n",
                            smp_get_func_res_str(ret, sizeof(b), b));
            }
            goto err_out;
        }
        if (SMP_FRAME_TYPE_RESP != smp_resp[0]) {
            pr2serr("expected SMP frame response type, got=0x%x\n",
                    smp_resp[0]);
            ret = SMP_LIB_CAT_MALFORMED;
            goto err_out;
        }
        if (smp_resp[1] != smp_req[1]) {
            pr2serr("Expected function code=0x%x, got=0x%x\n", smp_req[1],
                    smp_resp[1]);
            ret = SMP_LIB_CAT_MALFORMED;
            goto err_out;
        }
        if (smp_resp[2]) {
            cp = smp_get_func_res_str(smp_resp[2], sizeof(b), b);
            pr2serr("Report zone permission table result: %s\n", cp);
            ret = smp_resp[2];
            goto err_out;
        }
        numzg = (0xc0 & smp_resp[7]) >> 6;
        desc_len = smp_resp[13] * 4;
        num_desc = smp_resp[15];
        rtype = 0x3 & smp_resp[6];
        if (first) {
            first = false;
            if (0 == numzg) {
                max_sszg = 128;
                desc_per_resp = 63;
            } else {
                max_sszg = 256;
                desc_per_resp = 31;
            }
            fprintf(foutp, "# Report zone permission table response:\n");
            res = sg_get_unaligned_be16(smp_resp + 4);
            if (verbose || res)
                fprintf(foutp, "#  Expander change count: %d\n", res);
            fprintf(foutp, "#  zone locked: %d\n", !! (0x80 & smp_resp[6]));
            fprintf(foutp, "#  report type: %d [%s]\n", rtype,
                    decode_rtype[rtype]);
            fprintf(foutp, "#  number of zone groups: %d (%s)\n", numzg,
                    decode_numzg[numzg]);
            if (verbose) {
                fprintf(foutp, "#  zone permission descriptor length: %d "
                        "dwords\n", smp_resp[13]);
                fprintf(foutp, "#  starting source zone group%s: %d\n",
                        (multiple ? " (of first request)" : ""),
                        smp_resp[14]);
                fprintf(foutp, "#  number of zone permission descriptors%s: "
                        "%d\n", (multiple ? " (of first request)" : ""),
                        num_desc);
            } else if (! multiple)
                fprintf(foutp, "#  number of zone permission descriptors: "
                        "%d\n", num_desc);
            if (sszg > 0)
                fprintf(foutp, "--start=%d\n", sszg);
            if (bits_col) {
                fprintf(foutp, "\n\nOutput unsuitable for "
                        "smp_conf_zone_perm_tbl utility\n\n    ");
                for (k = 0; k < bits_col; ++k)
                    fprintf(foutp, "%d", k % 10);
                fprintf(foutp, "\n\n");
            }
            if (0 == numzg_blen[numzg]) {
                pr2serr("unexpected number of zone groups: %d\n", numzg);
                goto err_out;
            }
        }
        descp = smp_resp + 16;
        for (k = 0; k < num_desc; ++k, descp += desc_len) {
            if (0 == bits_col) {
                for (m = 0; m < desc_len; ++m) {
                    if (nocomma)
                        fprintf(foutp, "%02x", descp[m]);
                    else {
                        if (0 == m)
                            fprintf(foutp, "%x", descp[m]);
                        else
                            fprintf(foutp, ",%x", descp[m]);
                    }
                }
            } else {    /* --bit=<bits_col> given */
                int by, bi;

                if ((k + j) >= bits_col)
                    break;
                fprintf(foutp, "%-4d", j + k);
                for (m = 0; m < bits_col; ++m) {
                    by = (m / 8) + 1;
                    bi = m % 8;
                    fprintf(foutp, "%d", (descp[desc_len - by] >> bi) & 0x1);
                }
            }
            fprintf(foutp, "\n");
        }
        if ((! multiple) || (mndesc < desc_per_resp))
            break;
    }

err_out:
    if (foutp && (stdout != foutp)) {
        fclose(foutp);
        foutp = NULL;
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


