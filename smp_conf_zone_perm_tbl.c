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
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "smp_lib.h"

/* This is a Serial Attached SCSI (SAS) management protocol (SMP) utility
 * program.
 *
 * This utility issues a CONFIGURE ZONE PERMISSION TABLE function and outputs
 * its response.
 */

static char * version_str = "1.03 20110730";

/* Permission table big enough for 256 source zone groups (rows) and
 * 256 destination zone groups (columns). Each element is a single bit,
 * written in the drafts as ZP[s,d] . */
static unsigned char full_perm_tbl[32 * 256];

static int sszg = 0;
static int sszg_given = 0;


static struct option long_options[] = {
        {"deduce", 0, 0, 'd'},
        {"expected", 1, 0, 'E'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"interface", 1, 0, 'I'},
        {"numzg", 1, 0, 'n'},
        {"permf", 1, 0, 'P'},
        {"raw", 0, 0, 'r'},
        {"sa", 1, 0, 's'},
        {"save", 1, 0, 'S'},
        {"start", 1, 0, 'f'},   /* note the short option: 'f' */
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

static void usage()
{
    fprintf(stderr, "Usage: "
          "smp_conf_zone_perm_tbl [--deduce] [--expected=EX] [--help] "
          "[--hex]\n"
          "                              [--interface=PARAMS] [--numzg=NG] "
          "--permf=FN\n"
          "                              [--raw] [--sa=SAS_ADDR] "
          "[--save=SAV]\n"
          "                              [--start=SS] [--verbose] "
          "[--version]\n"
          "                              SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --deduce|-d            deduce number of zone groups from "
          "number\n"
          "                           of bytes on active FN lines\n"
          "    --expected=EX|-E EX    set expected expander change "
          "count to EX\n"
          "    --help|-h              print out usage message\n"
          "    --hex|-H               print response in hexadecimal\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --numzg=NG|-n NG       number of zone groups. NG should be "
          "0 (def)\n"
          "                           or 1. 0 -> 128 zone groups, 1 -> 256\n"
          "    --permf=FN|-P FN       FN is a file containing zone "
          "permission\n"
          "                           configuration descriptors in hex; "
          "required\n"
          "    --raw|-r               output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading\n"
          "                                 '0x' or trailing 'h'). Depending "
          "on\n"
          "                                 the interface, may not be "
          "needed\n"
          "    --save=SAV|-S SAV      SAV: 0 -> shadow (def); 1 -> "
          "saved\n"
          "                           2 -> shadow (and saved if "
          "supported))\n"
          "                           3 -> shadow and saved\n"
          "    --start=SS|-f SS       starting (first) source zone group "
          "(def: 0)\n"
          "    --verbose|-v           increase verbosity\n"
          "    --version|-V           print version string and exit\n\n"
          "Performs one of more SMP CONFIGURE ZONE PERMISSION TABLE "
          "functions\n"
          );
}

/* Read ASCII hex bytes from fname (a file named '-' taken as stdin).
 * There should be either one entry per line or a comma, space or tab
 * separated list of bytes. The first ASCII hex string detected has its
 * length checked; if the length is 1 or 2 then bytes are expected to
 * be space, comma or tab separated; if the length is 3 or more then a
 * string of ACSII hex digits is expected, 2 per byte. If any lines
 * contain more that 16 bytes (numzg256p is non-NULL), then 1 is written
 * to *numzg256p. Everything from and including a '#' and '-' on a line
 * is ignored. '#' is meant for comments. '-' is meant as a lead-in to an
 * option (e.g. "--start=8"); not yet implemented.
 * Returns 0 if ok, or 1 if error. */
static int
f2hex_arr(const char * fname, unsigned char * mp_arr, int * mp_arr_len,
          int max_arr_len, int * numzg256p, int verbose)
{
    int fn_len, in_len, k, j, m;
    int no_space = 0;
    int checked_hexlen = 0;
    int numzg256 = 0;
    unsigned int h;
    const char * lcp;
    FILE * fp;
    char line[512];
    int off = 0;

    if ((NULL == fname) || (NULL == mp_arr) || (NULL == mp_arr_len))
        return 1;
    fn_len = strlen(fname);
    if (0 == fn_len)
        return 1;
    if ((1 == fn_len) && ('-' == fname[0]))        /* read from stdin */
        fp = stdin;
    else {
        fp = fopen(fname, "r");
        if (NULL == fp) {
            fprintf(stderr, "Unable to open %s for reading\n", fname);
            return 1;
        }
    }

    for (j = 0; j < 512; ++j) {
        if (NULL == fgets(line, sizeof(line), fp))
            break;
        in_len = strlen(line);
        if (in_len > 0) {
            if ('\n' == line[in_len - 1]) {
                --in_len;
                line[in_len] = '\0';
            }
        }
        if (0 == in_len)
            continue;
        lcp = line;
        m = strspn(lcp, " \t");
        if (m == in_len)
            continue;
        lcp += m;
        in_len -= m;
        if ('#' == *lcp)
            continue;
        if ('-' == *lcp) {
            if (0 == strncmp("--start=", lcp, 8)) {
                if (1 == sscanf(lcp, "--start=%d", &k)) {
                    if (sszg_given && (k != sszg)) {
                        fprintf(stderr, "permission file '--start=%d' "
                                "contradicts command line '--start=%d'\n",
                                k, sszg);
                        goto bad;
                    }
                    if (verbose)
                        fprintf(stderr, "permission file contains "
                                "--start=%d, using it\n", k);
                    sszg = k;
                }  else if (verbose)
                    fprintf(stderr, "found line with '-' but "
                            "could not decode --start=<num>\n");
            }
            continue;
        }
        if (! checked_hexlen) {
            ++checked_hexlen;
            k = strspn(lcp, "0123456789aAbBcCdDeEfF");
            if (k > 2)
                no_space = 1;
        }

        k = strspn(lcp, "0123456789aAbBcCdDeEfF ,\t");
        if ((k < in_len) && ('#' != lcp[k]) && ('-' != lcp[k])) {
            fprintf(stderr, "f2hex_arr: syntax error at "
                    "line %d, pos %d\n", j + 1, m + k + 1);
            goto bad;
        }
        if (no_space) {
            for (k = 0; isxdigit(*lcp) && isxdigit(*(lcp + 1));
                 ++k, lcp += 2) {
                if (1 != sscanf(lcp, "%2x", &h)) {
                    fprintf(stderr, "f2hex_arr: bad hex number in line "
                            "%d, pos %d\n", j + 1, (int)(lcp - line + 1));
                    goto bad;
                }
                if ((off + k) >= max_arr_len) {
                    fprintf(stderr, "f2hex_arr: array length exceeded\n");
                    goto bad;
                }
                mp_arr[off + k] = h;
            }
            if (k > 16)
                ++numzg256;
            off += k;
        } else {
            for (k = 0; k < 1024; ++k) {
                if (1 == sscanf(lcp, "%x", &h)) {
                    if (h > 0xff) {
                        fprintf(stderr, "f2hex_arr: hex number "
                                "larger than 0xff in line %d, pos %d\n",
                                j + 1, (int)(lcp - line + 1));
                        goto bad;
                    }
                    if ((off + k) >= max_arr_len) {
                        fprintf(stderr, "f2hex_arr: array length "
                                "exceeded\n");
                        goto bad;
                    }
                    mp_arr[off + k] = h;
                    lcp = strpbrk(lcp, " ,\t");
                    if (NULL == lcp)
                        break;
                    lcp += strspn(lcp, " ,\t");
                    if ('\0' == *lcp)
                        break;
                } else {
                    if (('#' == *lcp) || ('-' == *lcp)) {
                        --k;
                        break;
                    }
                    fprintf(stderr, "f2hex_arr: error in "
                            "line %d, at pos %d\n", j + 1,
                            (int)(lcp - line + 1));
                    goto bad;
                }
            }
            if (k > 15)
                ++numzg256;
            off += (k + 1);
        }
    }
    *mp_arr_len = off;
    fclose(fp);
    if (numzg256p)
        *numzg256p = !! numzg256;
    return 0;
bad:
    fclose(fp);
    return 1;
}

static void dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

int main(int argc, char * argv[])
{
    int res, c, k, j, len, num_desc, numd, desc_len, numzg256;
    int max_desc_per_req, act_resplen;
    const char * permf = NULL;
    int deduce = 0;
    int expected_cc = 0;
    int do_hex = 0;
    int num_zg = 128;
    int num_zg_given = 0;
    int do_raw = 0;
    int do_save = 0;
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[1028];
    unsigned char smp_resp[8];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 0;

    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "dE:f:hHI:n:P:rs:S:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'd':
            ++deduce;
            break;
        case 'E':
            expected_cc = smp_get_num(optarg);
            if ((expected_cc < 0) || (expected_cc > 65535)) {
                fprintf(stderr, "bad argument to '--expected'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'f':       /* maps tp '--start=SS' */
            sszg = smp_get_num(optarg);
            if ((sszg < 0) || (sszg > 255)) {
                fprintf(stderr, "bad argument to '--start'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            ++sszg_given;
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
            k = smp_get_num(optarg);
            switch (k) {
            case 0:
                num_zg = 128;
                break;
            case 1:
                num_zg = 256;
                break;
            default:
                fprintf(stderr, "bad argument to '--numzg'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            ++num_zg_given;
            break;
        case 'P':
            permf = optarg;
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
            do_save = smp_get_num(optarg);
            if ((do_save < 0) || (do_save > 3)) {
                fprintf(stderr, "bad argument to '--save'\n");
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
            fprintf(stderr, "expected number after comma in SMP_DEVICE name\n");
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
    if (NULL == permf) {
        fprintf(stderr, "--permf=FN option is required (i.e. it's not "
                "optional)\n");
        return SMP_LIB_SYNTAX_ERROR;
    }
    if (deduce && num_zg_given) {
        fprintf(stderr, "can't give both --deduce and --numzg=\n");
        return SMP_LIB_SYNTAX_ERROR;
    }
    if (f2hex_arr(permf, full_perm_tbl, &len, sizeof full_perm_tbl,
                  &numzg256, verbose)) {
        fprintf(stderr, "failed decoding --permf=FN option\n");
        return SMP_LIB_SYNTAX_ERROR;
    }
    if (deduce && numzg256)
        num_zg = 256;
    desc_len = (128 == num_zg) ? 16 : 32;
    num_desc = len / desc_len;
    if (0 != (len % desc_len))
        fprintf(stderr, "warning: permf data not a multiple of %d bytes, "
                "ignore excess\n", desc_len);
    max_desc_per_req = (128 == num_zg) ? 63 : 31;

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    for (j = 0; j < num_desc; j += max_desc_per_req) {
        numd = num_desc - j;
        if (numd > max_desc_per_req)
            numd = max_desc_per_req;
        memset(smp_req, 0, sizeof(smp_req));
        smp_req[0] = SMP_FRAME_TYPE_REQ;
        smp_req[1] = SMP_FN_CONFIG_ZONE_PERMISSION_TBL;
        smp_req[3] = (numd * (desc_len / 4)) + 3;
        smp_req[4] = (expected_cc >> 8) & 0xff;
        smp_req[5] = expected_cc & 0xff;
        smp_req[6] = sszg + j;
        smp_req[7] = numd;
        smp_req[8] = (do_save & 0x3);
        if (256 == num_zg)
            smp_req[8] |= 0x40;
        smp_req[9] = (256 == num_zg) ? 8 : 4;
        memcpy(smp_req + 16, full_perm_tbl + (j * desc_len), numd * desc_len);
        if (verbose) {
            fprintf(stderr, "    Configure zone permission table request:");
            for (k = 0; k < (20 + (numd * desc_len)); ++k) {
                if (0 == (k % 16))
                    fprintf(stderr, "\n      ");
                else if (0 == (k % 8))
                    fprintf(stderr, " ");
                fprintf(stderr, "%02x ", smp_req[k]);
            }
            fprintf(stderr, "\n");
        }
        memset(&smp_rr, 0, sizeof(smp_rr));
        smp_rr.request_len = 20 + (numd * desc_len);
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
        act_resplen = smp_rr.act_response_len;
        if ((act_resplen >= 0) && (act_resplen < 4)) {
            fprintf(stderr, "response too short, len=%d\n", act_resplen);
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
        len = 4 + (len * 4);    /* length in bytes, excluding 4 byte CRC */
        if ((act_resplen >= 0) && (len > act_resplen)) {
            if (verbose)
                fprintf(stderr, "actual response length [%d] less than "
                        "deduced length [%d]\n", act_resplen, len);
            len = act_resplen; 
        }
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
                    fprintf(stderr, "Configure zone permission table result: "
                            "%s\n", smp_get_func_res_str(smp_resp[2],
                                                         sizeof(b), b));
                ret = smp_resp[2];
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
            fprintf(stderr, "Configure zone permission table result: %s\n",
                    cp);
            ret = smp_resp[2];
            goto err_out;
        }
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