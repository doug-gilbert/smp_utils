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
 * This utility issues a READ GPIO REGISTER or READ GPIO REGISTER ENHANCED
 * request and outputs its response. The READ GPIO REGISTER function is
 * defined in SFF-8485 0.7 [2006/02/01].
 *
 * Based on a comment by Rob Elliott in this t10 document : 08-212r8.pdf
 * (page 871 or 552) the ENHANCED variant request has its 4-byte header
 * changed to comply with other SAS-2 SMP requests. This will increase
 * the byte position by 2 of the register type, index and count fields.
 */

static char * version_str = "1.09 20110805";

#define SMP_MAX_RESP_LEN (1020 + 4 + 4)


static struct option long_options[] = {
        {"count", 1, 0, 'c'},
        {"enhanced", 0, 0, 'E'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"index", 1, 0, 'i'},
        {"interface", 1, 0, 'I'},
        {"phy", 1, 0, 'p'},
        {"raw", 0, 0, 'r'},
        {"sa", 1, 0, 's'},
        {"type", 0, 0, 't'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

static void usage()
{
    fprintf(stderr, "Usage: "
          "smp_read_gpio   [--count=CO] [--enhanced] [--help] [--hex]\n"
          "                       [--index=IN] [--interface=PARAMS] [--raw]\n"
          "                       [--sa=SAS_ADDR] [type=TY] [--verbose] "
          "[--version]\n"
          "                       SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --count=CO|-c CO     register count (dwords to read) "
          "(def: 1)\n"
          "    --enhanced|-E        use READ GPIO REGISTER ENHANCED "
          "function\n"
          "    --help|-h            print out usage message\n"
          "    --hex|-H             print response in hexadecimal\n"
          "    --index=IN|-i IN     register index (def: 0)\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --raw|-r             output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading '0x'\n"
          "                         or trailing 'h'). Depending on "
          "the interface, may\n"
          "                         not be needed\n"
          "    --type=TY|-t TY      register type (def: 0 (GPIO_CFG))\n"
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n\n"
          "Performs a SMP READ GPIO REGISTER (default) or READ GPIO "
          "REGISTER ENHANCED\nfunction\n"
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
    int res, c, k, len, off, decoded, act_resplen;
    int rcount = 1;
    int enhanced = 0;
    int do_hex = 0;
    int rindex = 0;
    int phy_id = 0;
    int do_raw = 0;
    int rtype = 0;
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    char i_params[256];
    char device_name[512];
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_READ_GPIO_REG,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char smp_resp[SMP_MAX_RESP_LEN];
    struct smp_target_obj tobj;
    struct smp_req_resp smp_rr;
    int subvalue = 0;
    char * cp;
    int ret = 0;
    char b[128];

    memset(device_name, 0, sizeof device_name);
    memset(i_params, 0, sizeof i_params);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "c:EhHi:I:p:rs:t:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'c':
            rcount = smp_get_num(optarg);
            if ((rcount < 1) || (rcount > 255)) {
                fprintf(stderr, "bad argument to '--count'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'E':
            ++enhanced;
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
                fprintf(stderr, "bad argument to '--index'\n");
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
                fprintf(stderr, "bad argument to '--phy', expect "
                        "value from 0 to 254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            if (verbose)
                fprintf(stderr, "'--phy=<n>' option not needed so "
                        "ignored\n");
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
        case 't':
            rtype = smp_get_num(optarg);
            if ((rtype < 0) || (rtype > 255)) {
                fprintf(stderr, "bad argument to '--type'\n");
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

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;
    if (enhanced) {
        smp_req[1] = SMP_FN_READ_GPIO_REG_ENH;
        smp_req[2] = rcount;    /* response payload in dwords */
        smp_req[3] = 0x1;       /* 12 byte request */
        off = 2;
    } else
        off = 0;
    smp_req[2 + off] = rtype;
    smp_req[3 + off] = rindex;
    smp_req[4 + off] = rcount;
    if (verbose) {
        fprintf(stderr, "    Read GPIO register%s request: ",
                (enhanced ? " enhanced" : ""));
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
    act_resplen = smp_rr.act_response_len;
    if ((act_resplen >= 0) && (act_resplen < 4)) {
        fprintf(stderr, "response too short, len=%d\n", act_resplen);
        ret = SMP_LIB_CAT_MALFORMED;
        goto err_out;
    }
    if (enhanced) {
        len = smp_resp[3];
        if ((len != rcount) && verbose)
            fprintf(stderr, "requested %d dwords but received %d\n", rcount, len);
    } else
        len = rcount;
    len = 4 + (len * 4);      /* length in bytes, excluding 4 byte CRC */
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
        else if (smp_resp[2])
            ret = smp_resp[2];
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
        ret = smp_resp[2];
        cp = smp_get_func_res_str(ret, sizeof(b), b);
        fprintf(stderr, "Read gpio register%s result: %s\n",
                (enhanced ? " enhanced" : ""), cp);
        goto err_out;
    }
    printf("Read GPIO register%s response:\n",
           (enhanced ? " enhanced" : ""));
    decoded = 0;
    if (0 == rtype) {
        off = 4;
        if (0 == rindex) {
            printf("  GPIO_CFG[0]:\n");
            printf("    version: %d\n", (smp_resp[off + 1] & 0xf));
            printf("    GPIO enable: %d\n", !!(smp_resp[off + 2] & 0x80));
            printf("    cfg register count: %d\n",
                   ((smp_resp[off + 2] >> 4) & 0x7));
            printf("    gp register count: %d\n", (smp_resp[off + 2] & 0xf));
            printf("    supported drive count: %d\n", smp_resp[off + 3]);
            ++decoded;
            off += 4;
        }
        if ((1 == rindex) || ((0 == rindex) && (rcount > 1))) {
            printf("  GPIO_CFG[1]:\n");
            printf("    blink generator rate B: %d\n",
                   ((smp_resp[off + 1] >> 4) & 0xf));
            printf("    blink generator rate A: %d\n",
                   (smp_resp[off + 1] & 0xf));
            printf("    force activity off: %d\n",
                   ((smp_resp[off + 2] >> 4) & 0xf));
            printf("    maximum activity on: %d\n",
                   (smp_resp[off + 2] & 0xf));
            printf("    stretch activity off: %d\n",
                   ((smp_resp[off + 3] >> 4) & 0xf));
            printf("    stretch activity on: %d\n",
                   (smp_resp[off + 3] & 0xf));
            ++decoded;
        }
    }
    if (rcount > decoded) {
        fprintf(stderr, "  only simple cfg registers decoded, others were "
                "requested\n");
        fprintf(stderr, "    use either '--hex' or '--raw' option to "
                "output other registers\n");
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
        fprintf(stderr, "Exit status %d indicates error detected\n", ret);
    return ret;
}
