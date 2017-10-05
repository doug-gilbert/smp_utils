/*
 * Copyright (c) 2011-2017 Douglas Gilbert.
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
#include <ctype.h>
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
 * This utility issues a ZONED BROADCAST function and outputs its response.
 */

static const char * version_str = "1.05 20171004";

static struct option long_options[] = {
    {"broadcast", 1, 0, 'b'},
    {"expected", 1, 0, 'E'},
    {"fszg", 1, 0, 'F'},
    {"help", 0, 0, 'h'},
    {"hex", 0, 0, 'H'},
    {"interface", 1, 0, 'I'},
    {"raw", 0, 0, 'r'},
    {"sa", 1, 0, 's'},
    {"szg", 1, 0, 'S'},
    {"verbose", 0, 0, 'v'},
    {"version", 0, 0, 'V'},
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
    pr2serr("Usage: smp_zoned_broadcast [--broadcast=BT] [--expected=EX] "
            "[--fszg=FS]\n"
            "                           [--help] [--hex] [--interface=PARAMS] "
            "[--raw]\n"
            "                           [--sa=SAS_ADDR] [--szg=ZGL] "
            "[--verbose]\n"
            "                           [--version] SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --broadcast=BT|-b BT    BT is type of broadcast (def: 0 "
            "which is\n"
            "                            Broadcast(Change))\n"
            "    --expected=EX|-E EX     set expected expander change "
            "count to EX\n"
            "    --fszg=FS|-F FS         file FS contains one or more source "
            "zone groups\n"
            "    --help|-h               print out usage message\n"
            "    --hex|-H                print response in hexadecimal\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --raw|-r                output response in binary\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending\n"
            "                                 on the interface, may not be "
            "needed\n"
            "    --szg=ZGL|-S ZGL        ZGL is a comma separated list of "
            "source\n"
            "                            zone groups for broadcast\n"
            "    --verbose|-v            increase verbosity\n"
            "    --version|-V            print version string and exit\n\n"
            "Performs a SMP ZONED BROADCAST function. Source zone groups "
            "can be given\nin decimal (default) or hex with a '0x' prefix "
            " or a 'h' suffix.\nBroadcast(Change) will cause an SMP "
            "initiator to run its discover process.\n"
           );
}

/* Read ASCII decimal bytes from fname (a file named '-' taken as stdin).
 * There should be either one entry per line or a comma, space or tab
 * separated list of bytes. By default decimal values are expected in
 * the range from 0 to 255. With a "0x" prefix or a 'h' suffix hex values
 * can be given in the range from 0x to 0xff. Everything from  and
 * including a '#' on a line is ignored. Returns 0 if ok, or 1 if error. */
static int
fd2hex_arr(const char * fname, unsigned char * mp_arr, int * mp_arr_len,
           int max_arr_len)
{
    int fn_len, in_len, k, j, m;
    int h;
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
            pr2serr("Unable to open %s for reading\n", fname);
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

        k = strspn(lcp, "0123456789aAbBcCdDeEfFxXhH ,\t");
        if ((k < in_len) && ('#' != lcp[k])) {
            pr2serr("%s: syntax error at line %d, pos %d\n", __func__, j + 1,
                    m + k + 1);
            goto bad;
        }
        for (k = 0; k < 1024; ++k) {
            h = smp_get_dhnum(lcp);
            if ((h >= 0) && (h < 256)) {
                if (h > 0xff) {
                    pr2serr("%s: hex number larger than 0xff in line %d, "
                            "pos %d\n", __func__, j + 1,
                            (int)(lcp - line + 1));
                    goto bad;
                }
                if ((off + k) >= max_arr_len) {
                    pr2serr("%s: array length exceeded\n", __func__);
                    goto bad;
                }
                mp_arr[off + k] = h;
                if (! isxdigit(*lcp)) {
                    lcp += strspn(lcp, " ,\t");
                }
                lcp = strpbrk(lcp, " ,\t");
                if (NULL == lcp)
                    break;
                lcp += strspn(lcp, " ,\t");
                if ('\0' == *lcp)
                    break;
            } else {
                if ('#' == *lcp) {
                    --k;
                    break;
                }
                pr2serr("%s: error in line %d, at pos %d\n", __func__, j + 1,
                        (int)(lcp - line + 1));
                goto bad;
            }
        }
        off += (k + 1);
    }
    *mp_arr_len = off;
    fclose(fp);
    return 0;
bad:
    fclose(fp);
    return 1;
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
    int res, c, k, len, n, act_resplen;
    int expected_cc = 0;
    const char * fszg = NULL;
    const char * zgl = NULL;
    int do_hex = 0;
    int btype = 0;
    int do_raw = 0;
    int verbose = 0;
    int64_t sa_ll;
    uint64_t sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[1028];
    unsigned char smp_resp[8];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    const char * ccp;
    int ret = 0;

    memset(smp_req, 0, sizeof smp_req);
    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "b:E:F:hHI:rs:S:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'b':
            btype = smp_get_dhnum(optarg);
            if ((btype < 0) || (btype > 15)) {
                pr2serr("bad argument to '--broadcast'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'E':
            expected_cc = smp_get_num(optarg);
            if ((expected_cc < 0) || (expected_cc > 65535)) {
                pr2serr("bad argument to '--expected'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'F':
            fszg = optarg;
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
        case 'r':
            ++do_raw;
            break;
        case 's':
            sa_ll = smp_get_llnum_nomult(optarg);
            if (-1LL == sa_ll) {
                pr2serr("bad argument to '--sa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            sa = (uint64_t)sa_ll;
            break;
        case 'S':
            zgl = optarg;
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
        ccp = getenv("SMP_UTILS_DEVICE");
        if (ccp)
            strncpy(device_name, ccp, sizeof(device_name) - 1);
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
    if (0 == sa) {
        ccp = getenv("SMP_UTILS_SAS_ADDR");
        if (ccp) {
           sa_ll = smp_get_llnum_nomult(ccp);
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
    len = 0;
    if (fszg) {
        if (zgl) {
            pr2serr("can't have both --fszg and --szg options\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
        if (fd2hex_arr(fszg, smp_req + 8, &len, 255)) {
            pr2serr("failed decoding --fszg=FS option\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
    } else if (zgl) {
        for (k = 0; k < 256; ++k) {
            n = smp_get_dhnum(zgl);
            if ((n < 0) || (n > 255)) {
                pr2serr("failed decoding --szg=ZGL option\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            smp_req[8 + k] = n;
            ccp = strchr(zgl, ',');
            if (NULL == ccp)
                break;
            zgl = ccp + 1;
        }
        if (k > 255) {
            pr2serr("failed decoding --szg option, max 255 source zone "
                    "groups\n");
            return SMP_LIB_SYNTAX_ERROR;
        } else
            len = k + 1;
    }
    if (0 == len) {
        pr2serr("didn't detect any source zone group numbers in the input.\n"
                "Give --szg=ZGL or --fszg=FS option (e.g. '--szg=1')\n");
        return SMP_LIB_SYNTAX_ERROR;
    }

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    smp_req[0] = SMP_FRAME_TYPE_REQ;
    smp_req[1] = SMP_FN_ZONED_BROADCAST,
    sg_put_unaligned_be16(expected_cc, smp_req + 4);
    smp_req[6] = btype & 0xf;
    smp_req[7] = len;
    if ((len % 4))
        len = ((len / 4) + 1) * 4;
    smp_req[3] = (len / 4) + 1;
    n = len + 8 + 4;
    if (verbose) {
        pr2serr("    Zoned broadcast request:");
        for (k = 0; k < n; ++k) {
            if (0 == (k % 16))
                pr2serr("\n      ");
            else if (0 == (k % 8))
                pr2serr(" ");
            pr2serr("%02x ", smp_req[k]);
        }
        pr2serr("\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = n;
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
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if ((act_resplen >= 0) && (len > act_resplen)) {
        if (verbose)
            pr2serr("actual response length [%d] less than deduced length "
                    "[%d]\n", act_resplen, len);
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
                pr2serr("Zoned broadcast result: %s\n",
                        smp_get_func_res_str(smp_resp[2], sizeof(b), b));
            ret = smp_resp[2];
        }
        goto err_out;
    }
    if (SMP_FRAME_TYPE_RESP != smp_resp[0]) {
        pr2serr("expected SMP frame response type, got=0x%x\n", smp_resp[0]);
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
        ccp = smp_get_func_res_str(smp_resp[2], sizeof(b), b);
        pr2serr("Zoned broadcast result: %s\n", ccp);
        ret = smp_resp[2];
        goto err_out;
    }
err_out:
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
