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
#include <ctype.h>
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
 * This utility issues a PHY CONTROL function and outputs its response.
 */

static const char * version_str = "1.25 20180725";

static struct option long_options[] = {
    {"attached", required_argument, 0, 'a'},
    {"expected", required_argument, 0, 'E'},
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"interface", required_argument, 0, 'I'},
    {"min", required_argument, 0, 'm'},
    {"max", required_argument, 0, 'M'},
    {"op", required_argument, 0, 'o'},
    {"phy", required_argument, 0, 'p'},
    {"pptv", required_argument, 0, 'P'},
    {"pwrdis", required_argument, 0, 'D'},
    {"raw", no_argument, 0, 'r'},
    {"sa", required_argument, 0, 's'},
    {"sas-pa", required_argument, 0, 'q'},
    {"sas_pa", required_argument, 0, 'q'},
    {"sas-sl", required_argument, 0, 'l'},
    {"sas_sl", required_argument, 0, 'l'},
    {"sata-pa", required_argument, 0, 'Q'},
    {"sata_pa", required_argument, 0, 'Q'},
    {"sata-sl", required_argument, 0, 'L'},
    {"sata_sl", required_argument, 0, 'L'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {0, 0, 0, 0},
};


static void
usage(void)
{
    pr2serr("Usage: smp_phy_control [--attached=ADN] [--expected=EX] "
            "[--help] [--hex]\n"
            "                       [--interface=PARAMS] [--max=MA] "
            "[--min=MI] [--op=OP]\n"
            "                       [--phy=ID] [--pptv=TI] [--pwrdis=PDC] "
            "[--raw]\n"
            "                       [--sa=SAS_ADDR] [--sas_pa=CO] "
            "[--sas_sl=CO]\n"
            "                       [--sata_pa=CO] [--sata_sl=CO] "
            "[--version]\n"
            "                       [--verbose] SMP_DEVICE[,N]\n"
            "  where:\n"
            "    --attached=ADN|-a ADN    attached device name [a decimal "
            "number,\n"
            "                             use '0x' prefix for hex], (def: "
            "0)\n"
            "    --expected=EX|-E EX      set expected expander change count "
            "to EX\n"
            "                             (def: 0 (implies ignore))\n"
            "    --help|-h                print out usage message\n"
            "    --hex|-H                 print response in hexadecimal\n"
            "    --interface=PARAMS|-I PARAMS    specify or override "
            "interface\n"
            "    --max=MA|-M MA           programmable maximum physical link "
            "speed\n"
            "                             (8->1.5 Gbps, 9->3 Gbps, "
            "10->6 Gbps,\n"
            "                             11->12 Gbps, 12->22.5 Gbps)\n"
            "    --min=MI|-m MI           programmable minimum physical link "
            "speed\n"
            "    --op=OP|-o OP            OP (operation) is a number or "
            "abbreviation.\n"
            "                             Default: 0 (nop). See below\n"
            "    --phy=ID|-p ID           phy identifier (def: 0)\n"
            "    --pptv=TI|-P TI          partial pathway timeout value "
            "(microseconds)\n"
            "                             (if given sets UPPTV bit)\n"
            "    --pwrdis=PDC|-D PDC      sets power disable control field "
            "(def: 0)\n"
            "    --raw|-r                 output response in binary\n"
            "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
            "target (use leading\n"
            "                                 '0x' or trailing 'h'). "
            "Depending on\n"
            "                                 the interface, may not be "
            "needed\n"
            "    --sas_pa=CO|-q CO        Enable SAS Partial field; CO: "
            "0->leave (def)\n"
            "                             1->manage (enable), 2->disable\n"
            "    --sas_sl=CO|-l CO        Enable SAS Slumber field\n"
            "    --sata_pa=CO|-Q CO       Enable SATA Partial field\n"
            "    --sata_sl=CO|-L CO       Enable SATA Slumber field\n"
            "    --verbose|-v             increase verbosity\n"
            "    --version|-V             print version string and exit\n\n"
            "Performs a SMP PHY CONTROL function. Operation codes (OP): "
            "0,'nop'; 1,'lr'\n[link reset]; 2,'hr' [hard reset]; 3,'dis' "
            "[disable]; 5,'cel' [clear error\nlog]; 6,'ca' [clear "
            "affiliation]; 7,'tspss' [transmit SATA port selection\nsignal]; "
            "8,'citnl' [clear STP I_T nexus loss]; 9,'sadn' [set attached\n"
            "device name].\n"
           );
}

static void
dStrRaw(const uint8_t * str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

static struct smp_val_name op_abbrev[] = {
    {0, "nop"},
    {1, "lr"},          /* link reset */
    {2, "hr"},          /* hard reset */
    {3, "dis"},         /* disable phy */
    {5, "cel"},         /* clear error log */
    {6, "ca"},          /* clear affiliation */
    {7, "tspss"},       /* transmit SATA port selection signal */
    {8, "citnl"},       /* clear STP I_T nexus loss */
    {9, "sadn"},        /* set attached device name */
    {-1, NULL},
};

static void
list_op_abbrevs()
{
    struct smp_val_name * vnp;

    pr2serr("  Valid operation abbreviations are:\n");
    for (vnp = op_abbrev; vnp->name; ++vnp)
        pr2serr("    %s\n", vnp->name);
}


int
main(int argc, char * argv[])
{
    bool do_raw = false;
    int res, c, k, len, act_resplen;
    int expected_cc = 0;
    int do_hex = 0;
    int do_min = 0;
    int do_max = 0;
    int op_val = 0;
    int sas_pa = 0;
    int sas_sl = 0;
    int sata_pa = 0;
    int sata_sl = 0;
    int pptv = -1;
    int phy_id = 0;
    int pwrdis = 0;
    int ret = 0;
    int subvalue = 0;
    int verbose = 0;
    int64_t sa_ll;
    uint64_t sa = 0;
    uint64_t adn = 0;
    char * cp;
    struct smp_val_name * vnp;
    char i_params[256];
    char device_name[512];
    char b[256];
    uint8_t smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_PHY_CONTROL, 0, 9,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, };
    uint8_t smp_resp[8];
    struct smp_req_resp smp_rr;
    struct smp_target_obj tobj;

    memset(device_name, 0, sizeof device_name);
    memset(i_params, 0, sizeof i_params);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "a:D:E:hHI:l:L:m:M:o:p:P:q:Q;rs:vV",
                        long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
           sa_ll = smp_get_llnum_nomult(optarg);
           if (-1LL == sa_ll) {
                pr2serr("bad argument to '--attached'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            adn = (uint64_t)sa_ll;
            break;
        case 'D':
           pwrdis = smp_get_num(optarg);
           if ((pwrdis < 0) || (pwrdis > 3)) {
                pr2serr("bad argument to '--pwrdis'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'E':
            expected_cc = smp_get_num(optarg);
            if ((expected_cc < 0) || (expected_cc > 65535)) {
                pr2serr("bad argument to '--expected'\n");
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
        case 'm':
            do_min = smp_get_num(optarg);
            switch (do_min) {
            case 0:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
                break;
            default:
                pr2serr("bad argument to '--min', want 0, 8, 9, 10, 11 or "
                        "12\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'M':
            do_max = smp_get_num(optarg);
            switch (do_max) {
            case 0:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
                break;
            default:
                pr2serr("bad argument to '--max', want 0, 8, 9, 10, 11 or "
                        "12\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'l':
           sas_sl = smp_get_num(optarg);
           if ((sas_sl < 0) || (sas_sl > 3)) {
                pr2serr("bad argument to '--sas_sl'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'L':
           sata_sl = smp_get_num(optarg);
           if ((sata_sl < 0) || (sata_sl > 3)) {
                pr2serr("bad argument to '--sata_sl'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'o':
            if (isalpha(optarg[0])) {
                for (vnp = op_abbrev; vnp->name; ++vnp) {
                    if (0 == strncmp(optarg, vnp->name, 2))
                        break;
                }
                if (vnp->name)
                    op_val = vnp->value;
                else {
                    pr2serr("bad argument to '--op'\n");
                    list_op_abbrevs();
                    return SMP_LIB_SYNTAX_ERROR;
                }
            } else {
                op_val = smp_get_num(optarg);
                if ((op_val < 0) || (op_val > 255)) {
                    pr2serr("bad numeric argument to '--op'\n");
                    return SMP_LIB_SYNTAX_ERROR;
                }
            }
            break;
        case 'p':
           phy_id = smp_get_num(optarg);
           if ((phy_id < 0) || (phy_id > 254)) {
                pr2serr("bad argument to '--phy', expect value from 0 to "
                        "254\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'P':
           pptv = smp_get_num(optarg);
           if ((pptv < 0) || (pptv > 15)) {
                pr2serr("bad argument to '--pptv', want value from 0 to 15 "
                        "inclusive\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'q':
           sas_pa = smp_get_num(optarg);
           if ((sas_pa < 0) || (sas_pa > 3)) {
                pr2serr("bad argument to '--sas_pa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'Q':
           sata_pa = smp_get_num(optarg);
           if ((sata_pa < 0) || (sata_pa > 3)) {
                pr2serr("bad argument to '--sata_pa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
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

    sg_put_unaligned_be16(expected_cc, smp_req + 4);
    smp_req[9] = phy_id;
    smp_req[10] = op_val;
    if (pptv >= 0) {
        smp_req[11] |= 1;
        smp_req[36] |= (pptv & 0xf);
    }
    if (adn)
        sg_put_unaligned_be64(adn, smp_req + 24);
    smp_req[32] |= (do_min << 4);
    smp_req[33] |= (do_max << 4);
    smp_req[34] = (sas_sl << 6) | (sas_pa << 4) | (sata_sl << 2) | sata_pa;
    smp_req[35] = (pwrdis << 6);        /* added spl3r3 */
    if (verbose) {
        pr2serr("    Phy control request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k) {
            if (0 == (k % 16))
                pr2serr("\n      ");
            else if (0 == (k % 8))
                pr2serr(" ");
            pr2serr("%02x ", smp_req[k]);
        }
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
                pr2serr("Phy control result: %s\n",
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
        pr2serr("Phy control result: %s\n", cp);
        ret = smp_resp[2];
        goto err_out;
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
