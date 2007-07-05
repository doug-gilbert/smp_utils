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
 * This utility issues a DISCOVER function and outputs its response.
 */

static char * version_str = "1.02 20060608";

#define ME "smp_discover: "


static struct option long_options[] = {
        {"brief", 0, 0, 'b'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"interface", 1, 0, 'I'},
        {"phy", 1, 0, 'p'},
        {"sa", 1, 0, 's'},
        {"raw", 0, 0, 'r'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

static void usage()
{
    fprintf(stderr, "Usage: "
          "smp_discover   [--brief] [--help] [--hex] [--interface=<params>]\n"
          "                      [--phy=<n>] [--raw] [--sa=<sas_addr>] "
          "[--verbose]\n"
          "                      [--version] <smp_device>[,<n>]\n"
          "  where: --brief|-b           brief decoded output\n"
          "         --help|-h            print out usage message\n"
          "         --hex|-H             print response in hexadecimal\n"
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
          "Performs a SMP DISCOVER function\n"
          );

}

static void dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

static char * smp_attached_device_type[] = {
    "no device attached",
    "end device",
    "edge expander device",
    "fanout expander device",
};

static char * smp_get_plink_rate(int val, int prog, int b_len, char * b)
{
    switch (val) {
    case 0:
        snprintf(b, b_len, "not programmable");
        break;
    case 8:
        snprintf(b, b_len, "1.5 Gbps");
        break;
    case 9:
        snprintf(b, b_len, "3 Gbps");
        break;
    case 0xa:
        snprintf(b, b_len, "6 Gbps");
        break;
    default:
        if (prog && (0 == val))
            snprintf(b, b_len, "not programmable");
        else
            snprintf(b, b_len, "reserved [%d]", val);
        break;
    }
    return b;
}

static char * find_sas_connector_type(int conn_type, char * buff,
                                      int buff_len)
{
    switch (conn_type) {
    case 0x0:
        snprintf(buff, buff_len, "No information");
        break;
    case 0x1:
        snprintf(buff, buff_len, "SAS 4x receptacle (SFF-8470) "
                 "[max 4 phys]");
        break;
    case 0x2:
        snprintf(buff, buff_len, "Mini SAS 4x receptacle (SFF-8088) "
                 "[max 4 phys]");
        break;
    case 0xf:
        snprintf(buff, buff_len, "Vendor specific external connector");
        break;
    case 0x10:
        snprintf(buff, buff_len, "SAS 4i plug (SFF-8484) [max 4 phys]");
        break;
    case 0x11:
        snprintf(buff, buff_len, "Mini SAS 4i receptacle (SFF-8087) "
                 "[max 4 phys]");
        break;
    case 0x20:
        snprintf(buff, buff_len, "SAS Drive backplane receptacle (SFF-8482) "
                 "[max 2 phys]");
        break;
    case 0x21:
        snprintf(buff, buff_len, "SATA host plug [max 1 phy]");
        break;
    case 0x22:
        snprintf(buff, buff_len, "SAS Drive plug (SFF-8482) [max 2 phys]");
        break;
    case 0x23:
        snprintf(buff, buff_len, "SATA device plug [max 1 phy]");
        break;
    case 0x3f:
        snprintf(buff, buff_len, "Vendor specific internal connector");
        break;
    default:
        if (conn_type < 0x10)
            snprintf(buff, buff_len, "unknown external connector type: 0x%x",
                     conn_type);
        else if (conn_type < 0x20)
            snprintf(buff, buff_len, "unknown internal wide connector type: "
                     "0x%x", conn_type);
        else if (conn_type < 0x30)
            snprintf(buff, buff_len, "unknown internal connector to end "
                     "device, type: 0x%x", conn_type);
        else if (conn_type < 0x70)
            snprintf(buff, buff_len, "reserved connector type: 0x%x",
                     conn_type);
        else if (conn_type < 0x80)
            snprintf(buff, buff_len, "vendor specific connector type: 0x%x",
                     conn_type);
        else    /* conn_type is a 7 bit field, so this is impossible */
            snprintf(buff, buff_len, "unexpected connector type: 0x%x",
                     conn_type);
        break;
    }
    return buff;
}


int main(int argc, char * argv[])
{
    int res, c, k, j, len;
    int do_brief = 0;
    int do_hex = 0;
    int phy_id = 0;
    int do_raw = 0;
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    unsigned long long ull;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_DISCOVER, 0, 2,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char smp_resp[128];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 1;

    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "bhHI:p:rs:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'b':
            ++do_brief;
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

    smp_req[9] = phy_id;
    if (verbose) {
        fprintf(stderr, "    Discover request: ");
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
        fprintf(stderr, "Discover result: %s\n", cp);
        goto err_out;
    }
    ret = 0;
    printf("Discover response%s:\n", (do_brief ? " (brief)" : ""));
    printf("  phy identifier: %d\n", smp_resp[9]);
    res = ((0x70 & smp_resp[12]) >> 4);
    if (res < 4)
        printf("  attached device type: %s\n", smp_attached_device_type[res]);
    else
        printf("  attached device type: reserved [%d]\n", res);
    if ((do_brief > 1) && (0 == res))
        goto err_out;

    res = (0xf & smp_resp[13]);
    switch (res) {
    case 0: snprintf(b, sizeof(b), "phy enabled; unknown");
                 break;
    case 1: snprintf(b, sizeof(b), "phy disabled"); break;
    case 2: snprintf(b, sizeof(b), "phy enabled; speed negotiation failed");
                 break;
    case 3: snprintf(b, sizeof(b), "phy enabled; SATA spinup hold state");
                 break;
    case 4: snprintf(b, sizeof(b), "phy enabled; port selector");
                 break;
    case 8: snprintf(b, sizeof(b), "phy enabled; 1.5 Gbps"); break;
    case 9: snprintf(b, sizeof(b), "phy enabled; 3 Gbps"); break;
    case 0xa: snprintf(b, sizeof(b), "phy enabled; 6 Gbps"); break;
    default: snprintf(b, sizeof(b), "reserved [%d]", res); break;
    }
    printf("  negotiated physical link rate: %s\n", b);

    printf("  attached initiator: ssp=%d stp=%d smp=%d sata_host=%d\n",
           !! (smp_resp[14] & 8), !! (smp_resp[14] & 4),
           !! (smp_resp[14] & 2), (smp_resp[14] & 1));
    if (0 == do_brief)
        printf("  attached sata port selector: %d\n",
               !! (smp_resp[15] & 0x80));
    printf("  attached target: ssp=%d stp=%d smp=%d sata_device=%d\n",
           !! (smp_resp[15] & 8), !! (smp_resp[15] & 4),
           !! (smp_resp[15] & 2), (smp_resp[15] & 1));

    ull = 0;
    for (j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= smp_resp[16 + j];
    }
    printf("  SAS address: 0x%llx\n", ull);
    ull = 0;
    for (j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= smp_resp[24 + j];
    }
    printf("  attached SAS address: 0x%llx\n", ull);
    printf("  attached phy identifier: %d\n", smp_resp[32]);
    if (0 == do_brief) {
        printf("  attached break_reply capable: %d\n", smp_resp[33] & 1);
        printf("  programmed minimum physical link rate: %s\n",
               smp_get_plink_rate(((smp_resp[40] >> 4) & 0xf), 1,
                                  sizeof(b), b));
        printf("  hardware minimum physical link rate: %s\n",
               smp_get_plink_rate((smp_resp[40] & 0xf), 0, sizeof(b), b));
        printf("  programmed maximum physical link rate: %s\n",
               smp_get_plink_rate(((smp_resp[41] >> 4) & 0xf), 1,
                                  sizeof(b), b));
        printf("  hardware maximum physical link rate: %s\n",
               smp_get_plink_rate((smp_resp[41] & 0xf), 0,
                                  sizeof(b), b));
        printf("  phy change count: %d\n", smp_resp[42]);
        printf("  virtual phy: %d\n", !!(smp_resp[43] & 0x80));
        printf("  partial pathway timeout value: %d us\n",
               (smp_resp[43] & 0xf));
    }
    res = (0xf & smp_resp[44]);
    switch (res) {
    case 0: snprintf(b, sizeof(b), "direct"); break;
    case 1: snprintf(b, sizeof(b), "subtractive"); break;
    case 2: snprintf(b, sizeof(b), "table"); break;
    default: snprintf(b, sizeof(b), "reserved [%d]", res); break;
    }
    printf("  routing attribute: %s\n", b);
    if (do_brief)
        goto err_out;
    printf("  connector type: %s\n",
           find_sas_connector_type((smp_resp[45] & 0x7f), b, sizeof(b)));
    printf("  connector element index: %d\n", smp_resp[46]);
    printf("  connector physical link: %d\n", smp_resp[47]);
    if (len > 59) {
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= smp_resp[52 + j];
        }
        printf("  attached device name: 0x%llx\n", ull);
        printf("  zone address resolved: %d\n", !!(smp_resp[60] & 0x8));
        printf("  zone group persistent: %d\n", !!(smp_resp[60] & 0x4));
        printf("  zone participating: %d\n", !!(smp_resp[60] & 0x2));
        printf("  zone enabled: %d\n", !!(smp_resp[60] & 0x1));
        printf("  zone group: %d\n", smp_resp[63]);
    }

err_out:
    res = smp_initiator_close(&tobj);
    if (res < 0)
        return 1;
    return ret;
}
