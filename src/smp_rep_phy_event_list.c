/*
 * Copyright (c) 2011-2018, Douglas Gilbert
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
 * This utility issues a REPORT PHY EVENT LIST function and outputs its
 * response.
 */

static const char * version_str = "1.14 20180725";

#define SMP_FN_REPORT_PHY_EVENT_LIST_RESP_LEN (1020 + 4 + 4)

#define DEF_STARTING_INDEX 1

struct pes_name_t {
    int pes;    /* phy event source, an 8 bit number */
    const char * pes_name;
};

static struct option long_options[] = {
    {"desc", no_argument, 0, 'd'},
    {"enumerate", no_argument, 0, 'e'},
    {"force", no_argument, 0, 'f'},
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"index", required_argument, 0, 'i'},
    {"interface", required_argument, 0, 'I'},
    {"long", no_argument, 0, 'l'},
    {"nonz", no_argument, 0, 'n'},
    {"raw", no_argument, 0, 'r'},
    {"sa", required_argument, 0, 's'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {0, 0, 0, 0},
};

static struct pes_name_t pes_name_arr[] = {
    {0x0, "No event"},
    /* Phy layer-based phy events (0x1 to 0x1F) */
    {0x1, "Invalid word count"},
    {0x2, "Running disparity error count"},
    {0x3, "Loss of dword synchronization count"},
    {0x4, "Phy reset problem count"},
    {0x5, "Elasticity buffer overflow count"},
    {0x6, "Received ERROR count"},
    {0x7, "Invalid SPL packet count"},
    {0x8, "Loss of SPL packet synchronization count"},
    /* SAS arbitration-based phy events (0x20 to 0x3F) */
    {0x20, "Received address frame error count"},
    {0x21, "Transmitted abandon-class OPEN_REJECT count"},
    {0x22, "Received abandon-class OPEN_REJECT count"},
    {0x23, "Transmitted retry-class OPEN_REJECT count"},
    {0x24, "Received retry-class OPEN_REJECT count"},
    {0x25, "Received AIP (WATING ON PARTIAL) count"},
    {0x26, "Received AIP (WAITING ON CONNECTION) count"},
    {0x27, "Transmitted BREAK count"},
    {0x28, "Received BREAK count"},
    {0x29, "Break timeout count"},
    {0x2a, "Connection count"},
    {0x2b, "Peak transmitted pathway blocked count"},   /*PVD */
    {0x2c, "Peak transmitted arbitration wait time"},   /*PVD */
    {0x2d, "Peak arbitration time"},                    /*PVD */
    {0x2e, "Peak connection time"},                     /*PVD */
    {0x2f, "Persistent connection count"},
    /* SSP related phy events (0x40 to 0x4F) */
    {0x40, "Transmitted SSP frame count"},
    {0x41, "Received SSP frame count"},
    {0x42, "Transmitted SSP frame error count"},
    {0x43, "Received SSP frame error count"},
    {0x44, "Transmitted CREDIT_BLOCKED count"},
    {0x45, "Received CREDIT_BLOCKED count"},
    /* SATA related phy events (0x50 to 0x5F) */
    {0x50, "Transmitted SATA frame count"},
    {0x51, "Received SATA frame count"},
    {0x52, "SATA flow control buffer overflow count"},
    /* SMP related phy events (0x60 to 0x6F) */
    {0x60, "Transmitted SMP frame count"},
    {0x61, "Received SMP frame count"},
    {0x63, "Received SMP frame error count"},
    /* Reserved 0x70 to 0xCF) */
    /* Vendor specific 0xD0 to 0xFF) */

    {-1, NULL}
};


static void
usage(void)
{
    pr2serr("Usage: smp_rep_phy_event_list [--desc] [--enumerate] [--force] "
            "[--help]\n"
            "                              [--hex] [--index=IN] "
            "[--interface=PARAMS]\n"
            "                              [--long] [--nonz] [--raw] "
            "[--sa=SAS_ADDR]\n"
            "                              [--verbose] [--version] "
            "SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --desc|-d            show descriptor number in output\n"
            "    --enumerate|-e       enumerate phy event source names, "
            "ignore\n"
            "                         SMP_DEVICE if given\n"
            "    --force|-f           force to continue past last descriptor "
            "index\n"
            "    --help|-h            print out usage message\n"
            "    --hex|-H             print response in hexadecimal\n"
            "    --index=IN|-i IN     starting phy event list descriptor "
            "index (def: 1)\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --long|-l            show phy event source hex value in "
            "output\n"
            "    --nonz|-n            only show phy events with non-zero "
            "counts\n"
            "    --raw|-r             output response in binary\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending on\n"
            "                                 the interface, may not be "
            "needed\n"
            "    --verbose|-v         increase verbosity\n"
            "    --version|-V         print version string and exit\n\n"
            "Performs a SMP REPORT PHY EVENT LIST function\n"
           );
}

static void
dStrRaw(const uint8_t * str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

static const char *
get_pes_name(int pes, char * b, int blen)
{
    int len;
    const char * res = NULL;
    const struct pes_name_t * pnp;

    if ((NULL == b) || (blen < 1))
        return res;
    for (pnp = pes_name_arr; pnp->pes_name; ++pnp) {
        if (pes == pnp->pes) {
            len = strlen(pnp->pes_name);
            if (len > (blen - 1))
                len = blen - 1;
            memcpy(b, pnp->pes_name, len);
            b[len] = '\0';
            return b;
        }
    }
    return res;
}

/* from sas2r15 */
static void
show_phy_event_info(int phy_id, int prev_pid, int pes, unsigned int val,
                    unsigned int thresh_val, bool do_long)
{
    unsigned int u;
    char b[80];
    const char * cp;

    if (do_long)
        printf("    phy_id=%d: [0x%x] ", phy_id, (unsigned int)pes);
    else {
        snprintf(b, sizeof(b) - 1, "    %d: ", phy_id);
        b[sizeof(b) - 1] = '\0';
        if (prev_pid == phy_id)
            memset(b, ' ', strlen(b));
        printf("%s", b);
    }

    switch (pes) {
    case 0:
        printf("No event\n");
        break;
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2a:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x60:
    case 0x61:
    case 0x63:
        if ((cp = get_pes_name(pes, b, sizeof(b))))
            printf("%s: %u\n", cp, val);
        else
            printf("Unknown Phy Event Source [0x%x]: %u\n", pes, val);
        break;


    case 0x2b:
        printf("%s: %u\n", get_pes_name(pes, b, sizeof(b)), val & 0xff);
        printf("      Peak value detector threshold: %u\n",
               thresh_val & 0xff);
        break;
    case 0x2c:
        cp = get_pes_name(pes, b, sizeof(b));
        u = val & 0xffff;
        if (u < 0x8000)
            printf("%s (us): %u\n", cp, u);
        else
            printf("%s (ms): %u\n", cp, 33 + (u - 0x8000));
        u = thresh_val & 0xffff;
        if (u < 0x8000)
            printf("      Peak value detector threshold (us): %u\n",
                   u);
        else
            printf("      Peak value detector threshold (ms): %u\n",
                   33 + (u - 0x8000));
        break;
    case 0x2d:
        printf("%s (us): %u\n", get_pes_name(pes, b, sizeof(b)), val);
        printf("      Peak value detector threshold: %u\n", thresh_val);
        break;
    case 0x2e:
        printf("%s (us): %u\n", get_pes_name(pes, b, sizeof(b)), val);
        printf("      Peak value detector threshold: %u\n", thresh_val);
        break;
    default:
        printf("Unknown phy event source: %d, val=%u, thresh_val=%u\n",
               pes, val, thresh_val);
        break;
    }
}


int
main(int argc, char * argv[])
{
    bool do_desc = false;
    bool do_enumerate = false;
    bool do_force = false;
    bool do_long = false;
    bool do_nonz = false;
    bool do_raw = false;
    int res, c, k, len, ped_len, num_ped, pes, phy_id, prev_pid, act_resplen;
    int do_hex = 0;
    int ret = 0;
    int starting_index = DEF_STARTING_INDEX;
    int subvalue = 0;
    int verbose = 0;
    unsigned int first_di, last_di, pe_val, pvdt;
    int64_t sa_ll;
    uint64_t sa = 0;
    char * cp;
    uint8_t * pedp;
    const struct pes_name_t * pnp;
    char i_params[256];
    char device_name[512];
    char b[256];
    uint8_t smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_PHY_EVENT_LIST,
                         0, 1,  0, 0, 0, 0,  0, 0, 0, 0};
    uint8_t * smp_resp = NULL;
    uint8_t * free_smp_resp = NULL;
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;

    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "defhHi:I:lnrs:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'd':
            do_desc = true;
            break;
        case 'e':
            do_enumerate = true;
            break;
        case 'f':
            do_force = true;
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++do_hex;
            break;
        case 'i':
           starting_index = smp_get_num(optarg);
           if ((starting_index < 0) || (starting_index > 65535)) {
                pr2serr("bad argument to '--index'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'l':
            do_long = true;
            break;
        case 'n':
            do_nonz = true;
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

    if (do_enumerate) {
        printf("Phy Event Source names (preceded by hex value):\n");
        for (pnp = pes_name_arr; pnp->pes_name; ++pnp)
            printf("    [0x%02x] %s\n", pnp->pes, pnp->pes_name);
        return 0;
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
            pr2serr("expected number after separator in SMP_DEVICE "
                    "name\n");
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
    smp_resp = smp_memalign(SMP_FN_REPORT_PHY_EVENT_LIST_RESP_LEN, 0,
                            &free_smp_resp, false);
    if (NULL == smp_resp) {
        pr2serr("Unable to allocated %u bytes on the heap\n",
                SMP_FN_REPORT_PHY_EVENT_LIST_RESP_LEN);
        ret = SMP_LIB_RESOURCE_ERROR;
        goto err_out;
    }
    len = (SMP_FN_REPORT_PHY_EVENT_LIST_RESP_LEN - 8) / 4;
    smp_req[2] = (len < 0x100) ? len : 0xff; /* Allocated Response Len */
    sg_put_unaligned_be16(starting_index, smp_req + 6);
    if (verbose) {
        pr2serr("    Report phy event list request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            pr2serr("%02x ", smp_req[k]);
        pr2serr("\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = SMP_FN_REPORT_PHY_EVENT_LIST_RESP_LEN;
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
        if (smp_resp[1] != smp_req[1])
            ret = SMP_LIB_CAT_MALFORMED;
        if (smp_resp[2]) {
            ret = smp_resp[2];
            if (verbose)
                pr2serr("Report phy event list result: %s\n",
                        smp_get_func_res_str(ret, sizeof(b), b));
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
        pr2serr("Report phy event list result: %s\n", cp);
        ret = smp_resp[2];
        goto err_out;
    }
    printf("Report phy event list response:\n");
    res = sg_get_unaligned_be16(smp_resp + 4);
    if (verbose || res)
        printf("  Expander change count: %d\n", res);
    first_di = sg_get_unaligned_be16(smp_resp + 6);
    last_di = sg_get_unaligned_be16(smp_resp + 8);
    printf("  first phy event list descriptor index: %u\n", first_di);
    printf("  last phy event list descriptor index: %u\n", last_di);
    printf("  phy event descriptor length: %d dwords\n", smp_resp[10]);
    ped_len = smp_resp[10] * 4;
    num_ped = smp_resp[15];
    printf("  number of phy event descriptors: %d\n", num_ped);
    if (ped_len < 12) {
        pr2serr("Unexpectedly low descriptor length: %d bytes\n", ped_len);
        ret = -1;
        goto err_out;
    }
    pedp = smp_resp + 16;
    for (k = 0, prev_pid = -1; k < num_ped;
         ++k, pedp += ped_len, prev_pid = phy_id) {
        if ((! do_force) && ((first_di + k) > last_di)) {
            if (do_long)
                printf("last descriptor index exceeded, exiting\n");
            break;
        }
        phy_id = pedp[2];
        pes = pedp[3];
        pe_val = sg_get_unaligned_be32(pedp + 4);
        pvdt = sg_get_unaligned_be32(pedp + 8);
        if ((! do_nonz) || pe_val) {
            if (do_desc)
                printf("   Descriptor index %u:\n", first_di + k);
            show_phy_event_info(phy_id, prev_pid, pes, pe_val, pvdt, do_long);
        }
    }
    if ((k >= num_ped) && ((first_di + k) < last_di))
        printf("Start next invocation at '--index=%u'\n", first_di + k);

err_out:
    if (free_smp_resp)
        free(free_smp_resp);
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
