/*
 * Copyright (c) 2011 Douglas Gilbert.
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
 * This utility issues a REPORT PHY EVENT LIST function and outputs its
 * response.
 */

static char * version_str = "1.04 20110805";

#define SMP_FN_REPORT_PHY_EVENT_LIST_RESP_LEN (1020 + 4 + 4)


static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"index", 1, 0, 'i'},
        {"interface", 1, 0, 'I'},
        {"raw", 0, 0, 'r'},
        {"sa", 1, 0, 's'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

static void usage()
{
    fprintf(stderr, "Usage: "
          "smp_rep_phy_event_list [--help] [--hex] [--index=IN]\n"
          "                              [--interface=PARAMS] [--raw] "
          "[--sa=SAS_ADDR]\n"
          "                              [--verbose] [--version] "
          "SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --help|-h            print out usage message\n"
          "    --hex|-H             print response in hexadecimal\n"
          "    --index=IN|-i IN     starting phy event list descriptor "
          "index (def: 1)\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --raw|-r             output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading\n"
          "                         '0x' or trailing 'h'). Depending on "
          "the\n"
          "                         interface, may not be needed\n"
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n\n"
          "Performs a SMP REPORT PHY EVENT LIST function\n"
          );
}

static void dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

/* from sas2r15 */
static void
show_phy_event_info(int phy_id, int pes, unsigned int val,
                    unsigned int thresh_val)
{
    unsigned int u;

    printf("     phy_id=%d: ", phy_id);
    switch (pes) {
    case 0:
        printf("No event\n");
        break;
    case 0x1:
        printf("Invalid word count: %u\n", val);
        break;
    case 0x2:
        printf("Running disparity error count: %u\n", val);
        break;
    case 0x3:
        printf("Loss of dword synchronization count: %u\n", val);
        break;
    case 0x4:
        printf("Phy reset problem count: %u\n", val);
        break;
    case 0x5:
        printf("Elasticity buffer overflow count: %u\n", val);
        break;
    case 0x6:
        printf("Received ERROR  count: %u\n", val);
        break;
    case 0x20:
        printf("Received address frame error count: %u\n", val);
        break;
    case 0x21:
        printf("Transmitted abandon-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x22:
        printf("Received abandon-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x23:
        printf("Transmitted retry-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x24:
        printf("Received retry-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x25:
        printf("Received AIP (WATING ON PARTIAL) count: %u\n", val);
        break;
    case 0x26:
        printf("Received AIP (WAITING ON CONNECTION) count: %u\n", val);
        break;
    case 0x27:
        printf("Transmitted BREAK count: %u\n", val);
        break;
    case 0x28:
        printf("Received BREAK count: %u\n", val);
        break;
    case 0x29:
        printf("Break timeout count: %u\n", val);
        break;
    case 0x2a:
        printf("Connection count: %u\n", val);
        break;
    case 0x2b:
        printf("Peak transmitted pathway blocked count: %u\n",
               val & 0xff);
        printf("    Peak value detector threshold: %u\n",
               thresh_val & 0xff);
        break;
    case 0x2c:
        u = val & 0xffff;
        if (u < 0x8000)
            printf("Peak transmitted arbitration wait time (us): "
                   "%u\n", u);
        else
            printf("Peak transmitted arbitration wait time (ms): "
                   "%u\n", 33 + (u - 0x8000));
        u = thresh_val & 0xffff;
        if (u < 0x8000)
            printf("    Peak value detector threshold (us): %u\n",
                   u);
        else
            printf("    Peak value detector threshold (ms): %u\n",
                   33 + (u - 0x8000));
        break;
    case 0x2d:
        printf("Peak arbitration time (us): %u\n", val);
        printf("    Peak value detector threshold: %u\n", thresh_val);
        break;
    case 0x2e:
        printf("Peak connection time (us): %u\n", val);
        printf("    Peak value detector threshold: %u\n", thresh_val);
        break;
    case 0x40:
        printf("Transmitted SSP frame count: %u\n", val);
        break;
    case 0x41:
        printf("Received SSP frame count: %u\n", val);
        break;
    case 0x42:
        printf("Transmitted SSP frame error count: %u\n", val);
        break;
    case 0x43:
        printf("Received SSP frame error count: %u\n", val);
        break;
    case 0x44:
        printf("Transmitted CREDIT_BLOCKED count: %u\n", val);
        break;
    case 0x45:
        printf("Received CREDIT_BLOCKED count: %u\n", val);
        break;
    case 0x50:
        printf("Transmitted SATA frame count: %u\n", val);
        break;
    case 0x51:
        printf("Received SATA frame count: %u\n", val);
        break;
    case 0x52:
        printf("SATA flow control buffer overflow count: %u\n", val);
        break;
    case 0x60:
        printf("Transmitted SMP frame count: %u\n", val);
        break;
    case 0x61:
        printf("Received SMP frame count: %u\n", val);
        break;
    case 0x63:
        printf("Received SMP frame error count: %u\n", val);
        break;
    default:
        printf("Unknown phy event source: %d, val=%u, thresh_val=%u\n",
               pes, val, thresh_val);
        break;
    }
}


int main(int argc, char * argv[])
{
    int res, c, k, len, ped_len, num_ped, pes, phy_id, act_resplen;
    int do_hex = 0;
    int starting_index = 1;
    int do_raw = 0;
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ,
                               SMP_FN_REPORT_PHY_EVENT_LIST,
                               0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char smp_resp[SMP_FN_REPORT_PHY_EVENT_LIST_RESP_LEN];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    unsigned int first_di, pe_val, pvdt;
    char * cp;
    unsigned char * pedp;
    int ret = 0;

    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "hHi:I:rs:vV", long_options,
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
        case 'i':
           starting_index = smp_get_num(optarg);
           if ((starting_index < 0) || (starting_index > 65535)) {
                fprintf(stderr, "bad argument to '--index'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
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

    len = (sizeof(smp_resp) - 8) / 4;
    smp_req[2] = (len < 0x100) ? len : 0xff; /* Allocated Response Len */
    smp_req[6] = ((starting_index) >> 8) & 0xff;
    smp_req[7] = starting_index & 0xff;
    if (verbose) {
        fprintf(stderr, "    Report phy event list request: ");
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
        if (smp_resp[1] != smp_req[1])
            ret = SMP_LIB_CAT_MALFORMED;
        if (smp_resp[2]) {
            ret = smp_resp[2];
            if (verbose)
                fprintf(stderr, "Report phy event list result: %s\n",
                        smp_get_func_res_str(ret, sizeof(b), b));
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
        fprintf(stderr, "Report phy event list result: %s\n", cp);
        ret = smp_resp[2];
        goto err_out;
    }
    printf("Report phy event list response:\n");
    res = (smp_resp[4] << 8) + smp_resp[5];
    if (verbose || res)
        printf("  Expander change count: %d\n", res);
    first_di = (smp_resp[6] << 8) | smp_resp[7];
    printf("  first phy event list descriptor index: %u\n", first_di);
    printf("  last phy event list descriptor index: %u\n",
           (smp_resp[8] << 8) | smp_resp[9]);
    printf("  phy event descriptor length: %d dwords\n", smp_resp[10]);
    ped_len = smp_resp[10] * 4;
    num_ped = smp_resp[15];
    printf("  number of phy event descriptors: %d\n", num_ped);
    if (ped_len < 12) {
        fprintf(stderr, "Unexpectedly low descriptor length: %d bytes\n",
                ped_len);
        ret = -1;
        goto err_out;
    }
    pedp = smp_resp + 16;
    for (k = 0; k < num_ped; ++k, pedp += ped_len) {
        printf("   Descriptor index %u:\n", first_di + k);
        phy_id = pedp[2];
        pes = pedp[3];
        pe_val = (pedp[4] << 24) | (pedp[5] << 16) | (pedp[6] << 8) |
                 pedp[7];
        pvdt = (pedp[8] << 24) | (pedp[9] << 16) | (pedp[10] << 8) |
               pedp[11];
        show_phy_event_info(phy_id, pes, pe_val, pvdt);
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
