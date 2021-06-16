/*
 * Copyright (c) 2011-2021, Douglas Gilbert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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
#include "sg_unaligned.h"
#include "sg_pr2serr.h"

/* This is a Serial Attached SCSI (SAS) Serial Management Protocol (SMP)
 * utility.
 *
 * This utility issues a REPORT SELF-CONFIGURATION STATUS function and
 * outputs its response.
 */

static const char * version_str = "1.09 20216155";

#define SMP_FN_REPORT_SELF_CONFIG_RESP_LEN (1020 + 4 + 4)

static struct option long_options[] = {
    {"brief", no_argument, 0, 'b'},
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"index", required_argument, 0, 'i'},
    {"interface", required_argument, 0, 'I'},
    {"last", no_argument, 0, 'l'},
    {"one", no_argument, 0, 'o'},
    {"raw", no_argument, 0, 'r'},
    {"sa", required_argument, 0, 's'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {0, 0, 0, 0},
};


static void
usage(void)
{
    pr2serr("Usage: smp_rep_self_conf_stat [--brief] [--help] [--hex] "
            "[--index=SDI]\n"
            "                              [--interface=PARAMS] [--last] "
            "[--one] [--raw]\n"
            "                              [--sa=SAS_ADDR] [--verbose] "
            "[--version]\n"
            "                              SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --brief|-b              lessen the amount output\n"
            "    --help|-h               print out usage message\n"
            "    --hex|-H                print response in hexadecimal\n"
            "    --index=SDI|-i SDI      SDI is starting self-configuration "
            "status\n"
            "                            descriptor index (def: 1)\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --last|-l               output descriptors starting at last "
            "recorded\n"
            "    --one|-o                only output first descriptor\n"
            "    --raw|-r                output response in binary\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending\n"
            "                                 on the interface, may not be "
            "needed\n"
            "    --verbose|-v            increase verbosity\n"
            "    --version|-V            print version string and exit\n\n"
            "Performs a SMP REPORT SELF-CONFIGURATION STATUS function\n"
           );
}

static void
dStrRaw(const uint8_t * str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

static char *
find_status_description(int status, char * buff, int buff_len)
{
    const char * cp = NULL;

    switch (status) {
    case 0x0: cp = "reserved"; break;
    case 0x1: cp = "error not related to a specific layer"; break;
    case 0x2: cp = "trying to connect to SMP target {SA}"; break;
    case 0x3: cp = "route table full, unable to add {SA}"; break;
    case 0x4: cp = "expander out of resources"; break;
    case 0x20: cp = "error reported by phy layer"; break;
    case 0x21: cp = "all phys including {PI} lost dword sync"; break;
    case 0x40: cp = "error reported by link layer"; break;
    case 0x41: cp = "open timeout timer expired"; break;
    case 0x42: cp = "received an abandon-class open-reject"; break;
    case 0x43: cp = "vendor specific number of retry-class"; break;
    case 0x44: cp = "I_T nexus loss occurred"; break;
    case 0x45: cp = "connection request, received break"; break;
    case 0x46: cp = "SMP response frame CRC error"; break;
    case 0x60: cp = "error reported by port layer"; break;
    case 0x61: cp = "SMP response frame timeout"; break;
    case 0x80: cp = "error reported by SMP transport layer"; break;
    case 0xa0: cp = "error reported by management app layer"; break;
    case 0xa1: cp = "SMP response frame is too short"; break;
    case 0xa2: cp = "SMP response contains invalid fields"; break;
    case 0xa3: cp = "SMP response contains inconsistent fields"; break;
    case 0xa4: cp = "{SA} has configuring bit set"; break;
    case 0xa5: cp = "{SA} has self configuring bit set"; break;
    case 0xa6: cp = "{SA} has zone configuring bit set"; break;
    default:
        if (status < 0x20)
            cp = "reserved for status not related to specific layer";
        else if (status < 0x40)
            cp = "reserved for status reported by phy layer";
        else if (status < 0x60)
            cp = "reserved for status reported by link layer";
        else if (status < 0x80)
            cp = "reserved for status reported by port layer";
        else if (status < 0xa0)
            cp = "reserved for status reported by SMP transport layer";
        else if (status < 0xc0)
            cp = "reserved for status reported by management app layer";
        else if (status < 0xe0)
            cp = "reserved";
        else
            cp = "vendor specific";
    }
    if (buff) {
        if (cp && (buff_len > 0)) {
            strncpy(buff, cp, buff_len - 1);
            buff[buff_len - 1] = '\0';
        } else if (buff_len > 0)
            buff[0] = '\0';
    }
    return buff;
}


int
main(int argc, char * argv[])
{
    bool do_brief = false;
    bool do_last = false;
    bool do_one = false;
    bool do_raw = false;
    int res, c, k, j, len, sscsd_ind, last_scsd_ind, scsd_len, num_scsd;
    int tot_num_scsd, ind, status, act_resplen;
    int do_hex = 0;
    int index = 1;
    int ret = 0;
    int subvalue = 0;
    int verbose = 0;
    int64_t sa_ll;
    uint64_t sa = 0;
    char * cp;
    const char * last_recp;
    uint8_t * scsdp;
    char b[256];
    char device_name[512];
    char i_params[256];
    uint8_t smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_SELF_CONFIG, 0, 1,
                         0, 0, 0, 0,  0, 0, 0, 0};
    uint8_t smp_resp[SMP_FN_REPORT_SELF_CONFIG_RESP_LEN];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;

    memset(device_name, 0, sizeof device_name);
    memset(i_params, 0, sizeof i_params);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "bhHi:I:lors:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'b':
            do_brief = true;
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++do_hex;
            break;
        case 'i':
            index = smp_get_dhnum(optarg);
            if ((index < 0) || (index > 65535)) {
                pr2serr("bad argument to '--index', expect value from 0 to "
                        "65535\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'l':
            do_last = true;
            break;
        case 'o':
            do_one = true;
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
        cp = getenv("SMP_UTILS_DEVICE");
        if (cp)
            strncpy(device_name, cp, sizeof(device_name) - 1);
        else {
            pr2serr("missing device name on command line\n    [Could use "
                    "environment variable SMP_UTILS_DEVICE instead]\n\n");
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

last_again:
    len = (sizeof(smp_resp) - 8) / 4;
    smp_req[2] = (len < 0x100) ? len : 0xff; /* Allocated Response Len */
    sg_put_unaligned_be16(index, smp_req + 6);
    if (verbose) {
        pr2serr("    Report self-configuration status request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            pr2serr("%02x ", smp_req[k]);
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
        pr2serr("response too short, len=%d\n", smp_rr.act_response_len);
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
            hex2stdout(smp_resp, len, 1);
        else
            dStrRaw(smp_resp, len);
        if (SMP_FRAME_TYPE_RESP != smp_resp[0])
            ret = SMP_LIB_CAT_MALFORMED;
        if (smp_resp[1] != smp_req[1])
            ret = SMP_LIB_CAT_MALFORMED;
        if (smp_resp[2]) {
            ret = smp_resp[2];
            if (verbose)
                pr2serr("Report self-configuration status result: %s\n",
                        smp_get_func_res_str(ret, sizeof(b), b));
        }
        if (! (do_last && (0 == ret)))
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
        cp = smp_get_func_res_str(smp_resp[2], sizeof(b), b);
        pr2serr("Report self-configuration status result: %s\n", cp);
        ret = smp_resp[2];
        goto err_out;
    }

    last_scsd_ind = sg_get_unaligned_be16(smp_resp + 10);
    if (do_last && (last_scsd_ind > 0) && (index != last_scsd_ind)) {
        do_last = false;
        memset(smp_req, 0, sizeof(smp_req));
        smp_req[0] = SMP_FRAME_TYPE_REQ;
        smp_req[1] = SMP_FN_REPORT_SELF_CONFIG;
        smp_req[3] = 1;
        index = last_scsd_ind;
        goto last_again;
    }
    printf("Report self-configuration status response:\n");
    res = sg_get_unaligned_be16(smp_resp + 4);
    if (verbose || res)
        printf("  Expander change count: %d\n", res);
    sscsd_ind = sg_get_unaligned_be16(smp_resp + 6);
    if (! do_brief)
        printf("  starting self-configuration status descriptor index: %d\n",
               sscsd_ind);
    tot_num_scsd = sg_get_unaligned_be16(smp_resp + 8);
    printf("  total number of self-configuration status descriptors: %d\n",
           tot_num_scsd);
    if (! do_brief) {
        printf("  last self-configuration status descriptor index: %d\n",
               last_scsd_ind);
        printf("  self-configuration status descriptor length: %d dwords\n",
               smp_resp[12]);
    }
    if (16 == smp_resp[12]) {
        if (! do_brief)
            printf("      <<assume that value is not dwords but bytes>>\n");
        scsd_len = smp_resp[12];
    } else
        scsd_len = smp_resp[12] * 4;
    num_scsd = smp_resp[19];
    printf("  number of self-configuration status descriptors: %d\n",
           num_scsd);
    if (scsd_len < 16) {
        pr2serr("Unexpectedly low descriptor length: %d bytes\n", scsd_len);
        ret = -1;
        goto err_out;
    }
    scsdp = smp_resp + 20;
    for (k = 0, ind = sscsd_ind; k < num_scsd; ++k, scsdp += scsd_len) {
        status = scsdp[0];
        last_recp = (ind == last_scsd_ind) ? ">>> " : "";
        if (do_brief)
            printf("    %s%d [%d]: status=0x%x flag=%d pi=%d sa=0x",
                   last_recp, k + 1, ind, status, scsdp[1] & 1, scsdp[3]);
        else {
            printf("   Descriptor %d [%sindex=%d]:\n", k + 1, last_recp, ind);
            printf("     status: %s [0x%x]\n",
                   find_status_description(status, b, sizeof(b)), status);
            printf("     final: %d\n", scsdp[1] & 1);
            printf("     phy id: %d\n", scsdp[3]);
            printf("     sas address: 0x");
        }
        for (j = 0; j < 8; ++j)
            printf("%02x", scsdp[8 + j]);
        printf("\n");
        if (verbose > 1) {
            printf("     in hex: ");
            for (j = 0; j < scsd_len; ++j)
                printf("%02x ", scsdp[j]);
            printf("\n");
        }
        if (ind > 0xfffe)
            ind = 1;
        else
            ++ind;
        if (do_one)
            break;
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
