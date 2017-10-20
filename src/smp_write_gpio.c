/*
 * Copyright (c) 2006-2017 Douglas Gilbert.
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

/* This is a Serial Attached SCSI (SAS) Serial Management Protocol (SMP)
 * utility.
 *
 * This utility issues a WRITE GPIO REGISTER request and outputs its
 * response.  The WRITE GPIO REGISTER function is defined in SFF-8485
 * 0.7 [2006/02/01].
 *
 * Based on a comment by Rob Elliott in this t10 document : 08-212r8.pdf
 * (page 871 or 552) the ENHANCED variant request has its 4-byte header
 * changed to comply with other SAS-2 SMP requests. This will increase
 * the byte position by 2 of the register type, index and count fields.
 * The remaining write data (first..last register) is not moved.
 */

static const char * version_str = "1.13 20171017";

#define SMP_MAX_REQ_LEN (1020 + 4 + 4)

static struct option long_options[] = {
    {"count", required_argument, 0, 'c'},
    {"data", required_argument, 0, 'd'},
    {"enhanced", no_argument, 0, 'E'},
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"index", required_argument, 0, 'i'},
    {"interface", required_argument, 0, 'I'},
    {"phy", required_argument, 0, 'p'},
    {"raw", no_argument, 0, 'r'},
    {"sa", required_argument, 0, 's'},
    {"type", no_argument, 0, 't'},
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
    pr2serr("Usage: smp_write_gpio [--count=CO] [--data=H,H...] [--enhanced] "
            "[--help]\n"
            "                      [--hex] [--index=IN] [--interface=PARAMS] "
            "[--raw]\n"
            "                      [--sa=SAS_ADDR] [type=TY] [--verbose] "
            "[--version]\n"
            "                      SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --count=CO|-c CO     register count (dwords to write) "
            "(def: 1)\n"
            "    --data=H,H...|-d H,H...    comma separated list of hex "
            "bytes to write\n"
            "    --data=-|-d -        read stdin for hex bytes to write\n"
            "    --enhanced|-E        use WRITE GPIO REGISTER ENHANCED "
            "function\n"
            "    --help|-h            print out usage message\n"
            "    --hex|-H             print response in hexadecimal\n"
            "    --index=IN|-i IN     register index (def: 0)\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --raw|-r             output response in binary\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending on\n"
            "                                 the interface, may not be "
            "needed\n"
            "    --type=TY|-t TY      register type (def: 0 (GPIO_CFG))\n"
            "    --verbose|-v         increase verbosity\n"
            "    --version|-V         print version string and exit\n\n"
            "Performs a SMP WRITE GPIO REGISTER (default) or SMP WRITE GPIO "
            "REGISTER\nENHANCED function\n"
           );
}

static void
dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

static int
read_hex(const char * inp, unsigned char * arr, int * arr_len)
{
    int in_len, k, j, m, off;
    unsigned int h;
    const char * lcp;
    const char * cp;
    char line[1024];

    if ((NULL == inp) || (NULL == arr) || (NULL == arr_len))
        return 1;
    lcp = inp;
    in_len = strlen(inp);
    if (0 == in_len) {
        *arr_len = 0;
    }
    if ('-' == inp[0]) {        /* read from stdin */
        for (j = 0, off = 0; j < 1024; ++j) {
            if (NULL == fgets(line, sizeof(line), stdin))
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
            k = strspn(lcp, "0123456789aAbBcCdDeEfF ,\t");
            if (in_len != k) {
                pr2serr("%s: syntax error at line %d, pos %d\n", __func__,
                        j + 1, m + k + 1);
                return 1;
            }
            for (k = 0; k < 1024; ++k) {
                if (1 == sscanf(lcp, "%x", &h)) {
                    if (h > 0xff) {
                        pr2serr("%s: hex number larger than 0xff in line %d, "
                                "pos %d\n", __func__, j + 1,
                                (int)(lcp - line + 1));
                        return 1;
                    }
                    arr[off + k] = h;
                    lcp = strpbrk(lcp, " ,\t");
                    if (NULL == lcp)
                        break;
                    lcp += strspn(lcp, " ,\t");
                    if ('\0' == *lcp)
                        break;
                } else {
                    pr2serr("%s: error in line %d, at pos %d\n", __func__,
                            j + 1, (int)(lcp - line + 1));
                    return 1;
                }
            }
            off += k + 1;
        }
        *arr_len = off;
    } else {        /* hex string on command line */
        k = strspn(inp, "0123456789aAbBcCdDeEfF,");
        if (in_len != k) {
            pr2serr("%s: error at pos %d\n", __func__, k + 1);
            return 1;
        }
        for (k = 0; k < 1024; ++k) {
            if (1 == sscanf(lcp, "%x", &h)) {
                if (h > 0xff) {
                    pr2serr("%s: hex number larger than 0xff at pos %d\n",
                            __func__, (int)(lcp - inp + 1));
                    return 1;
                }
                arr[k] = h;
                cp = strchr(lcp, ',');
                if (NULL == cp)
                    break;
                lcp = cp + 1;
            } else {
                pr2serr("%s: error at pos %d\n", __func__,
                        (int)(lcp - inp + 1));
                return 1;
            }
        }
        *arr_len = k + 1;
    }
    return 0;
}


int
main(int argc, char * argv[])
{
    bool do_data = false;
    bool enhanced = false;
    bool do_raw = false;
    int res, c, k, len, act_resplen, off;
    int arr_len = 0;
    int do_hex = 0;
    int rindex = 0;
    int phy_id = 0;
    int rcount = 1;
    int ret = 0;
    int rtype = 0;
    int subvalue = 0;
    int verbose = 0;
    int64_t sa_ll;
    uint64_t sa = 0;
    char * cp;
    char b[128];
    char i_params[256];
    char device_name[512];
    unsigned char smp_req[SMP_MAX_REQ_LEN];
    unsigned char smp_resp[8];
    struct smp_target_obj tobj;
    struct smp_req_resp smp_rr;
    unsigned char data_arr[1024];

    memset(device_name, 0, sizeof device_name);
    memset(i_params, 0, sizeof i_params);
    memset(smp_req, 0, sizeof smp_req);
    smp_req[0] = SMP_FRAME_TYPE_REQ;
    smp_req[1] = SMP_FN_WRITE_GPIO_REG;
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "c:d:EhHi:I:p:rs:t:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'c':
            rcount = smp_get_num(optarg);
            if ((rcount < 1) || (rcount > 255)) {
                pr2serr("bad argument to '--count'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'd':
            memset(data_arr, 0, sizeof(data_arr));
            if (read_hex(optarg, data_arr, &arr_len)) {
                pr2serr("bad argument to '--data'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            do_data = true;
            break;
        case 'E':
            enhanced = true;
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++do_hex;
            break;
        case 'i':
            rindex = smp_get_num(optarg);
            if ((rindex < 0) || (rindex > 255)) {
                pr2serr("bad argument to '--index'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'p':
           phy_id = smp_get_num(optarg);
           if ((phy_id < 0) || (phy_id > 254)) {
                pr2serr("bad argument to '--phy', expect value from 0 to "
                        "254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            if (verbose)
                pr2serr("'--phy=<n>' option not needed so ignored\n");
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
        case 't':
            rtype = smp_get_num(optarg);
            if ((rtype < 0) || (rtype > 255)) {
                pr2serr("bad argument to '--type'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
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
    if ((! do_data) || (arr_len < 1)) {
        pr2serr("need to supply data to write, see '--data=' option\n");
        usage();
        return SMP_LIB_SYNTAX_ERROR;
    }
    if ((rcount * 4) != arr_len) {
        pr2serr("number of data bytes given (%d) needs to be 4 times count "
                "(%d)\n", arr_len, rcount);
        return SMP_LIB_SYNTAX_ERROR;
    }
    if (0 == device_name[0]) {
        cp = getenv("SMP_UTILS_DEVICE");
        if (cp)
            strncpy(device_name, cp, sizeof(device_name) - 1);
        else {
            pr2serr("missing device name on command line\n    [Could use"
                    " environment variable SMP_UTILS_DEVICE instead]\n");
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

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;
    if (enhanced) {
        smp_req[1] = SMP_FN_WRITE_GPIO_REG_ENH;
        smp_req[2] = 0x0;       /* response is only header+CRC */
        smp_req[3] = rcount + 1;/* request: extra dword for register fields */
        off = 2;
    } else
        off = 0;
    smp_req[2 + off] = rtype;
    smp_req[3 + off] = rindex;
    smp_req[4 + off] = rcount;
    for (k = 0; k < arr_len; ++k)
        smp_req[8 + k] = data_arr[k];
    if (verbose) {
        pr2serr("    Write GPIO register%s request: ",
                (enhanced ? " enhanced" : ""));
        for (k = 0; k < (12 + arr_len); ++k)
            pr2serr("%02x ", smp_req[k]);
        pr2serr("\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = 12 + arr_len;
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
    len = 4 + (rcount * 4);      /* length in bytes, excluding 4 byte CRC */
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
        else if (smp_resp[2])
            ret = smp_resp[2];
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
        ret = smp_resp[2];
        cp = smp_get_func_res_str(ret, sizeof(b), b);
        pr2serr("Write gpio register%s result: %s\n",
                (enhanced ? " enhanced" : ""), cp);
        goto err_out;
    }

err_out:
    res = smp_initiator_close(&tobj);
    if (res < 0) {
        if (0 == ret)
            return SMP_LIB_FILE_ERROR;
    }
    if (ret < 0)
        ret = SMP_LIB_CAT_OTHER;
    if (verbose && ret)
        pr2serr("Exit status %d indicates error detected\n", ret);
    return ret;
}
