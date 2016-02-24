/*
 * Copyright (c) 2011-2013 Douglas Gilbert.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"

/* This is a Serial Attached SCSI (SAS) Serial Management Protocol (SMP)
 * utility.
 *
 * This utility issues a CONFIGURE ZONE MANAGER PASSWORD function and
 * outputs its response.
 */

static const char * version_str = "1.03 20130604";

static struct option long_options[] = {
    {"expected", 1, 0, 'E'},
    {"fpass", 1, 0, 'F'},
    {"help", 0, 0, 'h'},
    {"hex", 0, 0, 'H'},
    {"new-fpass", 1, 0, 'N'},
    {"new-pass", 1, 0, 'n'},
    {"password", 1, 0, 'P'},
    {"raw", 0, 0, 'r'},
    {"sa", 1, 0, 's'},
    {"save", 1, 0, 'S'},
    {"verbose", 0, 0, 'v'},
    {"version", 0, 0, 'V'},
    {0, 0, 0, 0},
};


static void
usage(void)
{
    fprintf(stderr, "Usage: "
          "smp_conf_zone_man_pass [--expected=EX] [--fpass=FP] [--help] "
          "[--hex]\n"
          "                              [--interface=PARAMS] "
          "[--new-fpass=NF]\n"
          "                              [--new-pass=NP] [--password=PA] "
          "[--raw]\n"
          "                              [--sa=SAS_ADDR] [--save=SAV] "
          "[--verbose]\n"
          "                              [--version] SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --expected=EX|-E EX    set expected expander change "
          "count to EX\n"
          "    --fpass=FP|-F FP       file FP contains password, in hex or "
          "ASCII\n"
          "    --help|-h              print out usage message\n"
          "    --hex|-H               print response in hexadecimal\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --new-fpass=NF|-N NF    file NF contains new password, in "
          "hex or\n"
          "                           ASCII\n"
          "    --new-pass=NP|-n NP    new password NP in ASCII, padded with "
          "NULLs to\n"
          "                           be 32 bytes long (def: all NULLs)\n"
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
          "    --save=SAV|-S SAV      SAV: 0 -> shadow (def); 1 -> "
          "saved\n"
          "                           2 -> shadow (and saved if "
          "supported))\n"
          "                           3 -> shadow and saved\n"
          "    --verbose|-v           increase verbosity\n"
          "    --version|-V           print version string and exit\n\n"
          "Performs a SMP CONFIGURE ZONE MANAGER PASSWORD function\n"
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
    const char * cp;
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
                    if ('#' == *lcp) {
                        --k;
                        break;
                    }
                    fprintf(stderr, "f2hex_arr: error in "
                            "line %d, at pos %d\n", j + 1,
                            (int)(lcp - line + 1));
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
    cp = strchr(lcp + 1, *lcp);
    if (NULL == cp) {
        fprintf(stderr, "f2hex_arr: unterminated ASCII string on line "
                "%d, starts: %s\n", j + 1, lcp);
        goto bad;
    }
    k = (cp - lcp) - 1;
    if (k > 0) {
        if ((off + k) > max_arr_len) {
            fprintf(stderr, "f2hex_arr: array length exceeded\n");
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
    const char * nfpass = NULL;
    int do_hex = 0;
    int do_save = 0;
    unsigned char password[32];
    unsigned char npassword[32];
    int do_raw = 0;
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[] = {
                SMP_FRAME_TYPE_REQ, SMP_FN_CONFIG_ZONE_MANAGER_PASS,
                0, 0x11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
    unsigned char smp_resp[8];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 0;

    memset(password, 0, sizeof password);
    memset(npassword, 0, sizeof npassword);
    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "E:F:hHI:n:N:P:rs:S:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'E':
            expected_cc = smp_get_num(optarg);
            if ((expected_cc < 0) || (expected_cc > 65535)) {
                fprintf(stderr, "bad argument to '--expected'\n");
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
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'n':
            len = (int)strlen(optarg);
            if (len > 32) {
                fprintf(stderr, "argument to '--new-pass' too long; "
                        "max 32 got %d\n", len);
                return SMP_LIB_SYNTAX_ERROR;
            }
            memcpy(npassword , optarg, len);
            break;
        case 'N':
            nfpass = optarg;
            break;
        case 'P':
            len = (int)strlen(optarg);
            if (len > 32) {
                fprintf(stderr, "argument to '--password' too long; "
                        "max 32 got %d\n", len);
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
    if ((cp = strchr(device_name, SMP_SUBVALUE_SEPARATOR))) {
        *cp = '\0';
        if (1 != sscanf(cp + 1, "%d", &subvalue)) {
            fprintf(stderr, "expected number after separator in SMP_DEVICE "
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
    if (fpass) {
        if (password[0]) {
            fprintf(stderr, "can't have both --fpass and --password "
                    "options\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
        if (f2hex_arr(fpass, password, &len, sizeof(password))) {
            fprintf(stderr, "failed decoding --fpass=FP option\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if (nfpass) {
        if (npassword[0]) {
            fprintf(stderr, "can't have both --new-pass and --new-fpass "
                    "options\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
        if (f2hex_arr(nfpass, npassword, &len, sizeof(npassword))) {
            fprintf(stderr, "failed decoding --new-fpass=FP option\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
    }

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    smp_req[4] = (expected_cc >> 8) & 0xff;
    smp_req[5] = expected_cc & 0xff;
    smp_req[6] = do_save & 0x3;
    memcpy(smp_req + 8, password, 32);
    memcpy(smp_req + 40, npassword, 32);
    if (verbose) {
        fprintf(stderr, "    Configure zone manager password request:");
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
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if ((act_resplen >= 0) && (len > act_resplen)) {
        if (verbose)
            fprintf(stderr, "actual response length [%d] less than deduced "
                    "length [%d]\n", act_resplen, len);
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
                fprintf(stderr, "Configure zone manager password result: "
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
        fprintf(stderr, "Configure zone manager password result: %s\n", cp);
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
