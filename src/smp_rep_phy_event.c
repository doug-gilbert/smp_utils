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

/* This is a Serial Attached SCSI (SAS) Serial Management Protocol (SMP)
 * utility.
 *
 * This utility issues a REPORT PHY EVENT function and outputs its
 * response.
 */

static char * version_str = "1.05 20110830";

#define SMP_FN_REPORT_PHY_EVENT_RESP_LEN (1020 + 4 + 4)

/* Comment out following line to disable test option */
#define SMP_TEST 1


static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"interface", 1, 0, 'I'},
        {"phy", 1, 0, 'p'},
        {"raw", 0, 0, 'r'},
        {"sa", 1, 0, 's'},
#ifdef SMP_TEST
        {"test", 0, 0, 't'},
#endif
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

#ifdef SMP_TEST
static unsigned char test_resp[] = {0x41, 0x14, 0x0, 15,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 4, /* rest of header */
                                              /* vvv counts vvv */
    0, 0, 0, 0x1, 0, 0, 0, 0x33, 0, 0, 0, 0,  /* invalid dword */
    0, 0, 0, 0x2, 0, 0, 0, 0x44, 0, 0, 0, 0,  /* running disparity error */
    0, 0, 0, 0x3, 0, 0, 0, 0x55, 0, 0, 0, 0,  /* loss of dword sync */
    0, 0, 0, 0x4, 0, 0, 0, 0x66, 0, 0, 0, 0,  /* phy reset problem */
    0, 0, 0, 0  /* CRC */ };
#endif


static void usage()
{
    fprintf(stderr, "Usage: "
          "smp_rep_phy_event [--help] [--hex] [--interface=PARAMS] "
          "[--phy=ID]\n"
#ifdef SMP_TEST
          "                         [--raw] [--sa=SAS_ADDR] [--test] "
          "[--verbose]\n"
          "                         [--version] SMP_DEVICE[,N]\n"
#else
          "                         [--raw] [--sa=SAS_ADDR] [--verbose] "
          "[--version]\n"
          "                         SMP_DEVICE[,N]\n"
#endif
          "  where:\n"
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
#ifdef SMP_TEST
          "    --test|-t            use dummy hard-coded response, ignore "
          "DEVICE\n"
#endif
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n\n"
          "Performs a SMP REPORT PHY EVENT function\n"
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
show_phy_event_info(int pes, unsigned int val, unsigned int thresh_val)
{
    unsigned int u;

    switch (pes) {
    case 0:
        printf("     No event\n");
        break;
    case 0x1:
        printf("     Invalid word count: %u\n", val);
        break;
    case 0x2:
        printf("     Running disparity error count: %u\n", val);
        break;
    case 0x3:
        printf("     Loss of dword synchronization count: %u\n", val);
        break;
    case 0x4:
        printf("     Phy reset problem count: %u\n", val);
        break;
    case 0x5:
        printf("     Elasticity buffer overflow count: %u\n", val);
        break;
    case 0x6:
        printf("     Received ERROR  count: %u\n", val);
        break;
    case 0x20:
        printf("     Received address frame error count: %u\n", val);
        break;
    case 0x21:
        printf("     Transmitted abandon-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x22:
        printf("     Received abandon-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x23:
        printf("     Transmitted retry-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x24:
        printf("     Received retry-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x25:
        printf("     Received AIP (WATING ON PARTIAL) count: %u\n", val);
        break;
    case 0x26:
        printf("     Received AIP (WAITING ON CONNECTION) count: %u\n", val);
        break;
    case 0x27:
        printf("     Transmitted BREAK count: %u\n", val);
        break;
    case 0x28:
        printf("     Received BREAK count: %u\n", val);
        break;
    case 0x29:
        printf("     Break timeout count: %u\n", val);
        break;
    case 0x2a:
        printf("     Connection count: %u\n", val);
        break;
    case 0x2b:
        printf("     Peak transmitted pathway blocked count: %u\n",
               val & 0xff);
        printf("         Peak value detector threshold: %u\n",
               thresh_val & 0xff);
        break;
    case 0x2c:
        u = val & 0xffff;
        if (u < 0x8000)
            printf("     Peak transmitted arbitration wait time (us): "
                   "%u\n", u);
        else
            printf("     Peak transmitted arbitration wait time (ms): "
                   "%u\n", 33 + (u - 0x8000));
        u = thresh_val & 0xffff;
        if (u < 0x8000)
            printf("         Peak value detector threshold (us): %u\n",
                   u);
        else
            printf("         Peak value detector threshold (ms): %u\n",
                   33 + (u - 0x8000));
        break;
    case 0x2d:
        printf("     Peak arbitration time (us): %u\n", val);
        printf("         Peak value detector threshold: %u\n", thresh_val);
        break;
    case 0x2e:
        printf("     Peak connection time (us): %u\n", val);
        printf("         Peak value detector threshold: %u\n", thresh_val);
        break;
    case 0x40:
        printf("     Transmitted SSP frame count: %u\n", val);
        break;
    case 0x41:
        printf("     Received SSP frame count: %u\n", val);
        break;
    case 0x42:
        printf("     Transmitted SSP frame error count: %u\n", val);
        break;
    case 0x43:
        printf("     Received SSP frame error count: %u\n", val);
        break;
    case 0x44:
        printf("     Transmitted CREDIT_BLOCKED count: %u\n", val);
        break;
    case 0x45:
        printf("     Received CREDIT_BLOCKED count: %u\n", val);
        break;
    case 0x50:
        printf("     Transmitted SATA frame count: %u\n", val);
        break;
    case 0x51:
        printf("     Received SATA frame count: %u\n", val);
        break;
    case 0x52:
        printf("     SATA flow control buffer overflow count: %u\n", val);
        break;
    case 0x60:
        printf("     Transmitted SMP frame count: %u\n", val);
        break;
    case 0x61:
        printf("     Received SMP frame count: %u\n", val);
        break;
    case 0x63:
        printf("     Received SMP frame error count: %u\n", val);
        break;
    default:
        printf("     Unknown phy event source: %d, val=%u, thresh_val=%u\n",
               pes, val, thresh_val);
        break;
    }
}


int main(int argc, char * argv[])
{
    int res, c, k, len, ped_len, num_ped, pes, act_resplen;
    int do_hex = 0;
    int phy_id = 0;
    int phy_id_given = 0;
    int do_raw = 0;
#ifdef SMP_TEST
    int do_test = 0;
#endif
    int verbose = 0;
    long long sa_ll;
    unsigned long long sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_REPORT_PHY_EVENT,
                               0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char smp_resp[SMP_FN_REPORT_PHY_EVENT_RESP_LEN];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    int subvalue = 0;
    unsigned int pe_val, pvdt;
    char * cp;
    unsigned char * pedp;
    int ret = 0;

    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

#ifdef SMP_TEST
        c = getopt_long(argc, argv, "hHI:p:rs:tvV", long_options,
                        &option_index);
#else
        c = getopt_long(argc, argv, "hHI:p:rs:vV", long_options,
                        &option_index);
#endif
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
        case 'p':
           phy_id = smp_get_num(optarg);
           if ((phy_id < 0) || (phy_id > 254)) {
                fprintf(stderr, "bad argument to '--phy', expect "
                        "value from 0 to 254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            ++phy_id_given;
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
#ifdef SMP_TEST
        case 't':
            ++do_test;
            break;
#endif
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

#ifdef SMP_TEST
    if (0 == do_test) {
#endif
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
#ifdef SMP_TEST
    }
#endif

    len = (sizeof(smp_resp) - 8) / 4;
    smp_req[2] = (len < 0x100) ? len : 0xff; /* Allocated Response Len */
    smp_req[9] = phy_id;
    if (verbose) {
        fprintf(stderr, "    Report phy event request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            fprintf(stderr, "%02x ", smp_req[k]);
        fprintf(stderr, "\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = sizeof(smp_resp);
    smp_rr.response = smp_resp;
#ifdef SMP_TEST
    if (do_test) {
        res = 0;
        smp_rr.act_response_len = sizeof(test_resp);
        memcpy(smp_resp, test_resp, sizeof(test_resp));
        smp_resp[9] = phy_id;
    } else
#endif
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
                fprintf(stderr, "Report phy event result: %s\n",
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
        fprintf(stderr, "Report phy event result%s: %s\n",
                (phy_id_given ? "" : " (for phy_id=0)"), cp);
        ret = smp_resp[2];
        goto err_out;
    }
    printf("Report phy event response:\n");
    res = (smp_resp[4] << 8) + smp_resp[5];
    if (verbose || res)
        printf("  Expander change count: %d\n", res);
    printf("  phy identifier: %d\n", smp_resp[9]);
    printf("  phy event descriptor length: %d dwords\n", smp_resp[14]);
    ped_len = smp_resp[14] * 4;
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
        printf("   Descriptor %d:\n", k + 1);
        pes = pedp[3];
        pe_val = (pedp[4] << 24) | (pedp[5] << 16) | (pedp[6] << 8) |
                 pedp[7];
        pvdt = (pedp[8] << 24) | (pedp[9] << 16) | (pedp[10] << 8) |
               pedp[11];
        show_phy_event_info(pes, pe_val, pvdt);
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
