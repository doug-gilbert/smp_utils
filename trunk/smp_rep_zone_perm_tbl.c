/*
 * Copyright (c) 2011 Douglas Gilbert.
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
 * This utility issues a REPORT ZONE PERMISSION TABLE function and outputs
 * its response.
 */

static char * version_str = "1.03 20110619";

#define SMP_FN_REPORT_ZONE_PERMISSION_TBL_RESP_LEN (1020 + 4 + 4)
#define DEF_MAX_NUM_DESC 63


static char * decode_rtype[] = {
        "current",
        "shadow",
        "saved",
        "default",
};

static char * decode_numzg[] = {
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
        {"append", 0, 0, 'a'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"interface", 1, 0, 'I'},
        {"multiple", 0, 0, 'm'},
        {"num", 1, 0, 'n'},
        {"nocomma", 0, 0, 'N'},
        {"permf", 1, 0, 'P'},
        {"raw", 0, 0, 'r'},
        {"report", 1, 0, 'R'},
        {"sa", 1, 0, 's'},
        {"start", 1, 0, 'f'},
        {"t10", 1, 0, 'T'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};


static void usage()
{
    fprintf(stderr, "Usage: "
          "smp_rep_zone_perm_tbl [--append] [--help] [--hex] "
          "[--interface=PARAMS]\n"
          "                             [--multiple] [--nocomma] "
          "[--num=MD] [--permf=FN]\n"
          "                             [--raw] [--report=RT] "
          "[--sa=SAS_ADDR]\n"
          "                             [--start=SS] [--t10=BITS] "
          "[--verbose]\n"
          "                             [--version] SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --append|-a          append to FN with '--permf' option\n"
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
          "                                 '0x' or trailing 'h'). Depending "
          "on\n"
          "                                 the interface, may not be "
          "needed\n"
          "    --start=SS|-f SS     starting (first) source zone group "
          "(default: 0)\n"
          "    --t10=BITS|-T BITS    square array with BITS rows. ZP[0,0] "
          "top left\n"
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n\n"
          "Perform one or more SMP REPORT ZONE PERMISSION TABLE functions\n"
          );
}

static void dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}


int main(int argc, char * argv[])
{
    int res, c, k, j, m, len, desc_len, num_desc, numzg, max_sszg;
    int desc_per_resp, first, rtype;
    int do_append = 0;
    int do_hex = 0;
    int multiple = 0;
    int nocomma = 0;
    int mndesc = DEF_MAX_NUM_DESC;
    int mndesc_given = 0;
    const char * permf = NULL;
    int do_raw = 0;
    int report_type = 0;
    int sszg = 0;
    int t10_rows = 0;
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[12];
    unsigned char smp_resp[SMP_FN_REPORT_ZONE_PERMISSION_TBL_RESP_LEN];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    unsigned char * descp;
    FILE * foutp = stdout;
    int ret = 0;

    memset(device_name, 0, sizeof device_name);
    memset(smp_resp, 0, sizeof smp_resp);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "af:hHI:mn:NP:rR:s:T:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
            ++do_append;
            break;
        case 'f':       /* note: maps to '--start=SS' option */
           sszg = smp_get_num(optarg);
           if ((sszg < 0) || (sszg > 255)) {
                fprintf(stderr, "bad argument to '--start=', expect 0 to "
                        "255\n");
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
                fprintf(stderr, "bad argument to '--num=', expect 0 to "
                        "63\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            if (0 == mndesc)
                mndesc = DEF_MAX_NUM_DESC;
            else
                ++mndesc_given;
            break;
        case 'm':
            ++multiple;
            break;
        case 'N':
            ++nocomma;
            break;
        case 'P':
           permf = optarg;
            break;
        case 'r':
            ++do_raw;
            break;
        case 'R':
           report_type = smp_get_num(optarg);
           if ((report_type < 0) || (report_type > 3)) {
                fprintf(stderr, "bad argument to '--report=', expect 0 to "
                        "3\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 's':
           sa_ll = smp_get_llnum(optarg);
           if (-1LL == sa_ll) {
                fprintf(stderr, "bad argument to '--sa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            sa = (unsigned long long)sa_ll;
            break;
        case 'T':
           t10_rows = smp_get_num(optarg);
           if ((t10_rows < 1) || (t10_rows > 256)) {
                fprintf(stderr, "bad argument to '--t10=', expect 1 to "
                        "256\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
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
    if (multiple && mndesc_given) {
        fprintf(stderr, "--multiple and --num clash, give one or the "
                "other\n");
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
                fprintf(stderr, "unable to open %s, error: %s\n", permf,
                        safe_strerror(errno));
                ret = SMP_LIB_FILE_ERROR;
                goto err_out;
            }
        }
    }
    max_sszg = 256;
    desc_per_resp = 63;

    for (j = sszg, first = 1; j < max_sszg; j +=  desc_per_resp) {
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
            fprintf(stderr, "    Report zone permission table request: ");
            for (k = 0; k < (int)sizeof(smp_req); ++k)
                fprintf(stderr, "%02x ", smp_req[k]);
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
            fprintf(stderr, "response too short, len=%d\n",
                    smp_rr.act_response_len);
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
            if (smp_resp[1] != smp_req[1])
                ret = SMP_LIB_CAT_MALFORMED;
            if (smp_resp[2]) {
                ret = smp_resp[2];
                if (verbose)
                    fprintf(stderr, "Report zone permission table result: %s\n",
                            smp_get_func_res_str(ret, sizeof(b), b));
            }
            goto err_out;
        }
        if (SMP_FRAME_TYPE_RESP != smp_resp[0]) {
            fprintf(stderr, "expected SMP frame response type, got=0x%x\n",
                    smp_resp[0]);
            ret = SMP_LIB_CAT_MALFORMED;
            goto err_out;
        }
        if (smp_resp[1] != smp_req[1]) {
            fprintf(stderr, "Expected function code=0x%x, got=0x%x\n",
                    smp_req[1], smp_resp[1]);
            ret = SMP_LIB_CAT_MALFORMED;
            goto err_out;
        }
        if (smp_resp[2]) {
            cp = smp_get_func_res_str(smp_resp[2], sizeof(b), b);
            fprintf(stderr, "Report zone permission table result: %s\n", cp);
            ret = smp_resp[2];
            goto err_out;
        }
        numzg = (0xc0 & smp_resp[7]) >> 6;
        desc_len = smp_resp[13] * 4;
        num_desc = smp_resp[15];
        rtype = 0x3 & smp_resp[6];
        if (first) {
            first = 0;
            if (0 == numzg) {
                max_sszg = 128;
                desc_per_resp = 63;
            } else {
                max_sszg = 256;
                desc_per_resp = 31;
            }
            fprintf(foutp, "# Report zone permission table response:\n");
            res = (smp_resp[4] << 8) + smp_resp[5];
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
            if (t10_rows) {
                fprintf(foutp, "\n\nOutput unsuitable for "
                        "smp_conf_zone_perm_tbl utility\n\n    ");
                for (k = 0; k < t10_rows; ++k)
                    fprintf(foutp, "%d", k % 10);
                fprintf(foutp, "\n");
            }
            if (0 == numzg_blen[numzg]) {
                fprintf(stderr, "unexpected number of zone groups: %d\n",
                        numzg);
                goto err_out;
            }
        }
        descp = smp_resp + 16;
        for (k = 0; k < num_desc; ++k, descp += desc_len) {
            if (0 == t10_rows) {
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
            } else {
                int by, bi;

                if ((k + j) >= t10_rows)
                    break;
                fprintf(foutp, "%-4d", j + k);
                for (m = 0; m < t10_rows; ++m) {
                    by = (m / 8) + 1;
                    bi = m % 8;
                    fprintf(foutp, "%d", (descp[desc_len - by] >> bi) & 0x1);
                }
            }
            fprintf(foutp, "\n");
        }
        if ((0 == multiple) || (mndesc < desc_per_resp))
            break;
    }

err_out:
    if (foutp && (stdout != foutp)) {
        fclose(foutp);
        foutp = NULL;
    }
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
