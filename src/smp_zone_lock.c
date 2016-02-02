/*
 * Copyright (c) 2011-2016 Douglas Gilbert.
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
 * This utility issues a ZONE LOCK function and outputs its response.
 */

static const char * version_str = "1.05 20160201";

static struct option long_options[] = {
    {"expected", 1, 0, 'E'},
    {"fpass", 1, 0, 'F'},
    {"help", 0, 0, 'h'},
    {"hex", 0, 0, 'H'},
    {"inactivity", 1, 0, 'i'},
    {"interface", 1, 0, 'I'},
    {"password", 1, 0, 'P'},
    {"raw", 0, 0, 'r'},
    {"sa", 1, 0, 's'},
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
    pr2serr("Usage: smp_zone_lock [--expected=EX] [--fpass=FP] [--help] "
            "[--hex]\n"
            "                     [--inactivity=TL] [--interface=PARAMS]\n"
            "                     [--password=PA] [--raw] "
            "[--sa=SAS_ADDR]\n"
            "                     [--verbose] [--version] SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --expected=EX|-E EX    set expected expander change "
            "count to EX\n"
            "    --fpass=FP|-F FP       file FP contains password, in hex or "
            "ASCII\n"
            "    --help|-h              print out usage message\n"
            "    --hex|-H               print response in hexadecimal\n"
            "    --inactivity=TL|-i TL    TL is inactivity time limit "
            "(units: 100ms)\n"
            "                             (def: 0 -> no time limit)\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --password=PA|-P PA    password PA in ASCII, padded with "
            "NULLs to\n"
            "                           be 32 bytes long (def: all NULLs)\n"
            "    --raw|-r               output response in binary\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending on\n"
            "                                 the interface, may not be "
            "needed\n"
            "    --verbose|-v           increase verbosity\n"
            "    --version|-V           print version string and exit\n\n"
            "Performs a SMP ZONE LOCK function\n"
            );
}

/* Read ASCII hex bytes from fname (a file named '-' taken as stdin).
 * There should be either one entry per line or a comma, space or tab
 * separated list of bytes. The first ASCII hex string detected has its
 * length checked; if the length is 1 or 2 then bytes are expected to
 * be space, comma or tab separated; if the length is 3 or more then a
 * string of ACSII hex digits is expected, 2 per byte. Everything from
 * and including a '#' on a line is ignored. If the first non-space
 * character on a line is quote or double quote then the ASCII string
 * up to the next quote or double quote on that line is translated to
 * binary (with each ASCII character translated to the binary byte
 * equivalent). Only the first such quoted ASCII string is translated.
 * If '-1' is detected at the start of a line then the rest of the
 * map_arr is filled with bytes of 0xff. Returns 0 if ok, or 1 if error. */
static int
f2hex_arr(const char * fname, unsigned char * mp_arr, int * mp_arr_len,
          int max_arr_len)
{
    int fn_len, in_len, k, j, m;
    int no_space = 0;
    int checked_hexlen = 0;
    unsigned int h;
    const char * lcp;
    const char * ccp;
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
        if (('\'' == *lcp) || ('"' == *lcp))
            goto astring;
        if (('-' == *lcp) && ('1' == *(lcp + 1)))
            goto minus1;
        if (! checked_hexlen) {
            ++checked_hexlen;
            k = strspn(lcp, "0123456789aAbBcCdDeEfF");
            if (k > 2)
                no_space = 1;
        }

        k = strspn(lcp, "0123456789aAbBcCdDeEfF ,\t");
        if ((k < in_len) && ('#' != lcp[k])) {
            pr2serr("%s: syntax error at line %d, pos %d\n", __func__, j + 1,
                    m + k + 1);
            goto bad;
        }
        if (no_space) {
            for (k = 0; isxdigit(*lcp) && isxdigit(*(lcp + 1));
                 ++k, lcp += 2) {
                if (1 != sscanf(lcp, "%2x", &h)) {
                    pr2serr("%s: bad hex number in line %d, pos %d\n",
                            __func__, j + 1, (int)(lcp - line + 1));
                    goto bad;
                }
                if ((off + k) >= max_arr_len) {
                    pr2serr("%s: array length exceeded\n", __func__);
                    goto bad;
                }
                mp_arr[off + k] = h;
            }
            off += k;
        } else {
            for (k = 0; k < 1024; ++k) {
                if (1 == sscanf(lcp, "%x", &h)) {
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
                    pr2serr("%s: error in line %d, at pos %d\n", __func__,
                            j + 1, (int)(lcp - line + 1));
                    goto bad;
                }
            }
            off += (k + 1);
        }
    }
    *mp_arr_len = off;
    fclose(fp);
    return 0;
astring:
    ccp = strchr(lcp + 1, *lcp);
    if (NULL == ccp) {
        pr2serr("%s: unterminated ASCII string on line %d, starts: %s\n",
                __func__, j + 1, lcp);
        goto bad;
    }
    k = (ccp - lcp) - 1;
    if (k > 0) {
        if ((off + k) > max_arr_len) {
            pr2serr("%s: array length exceeded\n", __func__);
            goto bad;
        }
        memcpy(mp_arr + off, lcp + 1, k);
    }
    off += k;
    *mp_arr_len = off;
    fclose(fp);
    return 0;
minus1:
    for (k = off; k < max_arr_len; ++k)
        mp_arr[k] = 0xff;
    *mp_arr_len = k;
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
    int res, c, k, len, act_resplen;
    int expected_cc = 0;
    const char * fpass = NULL;
    int do_hex = 0;
    int inact_tl = 0;
    unsigned char password[32];
    int do_raw = 0;
    int verbose = 0;
    int64_t sa_ll;
    uint64_t sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_ZONE_LOCK,
                               0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
    unsigned char smp_resp[20];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    const char * ccp;
    int ret = 0;

    memset(password, 0, sizeof password);
    memset(device_name, 0, sizeof device_name);
    memset(smp_resp, 0, sizeof smp_resp);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "E:F:hHi:I:P:rs:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'E':
            expected_cc = smp_get_num(optarg);
            if ((expected_cc < 0) || (expected_cc > 65535)) {
                pr2serr("bad argument to '--expected'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'F':
            fpass = optarg;
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++do_hex;
            break;
        case 'i':
            inact_tl = smp_get_num(optarg);
            if ((inact_tl < 0) || (inact_tl > 65535)) {
                pr2serr("bad argument to '--inactivity'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'P':
            len = (int)strlen(optarg);
            if (len > 32) {
                pr2serr("argument to '--password' too long; max 32 got %d\n",
                        len);
                return SMP_LIB_SYNTAX_ERROR;
            }
            memcpy(password , optarg, len);
            break;
        case 'r':
            ++do_raw;
            break;
        case 's':
            sa_ll = smp_get_llnum(optarg);
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
           sa_ll = smp_get_llnum(ccp);
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
    if (fpass) {
        if (password[0]) {
            pr2serr("can't have both --fpass and --password options\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
        if (f2hex_arr(fpass, password, &len, sizeof(password))) {
            pr2serr("failed decoding --fpass=FP option\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
    }

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    smp_req[2] = (sizeof(smp_resp) - 8) / 4;
    sg_put_unaligned_be16(expected_cc, smp_req + 4);
    sg_put_unaligned_be16(inact_tl, smp_req + 6);
    memcpy(smp_req + 8, password, 32);
    if (verbose) {
        pr2serr("    Zone lock request:");
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
                pr2serr("Zone lock result: %s\n",
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
        pr2serr("Zone lock result: %s\n", ccp);
        ret = smp_resp[2];
        if (smp_resp[8] | smp_resp[9] | smp_resp[10] | smp_resp[11]) {
            pr2serr("Active zone manager SAS address (hex): ");
            for (k = 0; k < 8; ++k)
                pr2serr("%02x", smp_resp[8 + k]);
            pr2serr("\n");
        }
        goto err_out;
    }
    printf("Active zone manager SAS address (hex): ");
    for (k = 0; k < 8; ++k)
        printf("%02x", smp_resp[8 + k]);
    printf("\n");

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
