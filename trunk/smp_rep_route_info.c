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
 * This utility issues a REPORT ROUTE INFORMATION function and outputs its
 * response.
 */

static char * version_str = "1.01 20060608";

#define ME "smp_rep_route_info: "


static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"index", 1, 0, 'i'},
        {"interface", 1, 0, 'I'},
        {"phy", 1, 0, 'p'},
        {"raw", 0, 0, 'r'},
        {"sa", 1, 0, 's'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

static void usage()
{
    fprintf(stderr, "Usage: "
          "smp_rep_route_info [--help] [--hex] [--index=<n>] "
          "[--interface=<params>]\n"
          "                       [--phy=<n>] [--raw] [--sa=<sas_addr>] "
          "[--verbose]\n"
          "                       [--version] <smp_device>[,<n>]\n"
          "  where: --help|-h            print out usage message\n"
          "         --hex|-H             print response in hexadecimal\n"
          "         --index=<n>|-i <n>   expander route index (def: 0)\n"
          "         --interface=<params>|-I <params>   specify or override "
          "interface\n"
          "         --phy=<n>|-p <n>     phy identifier (def: 0)\n"
          "         --raw|-r             output response in binary\n"
          "         --sa=<sas_addr>|-s <sas_addr>   SAS address of SMP "
          "target (use\n"
          "                              leading '0x' or trailing 'h'). "
          "Depending on\n"
          "                              the interface, may not be needed\n"
          "         --verbose|-v         increase verbosity\n"
          "         --version|-V         print version string and exit\n\n"
          "Performs a SMP REPORT ROUTE INFORMATION function\n"
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
    int res, c, k, j, len;
    int do_hex = 0;
    int er_ind = 0;
    int phy_id = 0;
    int do_raw = 0;
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    unsigned long long ull;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_ROUTE_INFO,
                               0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char smp_resp[128];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 1;

    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "hHi:I:p:rs:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
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
        case 'i':
           er_ind = smp_get_num(optarg);
           if ((er_ind < 0) || (er_ind > 65535)) {
                fprintf(stderr, "bad argument to '--index'\n");
                return 1;
            }
            break;
        case 'p':
           phy_id = smp_get_num(optarg);
           if ((phy_id < 0) || (phy_id > 127)) {
                fprintf(stderr, "bad argument to '--phy'\n");
                return 1;
            }
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

    smp_req[6] = (er_ind >> 8) & 0xff;
    smp_req[7] = er_ind & 0xff;
    smp_req[9] = phy_id;
    if (verbose) {
        fprintf(stderr, "    Report route information request: ");
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
    len = smp_resp[3];
    if (0 == len) {
        len = smp_get_func_def_resp_len(smp_resp[1]);
        if (len < 0) {
            fprintf(stderr, "unable to determine reponse length\n");
            goto err_out;
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
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
    if (smp_resp[2]) {
        cp = smp_get_func_res_str(smp_resp[2], sizeof(b), b);
        fprintf(stderr, "Report route information result: %s\n", cp);
        goto err_out;
    }
    ret = 0;
    printf("Report route information response:\n");
    printf("  expander route index: %d\n", (smp_resp[6] << 8) + smp_resp[7]);
    printf("  phy identifier: %d\n", smp_resp[9]);
    printf("  expander route entry disabled: %d\n", !!(smp_resp[12] & 0x80));
    if ((! (smp_resp[12] & 0x80)) || (verbose > 0)) {
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= smp_resp[16 + j];
        }
        printf("  routed SAS address: 0x%llx\n", ull);
    }
err_out:
    res = smp_initiator_close(&tobj);
    if (res < 0) {
        fprintf(stderr, ME "close error: %s\n", safe_strerror(errno));
        return 1;
    }
    return ret;
}
