/*
 * Copyright (c) 2006-2018, Douglas Gilbert
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
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"
#include "sg_unaligned.h"
#include "sg_pr2serr.h"

/* This is a Serial Attached SCSI (SAS) Serial Management Protocol (SMP)
 * utility.
 *
 * This utility issues a REPORT PHY SATA function and outputs its
 * response.
 */

static const char * version_str = "1.20 20180725";

#define SMP_FN_REPORT_PHY_SATA_RESP_LEN 72

static struct option long_options[] = {
    {"affiliation", required_argument, 0, 'a'},
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"interface", required_argument, 0, 'I'},
    {"phy", required_argument, 0, 'p'},
    {"raw", no_argument, 0, 'r'},
    {"sa", required_argument, 0, 's'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"zero", no_argument, 0, 'z'},
    {0, 0, 0, 0},
};


static void
usage(void)
{
    pr2serr("Usage: smp_rep_phy_sata [--affiliation=AC] [--help] [--hex]\n"
            "                        [--interface=PARAMS] [--phy=ID] "
            "[--raw]\n"
            "                        [--sa=SAS_ADDR] [--verbose] [--version] "
            "[--zero]\n"
            "                        SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --affiliation=AC|-a AC    relative identifier of affiliation "
            "context\n"
            "                              (def: 0)\n"
            "    --help|-h            print out usage message\n"
            "    --hex|-H             print response in hexadecimal\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --phy=ID|-p ID       phy identifier (def: 0)\n"
            "    --raw|-r             output response in binary\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending on\n"
            "                                 the interface, may not be "
            "needed\n"
            "    --verbose|-v         increase verbosity\n"
            "    --version|-V         print version string and exit\n"
            "    --zero|-z            zero Allocated Response Length "
            "field,\n"
            "                         may be required prior to SAS-2\n\n"
            "Performs a SMP REPORT PHY SATA function\n"
           );
}

static void
dStrRaw(const uint8_t * str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}


int
main(int argc, char * argv[])
{
    bool do_raw = false;
    bool do_zero = false;
    bool phy_id_given = false;
    int res, c, k, len, act_resplen;
    int aff_context = 0;
    int do_hex = 0;
    int phy_id = 0;
    int ret = 0;
    int subvalue = 0;
    int verbose = 0;
    int64_t sa_ll;
    uint64_t sa = 0;
    char * cp;
    char i_params[256];
    char device_name[512];
    char b[256];
    uint8_t smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_PHY_SATA, 0, 0,
                         0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t * smp_resp = NULL;
    uint8_t * free_smp_resp = NULL;
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;

    memset(device_name, 0, sizeof device_name);
    memset(i_params, 0, sizeof i_params);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "ahHI:p:rs:vVz", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
           aff_context = smp_get_num(optarg);
           if ((aff_context < 0) || (aff_context > 255)) {
                pr2serr("bad argument to '--affiliation'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
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
           if ((phy_id < 0) || (phy_id > 254)) {
                pr2serr("bad argument to '--phy', expect value from 0 to "
                        "254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            phy_id_given = true;
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
        case 'z':
            do_zero = true;
            break;
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

    /* Align SMP response buffer to a page boundary */
    smp_resp = smp_memalign(SMP_FN_REPORT_PHY_SATA_RESP_LEN, 0,
                            &free_smp_resp, false);
    if (NULL == smp_resp) {
        pr2serr("Unable to allocated %u bytes on the heap\n",
                SMP_FN_REPORT_PHY_SATA_RESP_LEN);
        ret = SMP_LIB_RESOURCE_ERROR;
        goto err_out;
    }
    if (! do_zero) {     /* SAS-2 or later */
        len = (SMP_FN_REPORT_PHY_SATA_RESP_LEN - 8) / 4;
        smp_req[2] = (len < 0x100) ? len : 0xff; /* Allocated Response Len */
        smp_req[3] = 2; /* Request Length: in dwords */
    }
    smp_req[9] = phy_id;
    smp_req[10] = aff_context;
    if (verbose) {
        pr2serr("    Report phy SATA request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            pr2serr("%02x ", smp_req[k]);
        pr2serr("\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = SMP_FN_REPORT_PHY_SATA_RESP_LEN;
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
            hex2stdout(smp_resp, len, 1);
        else
            dStrRaw(smp_resp, len);
        if (SMP_FRAME_TYPE_RESP != smp_resp[0])
            ret = SMP_LIB_CAT_MALFORMED;
        else if (smp_resp[1] != smp_req[1])
            ret = SMP_LIB_CAT_MALFORMED;
        else if (smp_resp[2]) {
            if (verbose)
                pr2serr("Report phy SATA result: %s\n",
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
        cp = smp_get_func_res_str(smp_resp[2], sizeof(b), b);
        pr2serr("Report phy SATA result%s: %s\n",
                (phy_id_given ? "" : " (for phy_id=0)"), cp);
        ret = smp_resp[2];
        goto err_out;
    }
    printf("Report phy SATA response:\n");
    res = sg_get_unaligned_be16(smp_resp + 4);
    if (verbose || (res > 0))
        printf("  expander change count: %d\n", res);
    printf("  phy identifier: %d\n", smp_resp[9]);
    printf("  STP I_T nexus loss occurred: %d\n", !!(smp_resp[11] & 0x4));
    printf("  affiliations supported: %d\n", !!(smp_resp[11] & 0x2));
    printf("  affiliation valid: %d\n", !!(smp_resp[11] & 0x1));
    printf("  STP SAS address: 0x%" PRIx64 "\n",
           sg_get_unaligned_be64(smp_resp + 16));
    printf("  register device to host FIS:\n    ");
    for (k = 0; k < 20; ++k)
        printf("%02x ", smp_resp[24 + k]);
    printf("\n");
    printf("  affiliated STP initiator SAS address: 0x%" PRIx64 "\n",
           sg_get_unaligned_be64(smp_resp + 48));
    if (len > 63)
        printf("  STP I_T nexus loss SAS address: 0x%" PRIx64 "\n",
               sg_get_unaligned_be64(smp_resp + 56));
    if (len > 67) {
        printf("  affiliation context: %d\n", smp_resp[65]);
        printf("  current affiliation contexts: %d\n", smp_resp[66]);
        printf("  maximum affiliation contexts: %d\n", smp_resp[67]);
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
