/*
 * Copyright (c) 2006 Douglas Gilbert.
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
 * This utility issues a READ GPIO REGISTER request and outputs its
 * response.
 */

static char * version_str = "1.01 20060608";

#define ME "smp_read_gpio: "


static struct option long_options[] = {
        {"count", 1, 0, 'c'},
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
          "smp_read_gpio   [--count=<n>] [--help] [--hex] [--index=<n>]\n"
          "                       [--interface=<params>] [--raw] "
          " [--sa=<sas_addr>]\n"
          "                       [type=<n>] [--verbose] [--version] "
          "<smp_device>[,<n>]\n"
          "  where: --count=<n>|-c <n>   register count (dwords to read) "
          "(def: 1)\n"
          "         --help|-h            print out usage message\n"
          "         --hex|-H             print response in hexadecimal\n"
          "         --index=<n>|-i <n>   register index (def: 0)\n"
          "         --interface=<params>|-I <params>   specify or override "
          "interface\n"
          "         --raw|-r             output response in binary\n"
          "         --sa=<sas_addr>|-s <sas_addr>   SAS address of SMP target "
          "(use\n"
          "                              leading '0x' or trailing 'h'). "
          "Depending on\n"
          "                              the interface, may not be needed\n"
          "         --type=<n>|-t <n>    register type (def: 0 (GPIO_CFG))\n"
          "         --verbose|-v         increase verbosity\n"
          "         --version|-V         print version string and exit\n\n"
          "Performs a SMP READ GPIO REGISTER function\n"
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
    int res, c, k, len, off, decoded;
    int rcount = 1;
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
    unsigned char smp_resp[1024 + 16];
    struct smp_target_obj tobj;
    struct smp_req_resp smp_rr;
    int subvalue = 0;
    char * cp;
    int ret = 1;

    memset(device_name, 0, sizeof device_name);
    memset(i_params, 0, sizeof i_params);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "c:hHi:I:p:rs:t:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'c':
            rcount = smp_get_num(optarg);
            if ((rcount < 1) || (rcount > 255)) {
                fprintf(stderr, "bad argument to '--count'\n");
                return 1;
            }
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
                return 1;
            }
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'p':
           phy_id = smp_get_num(optarg);
           if ((phy_id < 0) || (phy_id > 127)) {
                fprintf(stderr, "bad argument to '--phy'\n");
                return 1;
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
                return 1;
            }
            sa = (unsigned long long)sa_ll;
            break;
        case 't':
            rtype = smp_get_num(optarg);
            if ((rtype < 0) || (rtype > 255)) {
                fprintf(stderr, "bad argument to '--type'\n");
                return 1;
            }
            break;
        case 'v':
            ++verbose;
            break;
        case 'V':
            fprintf(stderr, ME "version: %s\n", version_str);
            return 0;
        default:
            fprintf(stderr, "unrecognised switch code 0x%x ??\n", c);
            usage();
            return 1;
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
            return 1;
        }
    }
    if (0 == device_name[0]) {
        cp = getenv("SMP_UTILS_DEVICE");
        if (cp)
            strncpy(device_name, cp, sizeof(device_name) - 1);
        else {
            fprintf(stderr, "missing device name!\n    [Could use "
                    "environment variable SMP_UTILS_DEVICE instead]\n");
            usage();
            return 1;
        }
    }
    if ((cp = strchr(device_name, ','))) {
        *cp = '\0';
        if (1 != sscanf(cp + 1, "%d", &subvalue)) {
            fprintf(stderr, "expected number after comma in <device> name\n");
            return 1;
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
            fprintf(stderr, "SAS (target) address not in naa-5 format\n");
            if ('\0' == i_params[0]) {
                fprintf(stderr, "    use any '--interface=' to continue\n");
                return 1;
            }
        }
    }

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return 1;
    smp_req[2] = rtype;
    smp_req[3] = rindex;
    smp_req[4] = rcount;
    if (verbose) {
        fprintf(stderr, "    Read GPIO register request: ");
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
        goto err_out;
    }
    if (smp_rr.transport_err) {
        fprintf(stderr, "smp_send_req transport_error=%d\n",
                smp_rr.transport_err);
        goto err_out;
    }
    if ((smp_rr.act_response_len >= 0) && (smp_rr.act_response_len < 4)) {
        fprintf(stderr, "response too short, len=%d\n",
                smp_rr.act_response_len);
        goto err_out;
    }
    len = 4 + (rcount * 4);      /* length in bytes, excluding 4 byte CRC */
    if (do_hex) {
        dStrHex((const char *)smp_resp, len, 1);
        ret = 0;
        goto err_out;
    } else if (do_raw) {
        dStrRaw((const char *)smp_resp, len);
        ret = 0;
        goto err_out;
    }
    if (SMP_FRAME_TYPE_RESP != smp_resp[0]) {
        fprintf(stderr, "expected SMP frame response type, got=0x%x\n",
                smp_resp[0]);
        goto err_out;
    }
    if (smp_resp[1] != smp_req[1]) {
        fprintf(stderr, "Expected function code=0x%x, got=0x%x\n",
                smp_req[1], smp_resp[1]);
        goto err_out;

    }
    ret = 0;
    printf("Read GPIO register response:\n");
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
        return 1;
    }
    return ret;
}
