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
#include <ctype.h>
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
 * This utility issues a CONFIGURE PHY EVENT function and outputs
 * its response.
 */

static char * version_str = "1.00 20111222";

#define MAX_PHY_EV_SRC 126      /* max in one request */

struct pes_name_t {
    int pes;    /* phy event source, an 8 bit number */
    const char * pes_name;
};

static struct option long_options[] = {
    {"clear", 0, 0, 'C'},
    {"enumerate", 0, 0, 'e'},
    {"expected", 1, 0, 'E'},
    {"file", 1, 0, 'f'},
    {"help", 0, 0, 'h'},
    {"hex", 0, 0, 'H'},
    {"interface", 1, 0, 'I'},
    {"pes", 1, 0, 'P'},
    {"phy", 1, 0, 'p'},
    {"raw", 0, 0, 'r'},
    {"sa", 1, 0, 's'},
    {"thres", 1, 0, 'T'},
    {"verbose", 0, 0, 'v'},
    {"version", 0, 0, 'V'},
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
    fprintf(stderr, "Usage: "
          "smp_conf_phy_event [--clear] [--enumerate] [--expected=EX]\n"
          "                          [--file=FILE] [--help] [--hex]\n"
          "                          [--interface=PARAMS] "
          "[--pes=PES,PES...]\n"
          "                          [--phy=ID] [--raw] [--sa=SAS_ADDR]\n"
          "                          [--thres=THR,THR...] [--verbose] "
          "[--version]\n"
          "                          SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --clear|-C             clear all peak value detectors for "
          "this phy\n"
          "    --enumerate|-e         enumerate phy event source names, "
          "ignore\n"
          "                           SMP_DEVICE if given\n"
          "    --expected=EX|-E EX    set expected expander change "
          "count to EX\n"
          "    --file=FILE|-f FILE    read PES, THR pairs from FILE\n"
          "    --help|-h              print out usage message\n"
          "    --hex|-H               print response in hexadecimal\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --pes=PES,PES...|-P PES,PES...    comma separated list "
          "of Phy\n"
          "                                      Event Sources\n"
          "    --phy=ID|-p ID         phy identifier (def: 0)\n"
          "    --raw|-r               output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading\n"
          "                                 '0x' or trailing 'h'). "
          "Depending on\n"
          "                                 the interface, may not be "
          "needed\n"
          "    --thres=THR,THR...|-T THR,THR...    comma separated list "
          "of peak\n"
          "                                        value detector "
          "thresholds\n"
          "    --verbose|-v           increase verbosity\n"
          "    --version|-V           print version string and exit\n\n"
          "Performs a SMP CONFIGURE PHY EVENT function\n"
          );
}

/* Fetches unsigned int and returns it if found, with *err set to 0 if
 * err is non-NULL. If unable to decode returns 0, with *err set to 1 if
 * err is non-NULL. Assumes number is decimal unless prefixed by '0x' (or
 * '0X') or has a trailing 'h' (or 'H') in which case it is assumed to be
 * hexadecimal. */
unsigned int
get_unum(const char * buf, int * err)
{
    int res, len;
    unsigned unum;

    if ((NULL == buf) || ('\0' == buf[0])) {
        if (err)
            *err = 1;
        return 0;
    }
    len = strspn(buf, "0123456789aAbBcCdDeEfFhHxX");
    if (0 == len) {
        if (err)
            *err = 1;
        return 0;
    }
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1])))
        res = sscanf(buf + 2, "%x", &unum);
    else if ('H' == toupper(buf[len - 1]))
        res = sscanf(buf, "%x", &unum);
    else
        res = sscanf(buf, "%u", &unum);
    if (1 == res) {
        if (err)
            *err = 0;
        return unum;
    } else {
        if (err)
            *err = 1;
        return 0;
    }
}

/* Read numbers (up to 8 bits in size) from command line (comma (or
 * (single) space) separated list). Assumed decimal unless prefixed
 * by '0x', '0X' or contains trailing 'h' or 'H' (which indicate hex).
 * Returns 0 if ok, or 1 if error. */
static int
build_pes_arr(const char * inp, unsigned char * pes_arr,
              int * pes_arr_len, int max_arr_len)
{
    int in_len, k, err;
    const char * lcp;
    unsigned int unum;
    char * cp;
    char * c2p;

    if ((NULL == inp) || (NULL == pes_arr) || (NULL == pes_arr_len))
        return 1;
    lcp = inp;
    in_len = strlen(inp);
    if (0 == in_len)
        *pes_arr_len = 0;
    if ('-' == inp[0]) {        /* read from stdin */
        fprintf(stderr, "'--pes' cannot be read from stdin\n");
        return 1;
    } else {        /* list of numbers (default decimal) on command line */
        k = strspn(inp, "0123456789aAbBcCdDeEfFhHxX, ");
        if (in_len != k) {
            fprintf(stderr, "build_pes_arr: error at pos %d\n", k + 1);
            return 1;
        }
        for (k = 0; k < max_arr_len; ++k) {
            unum = get_unum(lcp, &err);
            if ((! err) && (unum < 256)) {
                pes_arr[k] = (unsigned char)unum;
                cp = strchr(lcp, ',');
                c2p = strchr(lcp, ' ');
                if (NULL == cp)
                    cp = c2p;
                if (NULL == cp)
                    break;
                if (c2p && (c2p < cp))
                    cp = c2p;
                lcp = cp + 1;
            } else {
                fprintf(stderr, "build_pes_arr: error at pos %d\n",
                        (int)(lcp - inp + 1));
                return 1;
            }
        }
        *pes_arr_len = k + 1;
        if (k == max_arr_len) {
            fprintf(stderr, "build_pes_arr: array length exceeded\n");
            return 1;
        }
    }
    return 0;
}

/* Read numbers (up to 32 bits in size) from command line (comma (or
 * (single) space) separated list). Assumed decimal unless prefixed
 * by '0x', '0X' or contains trailing 'h' or 'H' (which indicate hex).
 * Returns 0 if ok, or 1 if error. */
static int
build_thres_arr(const char * inp, unsigned int * thres_arr,
                int * thres_arr_len, int max_arr_len)
{
    int in_len, k, err;
    const char * lcp;
    unsigned int unum;
    char * cp;
    char * c2p;

    if ((NULL == inp) || (NULL == thres_arr) || (NULL == thres_arr_len))
        return 1;
    lcp = inp;
    in_len = strlen(inp);
    if (0 == in_len)
        *thres_arr_len = 0;
    if ('-' == inp[0]) {        /* read from stdin */
        fprintf(stderr, "'--thres' cannot be read from stdin\n");
        return 1;
    } else {        /* list of numbers (default decimal) on command line */
        k = strspn(inp, "0123456789aAbBcCdDeEfFhHxX, ");
        if (in_len != k) {
            fprintf(stderr, "build_thres_arr: error at pos %d\n", k + 1);
            return 1;
        }
        for (k = 0; k < max_arr_len; ++k) {
            unum = get_unum(lcp, &err);
            if (! err) {
                thres_arr[k] = unum;
                cp = strchr(lcp, ',');
                c2p = strchr(lcp, ' ');
                if (NULL == cp)
                    cp = c2p;
                if (NULL == cp)
                    break;
                if (c2p && (c2p < cp))
                    cp = c2p;
                lcp = cp + 1;
            } else {
                fprintf(stderr, "build_thres_arr: error at pos %d\n",
                        (int)(lcp - inp + 1));
                return 1;
            }
        }
        *thres_arr_len = k + 1;
        if (k == max_arr_len) {
            fprintf(stderr, "build_num_arr: array length exceeded\n");
            return 1;
        }
    }
    return 0;
}


/* Read numbers from filename (or stdin) line by line (comma (or
 * (single) space) separated list). Assumed decimal unless prefixed
 * by '0x', '0X' or contains trailing 'h' or 'H' (which indicate hex).
 * Returns 0 if ok, or 1 if error. */
static int
build_joint_arr(const char * file_name, unsigned char * pes_arr,
                unsigned int * thres_arr, int * arr_len, int max_arr_len)
{
    char line[512];
    int off = 0;
    int in_len, k, j, m, have_stdin, ind, bit0, err;
    char * lcp;
    FILE * fp;
    unsigned int unum;

    have_stdin = ((1 == strlen(file_name)) && ('-' == file_name[0]));
    if (have_stdin)
        fp = stdin;
    else {
        fp = fopen(file_name, "r");
        if (NULL == fp) {
            fprintf(stderr, "build_joint_arr: unable to open %s\n",
                    file_name);
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
        k = strspn(lcp, "0123456789aAbBcCdDeEfFhHxX ,\t");
        if ((k < in_len) && ('#' != lcp[k])) {
            fprintf(stderr, "build_joint_arr: syntax error at "
                    "line %d, pos %d\n", j + 1, m + k + 1);
            return 1;
        }
        for (k = 0; k < 1024; ++k) {
            unum = get_unum(lcp, &err);
            if (! err) {
                ind = ((off + k) >> 1);
                bit0 = 0x1 & (off + k);
                if (ind >= max_arr_len) {
                    fprintf(stderr, "build_joint_arr: array length "
                            "exceeded\n");
                    return 1;
                }
                if (bit0)
                    thres_arr[ind] = unum;
                else {
                    if (unum > 255) {
                        fprintf(stderr, "build_joint_arr: pes (%u) too "
                                "large\n", unum);
                        return 1;
                    }
                    pes_arr[ind] = (unsigned char)unum;
                }
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
                fprintf(stderr, "build_joint_arr: error in "
                        "line %d, at pos %d\n", j + 1,
                        (int)(lcp - line + 1));
                return 1;
            }
        }
        off += (k + 1);
    }
    if (0x1 & off) {
        fprintf(stderr, "build_joint_arr: expect LBA,NUM pairs but decoded "
                "odd number\n  from %s\n", have_stdin ? "stdin" : file_name);
        return 1;
    }
    *arr_len = off >> 1;
    return 0;
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
    int res, c, k, j, len, num_desc, act_resplen, pes_elem, thres_elem;
    int do_clear = 0;
    int do_enumerate = 0;
    int expected_cc = 0;
    int do_hex = 0;
    int phy_id = 0;
    int do_raw = 0;
    int verbose = 0;
    const char * file_op = NULL;
    const char * pes_op = NULL;
    const char * thres_op = NULL;
    long long sa_ll;
    unsigned long long sa = 0;
    char i_params[256];
    char device_name[512];
    char b[256];
    unsigned char pes_arr[MAX_PHY_EV_SRC];
    unsigned int thres_arr[MAX_PHY_EV_SRC];
    unsigned char smp_req[1028];
    unsigned char smp_resp[8];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;
    const struct pes_name_t * pnp;
    int subvalue = 0;
    char * cp;
    int ret = 0;

    memset(smp_req, 0, sizeof(smp_req));
    memset(pes_arr, 0, sizeof(pes_arr));
    memset(thres_arr, 0, sizeof(thres_arr));
    smp_req[0] = SMP_FRAME_TYPE_REQ;
    smp_req[1] = SMP_FN_CONFIG_PHY_EVENT;
    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "CeE:f:hHI:p:P:rs:S:T:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'C':
            ++do_clear;
            break;
        case 'e':
            ++do_enumerate;
            break;
        case 'E':
            expected_cc = smp_get_num(optarg);
            if ((expected_cc < 0) || (expected_cc > 65535)) {
                fprintf(stderr, "bad argument to '--expected'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'f':
            if (file_op) {
                fprintf(stderr, "only expected one '--file=' option\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            file_op = optarg;
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
                fprintf(stderr, "bad argument to '--phy'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'P':
            if (pes_op) {
                fprintf(stderr, "only expected one '--pes=' option\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            pes_op = optarg;
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
        case 'T':
            if (thres_op) {
                fprintf(stderr, "only expected one '--thres=' option\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            thres_op = optarg;
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
            fprintf(stderr, "missing device name on command line\n    [Could "
                    "use environment variable SMP_UTILS_DEVICE instead]\n");
            usage();
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if ((cp = strchr(device_name, SMP_SUBVALUE_SEPARATOR))) {
        *cp = '\0';
        if (1 != sscanf(cp + 1, "%d", &subvalue)) {
            fprintf(stderr, "expected number after seperator in SMP_DEVICE "
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

    if ((0 == do_clear) && (! file_op) && (! pes_op))
        fprintf(stderr, "warning: without --clear, --file and --pes not "
                "much will happen\n");
    if (file_op && pes_op) {
        fprintf(stderr, "can use either --file= or --pes= but not both\n");
        return SMP_LIB_SYNTAX_ERROR;
    }
    if (file_op && thres_op)
        fprintf(stderr, "warning: ignoring --thres= and taking threshold "
                "values from --file= argument\n");

    pes_elem = 0;
    thres_elem = 0;
    if (pes_op) {
        if (build_pes_arr(pes_op, pes_arr, &pes_elem, MAX_PHY_EV_SRC))
            return SMP_LIB_SYNTAX_ERROR;
        if (thres_op) {
            if (build_thres_arr(thres_op, thres_arr, &thres_elem,
                                MAX_PHY_EV_SRC))
                return SMP_LIB_SYNTAX_ERROR;
        }
        if (thres_elem > pes_elem)
            fprintf(stderr, "warning: more threshold elements (%d) than "
                    "phy event source elements (%d)\n", thres_elem, pes_elem);
    }
    if (file_op) {
        if (build_joint_arr(file_op, pes_arr, thres_arr, &pes_elem,
                            MAX_PHY_EV_SRC))
            return SMP_LIB_SYNTAX_ERROR;
    }

    num_desc = pes_elem;

    res = smp_initiator_open(device_name, subvalue, i_params, sa,
                             &tobj, verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    smp_req[3] = (num_desc * 2) + 2;
    smp_req[4] = (expected_cc >> 8) & 0xff;
    smp_req[5] = expected_cc & 0xff;
    smp_req[6] = do_clear ? 1 : 0;
    smp_req[9] = phy_id;
    smp_req[10] = 2;    /* descriptor 2 dwords long */
    smp_req[11] = num_desc;
    for (k = 0, j = 12; k < num_desc; ++k, j += 8) {
        smp_req[j + 3] = pes_arr[k];
        smp_req[j + 4] = (thres_arr[k] >> 24) & 0xff;
        smp_req[j + 5] = (thres_arr[k] >> 16) & 0xff;
        smp_req[j + 6] = (thres_arr[k] >> 8) & 0xff;
        smp_req[j + 7] = thres_arr[k] & 0xff;
    }
    if (verbose) {
        fprintf(stderr, "    Configure phy event request:");
        for (k = 0; k < (16 + (num_desc * 8)); ++k) {
            if (0 == (k % 16))
                fprintf(stderr, "\n      ");
            else if (0 == (k % 8))
                fprintf(stderr, " ");
            fprintf(stderr, "%02x ", smp_req[k]);
        }
        fprintf(stderr, "\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = 16 + (num_desc * 8);
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
                fprintf(stderr, "Configure zone phy information result: %s\n",
                        smp_get_func_res_str(smp_resp[2], sizeof(b), b));
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
        fprintf(stderr, "Configure zone phy information result: %s\n", cp);
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
