/*
 * Copyright (c) 2006-2008 Douglas Gilbert.
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

static char * version_str = "1.15 20081229";    /* sas2r15 */

#ifndef OVERRIDE_TO_SAS2
#define OVERRIDE_TO_SAS2 0
#endif

struct opts_t {
    int do_brief;
    int do_hex;
    int ign_zp;
    int do_list;
    int multiple;
    int do_num;
    int phy_id;
    int do_raw;
    int verbose;
    int sa_given;
    unsigned long long sa;
};

static struct option long_options[] = {
        {"brief", 0, 0, 'b'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"ignore", 0, 0, 'i'},
        {"interface", 1, 0, 'I'},
        {"list", 0, 0, 'l'},
        {"multiple", 0, 0, 'm'},
        {"num", 1, 0, 'n'},
        {"phy", 1, 0, 'p'},
        {"sa", 1, 0, 's'},
        {"raw", 0, 0, 'r'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

static void
usage()
{
    fprintf(stderr, "Usage: "
          "smp_discover [--brief] [--help] [--hex] [--ignore] "
          "[--interface=PARAMS]\n"
          "                    [--list] [--multiple] [--num=NUM] "
          "[--phy=ID] [--raw]\n"
          "                    [--sa=SAS_ADDR] [--verbose] "
          "[--version] SMP_DEVICE[,N]\n"
          "  where:\n"
          "    --brief|-b           brief decoded output\n"
          "    --help|-h            print out usage message\n"
          "    --hex|-H             print response in hexadecimal\n"
          "    --ignore|-i          sets the Ignore Zone Group bit\n"
          "    --interface=PARAMS|-I PARAMS    specify or override "
          "interface\n"
          "    --list|-l            output attribute=value, 1 per line\n"
          "    --multiple|-m        query multiple phys, output 1 line "
          "for each\n"
          "    --num=NUM|-n NUM     number of phys to fetch when '-m' "
          "is given\n"
          "                         (def: 0 -> the rest)\n"
          "    --phy=ID|-p ID       phy identifier (def: 0) [starting "
          "phy id]\n"
          "    --raw|-r             output response in binary\n"
          "    --sa=SAS_ADDR|-s SAS_ADDR    SAS address of SMP "
          "target (use leading '0x'\n"
          "                         or trailing 'h'). Depending on "
          "the interface, may\n"
          "                         not be needed\n"
          "    --verbose|-v         increase verbosity\n"
          "    --version|-V         print version string and exit\n\n"
          "Performs a SMP DISCOVER function\n"
          );

}

static void
dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

static char * smp_attached_device_type[] = {
    "no device attached",
    "end device",
    "expander device",
    "expander device (fanout)",
    "reserved [4]",
    "reserved [5]",
    "reserved [6]",
    "reserved [7]",
};

static char * smp_short_attached_device_type[] = {
    "",         /* was "no " */
    "",         /* was "end" */
    "exp",
    "fex",      /* obsolete in sas2r05a */
    "res",
    "res",
    "res",
    "res",
};

static char *
smp_get_plink_rate(int val, int prog, int b_len, char * b)
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

static char *
smp_get_reason(int val, int b_len, char * b)
{
    switch (val) {
    case 0: snprintf(b, b_len, "unknown"); break;
    case 1: snprintf(b, b_len, "power on"); break;
    case 2: snprintf(b, b_len, "hard reset");
         break;
    case 3: snprintf(b, b_len, "SMP phy control requested");
         break;
    case 4: snprintf(b, b_len, "loss of dword synchronization"); break;
    case 5: snprintf(b, b_len, "error in multiplexing sequence"); break;
    case 6: snprintf(b, b_len, "I_T nexus loss timeout STP/SATA"); break;
    case 7: snprintf(b, b_len, "break timeout timer expired"); break;
    case 8: snprintf(b, b_len, "phy test function stopped"); break;
    case 9: snprintf(b, b_len, "expander reduced functionality"); break;
    default: snprintf(b, b_len, "reserved [%d]", val); break;
    }
    return b;
}

static char *
smp_get_neg_xxx_link_rate(int val, int b_len, char * b)
{
    switch (val) {
    case 0: snprintf(b, b_len, "phy enabled; unknown"); break;
    case 1: snprintf(b, b_len, "phy disabled"); break;
    case 2: snprintf(b, b_len, "phy enabled; speed negotiation failed");
         break;
    case 3: snprintf(b, b_len, "phy enabled; SATA spinup hold state");
         break;
    case 4: snprintf(b, b_len, "phy enabled; port selector"); break;
    case 5: snprintf(b, b_len, "phy enabled; reset in progress"); break;
    case 6: snprintf(b, b_len, "phy enabled; unsupported phy attached");
         break;
    case 8: snprintf(b, b_len, "phy enabled; 1.5 Gbps"); break;
    case 9: snprintf(b, b_len, "phy enabled; 3 Gbps"); break;
    case 0xa: snprintf(b, b_len, "phy enabled; 6 Gbps"); break;
    default: snprintf(b, b_len, "reserved [%d]", val); break;
    }
    return b;
}

static char *
find_sas_connector_type(int conn_type, char * buff, int buff_len)
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
    case 0x2f:
        snprintf(buff, buff_len, "SAS virtual connector [max 1 phy]");
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

/* Returns length of response in bytes, excluding the CRC on success,
   -3 (or less) -> SMP_LIB errors negated ('-4 - smp_err),
   -1 for other errors */
static int
do_discover(struct smp_target_obj * top, int disc_phy_id,
            unsigned char * resp, int max_resp_len,
            int silence_err_report, const struct opts_t * optsp)
{
    unsigned char smp_req[] = {SMP_FRAME_TYPE_REQ, SMP_FN_DISCOVER, 0, 2,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct smp_req_resp smp_rr;
    char b[256];
    char * cp;
    int len, res, k;

    if (optsp->ign_zp)
        smp_req[8] |= 0x1;
    smp_req[9] = disc_phy_id;
    if (optsp->verbose) {
        fprintf(stderr, "    Discover request: ");
        for (k = 0; k < (int)sizeof(smp_req); ++k)
            fprintf(stderr, "%02x ", smp_req[k]);
        fprintf(stderr, "\n");
    }
    memset(&smp_rr, 0, sizeof(smp_rr));
    smp_rr.request_len = sizeof(smp_req);
    smp_rr.request = smp_req;
    smp_rr.max_response_len = max_resp_len;
    smp_rr.response = resp;
    res = smp_send_req(top, &smp_rr, optsp->verbose);

    if (res) {
        fprintf(stderr, "smp_send_req failed, res=%d\n", res);
        if (0 == optsp->verbose)
            fprintf(stderr, "    try adding '-v' option for more debug\n");
        return -1;
    }
    if (smp_rr.transport_err) {
        fprintf(stderr, "smp_send_req transport_error=%d\n",
                smp_rr.transport_err);
        return -1;
    }
    if ((smp_rr.act_response_len >= 0) && (smp_rr.act_response_len < 4)) {
        fprintf(stderr, "response too short, len=%d\n",
                smp_rr.act_response_len);
        return -4 - SMP_LIB_CAT_MALFORMED;
    }
    len = resp[3];
    if (0 == len) {
        len = smp_get_func_def_resp_len(resp[1]);
        if (len < 0) {
            len = 0;
            if (optsp->verbose > 1)
                fprintf(stderr, "unable to determine response length\n");
        }
    }
    len = 4 + (len * 4);        /* length in bytes, excluding 4 byte CRC */
    if (optsp->do_hex || optsp->do_raw) {
        if (optsp->do_hex)
            dStrHex((const char *)resp, len, 1);
        else
            dStrRaw((const char *)resp, len);
        if (SMP_FRAME_TYPE_RESP != resp[0])
            return -4 - SMP_LIB_CAT_MALFORMED;
        if (resp[1] != smp_req[1])
            return -4 - SMP_LIB_CAT_MALFORMED;
        if (resp[2])
            return -4 - resp[2];
        return len;
    }
    if (SMP_FRAME_TYPE_RESP != resp[0]) {
        fprintf(stderr, "expected SMP frame response type, got=0x%x\n",
                resp[0]);
        return -4 - SMP_LIB_CAT_MALFORMED;
    }
    if (resp[1] != smp_req[1]) {
        fprintf(stderr, "Expected function code=0x%x, got=0x%x\n",
                smp_req[1], resp[1]);
        return -4 - SMP_LIB_CAT_MALFORMED;
    }
    if (resp[2]) {
        if ((optsp->verbose > 0) || (! silence_err_report)) {
            cp = smp_get_func_res_str(resp[2], sizeof(b), b);
            fprintf(stderr, "Discover result: %s\n", cp);
        }
        return -4 - resp[2];
    }
    return len;
}

static int
do_single_list(const unsigned char * smp_resp, int len, int show_exp_cc,
               int do_brief)
{
    int res, j, sas2;
    unsigned long long ull;

    sas2 = smp_resp[3] ? 1 : OVERRIDE_TO_SAS2;
    if (sas2 && show_exp_cc && (! do_brief)) {
        res = (smp_resp[4] << 8) + smp_resp[5];
        printf("expander_cc=%d\n", res);
    }
    printf("phy_id=%d\n", smp_resp[9]);
    if (! do_brief) {
        if (sas2)
            printf("  att_break_rc=%d\n", !!(0x1 & smp_resp[33]));
        if (len > 59) {
            for (ull = 0, j = 0; j < 8; ++j) {
                if (j > 0)
                    ull <<= 8;
                ull |= smp_resp[52 + j];
            }
            printf("  att_dev_name=0x%llx\n", ull);
        }
    }
    printf("  att_dev_type=%d\n", (0x70 & smp_resp[12]) >> 4);
    printf("  att_iport_mask=0x%x\n", smp_resp[14]);
    if (sas2 && (! do_brief))
        printf("  att_izp=%d\n", !!(0x4 & smp_resp[33]));
    printf("  att_phy_id=%d\n", smp_resp[32]);
    if (sas2 && (! do_brief)) {
        printf("  att_reason=%d\n", (0xf & smp_resp[12]));
        printf("  att_req_iz=%d\n", !!(0x2 & smp_resp[33]));
    }
    for (ull = 0, j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= smp_resp[24 + j];
    }
    printf("  att_sas_addr=0x%llx\n", ull);
    printf("  att_tport_mask=0x%x\n", smp_resp[15]);
    if (! do_brief) {
        if (sas2 || (smp_resp[45] & 0x7f)) {
            printf("  conn_elem_ind=%d\n", smp_resp[46]);
            printf("  conn_p_link=%d\n", smp_resp[47]);
            printf("  conn_type=%d\n", (0x7f & smp_resp[45]));
        }
    }
    if (! do_brief) {
        printf("  hw_max_p_lrate=%d\n", (0xf & smp_resp[41]));
        printf("  hw_min_p_lrate=%d\n", (0xf & smp_resp[40]));
        if (len > 95)
            printf("  hw_mux_sup=%d\n", (!! (smp_resp[95] & 0x1)));
    }
    if (! do_brief) {
        printf("  pr_max_p_lrate=%d\n", ((0xf0 & smp_resp[41]) >> 4));
        printf("  pr_min_p_lrate=%d\n", ((0xf0 & smp_resp[40]) >> 4));
    }

    printf("  neg_log_lrate=%d\n", (0xf & smp_resp[13]));
    if (! do_brief) {
        if (len > 95)
            printf("  neg_phy_lrate=%d\n", (0xf & smp_resp[94]));
        printf("  pp_timeout=%d\n", !!(0xf & smp_resp[43]));
        printf("  phy_cc=%d\n", smp_resp[42]);
        if (len > 95)
            printf("  reason=%d\n", (0xf0 & smp_resp[94]) >> 4);
    }
    
    for (ull = 0, j = 0; j < 8; ++j) {
        if (j > 0)
            ull <<= 8;
        ull |= smp_resp[16 + j];
    }
    printf("  sas_addr=0x%llx\n", ull);

    printf("  virt_phy=%d\n", !!(0x80 & smp_resp[43]));
    return 0;
}

static int
do_single(struct smp_target_obj * top, const struct opts_t * optsp)
{
    unsigned char smp_resp[128];
    unsigned long long ull;
    unsigned int ui;
    int res, len, j, sas2;
    char b[256];

    len = do_discover(top, optsp->phy_id, smp_resp, sizeof(smp_resp), 0,
                      optsp);
    if (len < 0)
        return (len < -2) ? (-4 - len) : len;
    if (optsp->do_hex || optsp->do_raw)
        return 0;
    if (optsp->do_list)
        return do_single_list(smp_resp, len, 1, optsp->do_brief);
    printf("Discover response%s:\n", (optsp->do_brief ? " (brief)" : ""));
    sas2 = smp_resp[3] ? 1 : OVERRIDE_TO_SAS2;
    res = (smp_resp[4] << 8) + smp_resp[5];
    if (sas2 || (optsp->verbose > 3)) {
        if (optsp->verbose || (res > 0))
            printf("  expander change count: %d\n", res);
    }
    printf("  phy identifier: %d\n", smp_resp[9]);
    res = ((0x70 & smp_resp[12]) >> 4);
    if (res < 8)
        printf("  attached device type: %s\n", smp_attached_device_type[res]);
    if ((optsp->do_brief > 1) && (0 == res))
        return 0;
    if (sas2 || (optsp->verbose > 3))
        printf("  attached reason: %s\n",
               smp_get_reason(0xf & smp_resp[12], sizeof(b), b));

    printf("  negotiated logical link rate: %s\n",
           smp_get_neg_xxx_link_rate(0xf & smp_resp[13], sizeof(b), b));

    printf("  attached initiator: ssp=%d stp=%d smp=%d sata_host=%d\n",
           !! (smp_resp[14] & 8), !! (smp_resp[14] & 4),
           !! (smp_resp[14] & 2), (smp_resp[14] & 1));
    if (0 == optsp->do_brief)
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
    if (0 == optsp->do_brief) {
        if (sas2 || (optsp->verbose > 3)) {
            printf("  attached inside ZPSDS persistent: %d\n",
                   smp_resp[33] & 4);
            printf("  attached requested inside ZPSDS: %d\n",
                   smp_resp[33] & 2);
            printf("  attached break_reply capable: %d\n", smp_resp[33] & 1);
        }
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
    if (optsp->do_brief)
        return 0;
    if (sas2 || (smp_resp[45] & 0x7f)) {
        printf("  connector type: %s\n",
               find_sas_connector_type((smp_resp[45] & 0x7f), b, sizeof(b)));
        printf("  connector element index: %d\n", smp_resp[46]);
        printf("  connector physical link: %d\n", smp_resp[47]);
    }
    if (len > 59) {
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= smp_resp[52 + j];
        }
        printf("  attached device name: 0x%llx\n", ull);
        printf("  requested inside ZPSDS changed by expander: %d\n",
              !!(smp_resp[60] & 0x40));
        printf("  inside ZPSDS persistent: %d\n", !!(smp_resp[60] & 0x20));
        printf("  requested inside ZPSDS: %d\n", !!(smp_resp[60] & 0x10));
        /* printf("  zone address resolved: %d\n", !!(smp_resp[60] & 0x8)); */
        printf("  zone group persistent: %d\n", !!(smp_resp[60] & 0x4));
        printf("  zone participating: %d\n", !!(smp_resp[60] & 0x2));
        printf("  zone enabled: %d\n", !!(smp_resp[60] & 0x1));
        printf("  zone group: %d\n", smp_resp[63]);
        if (len < 76)
            return 0;
        printf("  self-configuration status: %d\n", smp_resp[64]);
        printf("  self-configuration levels completed: %d\n", smp_resp[65]);
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= smp_resp[68 + j];
        }
        printf("  self-configuration sas address: 0x%llx\n", ull);
        ui = 0;
        for (j = 0; j < 4; ++j) {
            if (j > 0)
                ui <<= 8;
            ui |= smp_resp[76 + j];
        }
        printf("  programmed phy capabilities: 0x%x\n", ui);
        ui = 0;
        for (j = 0; j < 4; ++j) {
            if (j > 0)
                ui <<= 8;
            ui |= smp_resp[80 + j];
        }
        printf("  current phy capabilities: 0x%x\n", ui);
        ui = 0;
        for (j = 0; j < 4; ++j) {
            if (j > 0)
                ui <<= 8;
            ui |= smp_resp[84 + j];
        }
        printf("  attached phy capabilities: 0x%x\n", ui);
    }
    if (len > 95) {
        printf("  reason: %s\n",
               smp_get_reason((0xf0 & smp_resp[94]) >> 4, sizeof(b), b));
        printf("  negotiated physical link rate: %s\n",
               smp_get_neg_xxx_link_rate(0xf & smp_resp[94], sizeof(b), b));
        printf("  negotiated SSC: %d\n", !!(smp_resp[95] & 0x2));
        printf("  hardware muxing supported: %d\n", !!(smp_resp[95] & 0x1));
    }
    if (len > 107) {
        printf("  default inside ZPSDS persistent: %d\n",
               !!(smp_resp[96] & 0x20));
        printf("  default requested inside ZPSDS: %d\n",
               !!(smp_resp[96] & 0x10));
        printf("  default zone group persistent: %d\n",
               !!(smp_resp[96] & 0x4));
        printf("  default zoning enabled: %d\n", !!(smp_resp[96] & 0x1));
        printf("  default zone group: %d\n", smp_resp[99]);
        printf("  saved inside ZPSDS persistent: %d\n",
               !!(smp_resp[100] & 0x20));
        printf("  saved requested inside ZPSDS: %d\n",
               !!(smp_resp[100] & 0x10));
        printf("  saved zone group persistent: %d\n",
               !!(smp_resp[100] & 0x4));
        printf("  saved zoning enabled: %d\n", !!(smp_resp[100] & 0x1));
        printf("  saved zone group: %d\n", smp_resp[103]);
        printf("  shadow inside ZPSDS persistent: %d\n",
               !!(smp_resp[104] & 0x20));
        printf("  shadow requested inside ZPSDS: %d\n",
               !!(smp_resp[104] & 0x10));
        printf("  shadow zone group persistent: %d\n",
               !!(smp_resp[104] & 0x4));
        printf("  shadow zone group: %d\n", smp_resp[107]);
    }
    if (len > 115) {
        printf("  device slot number: %d\n", smp_resp[108]);
        printf("  device slot group number: %d\n", smp_resp[109]);
        printf("  device slot group output connector: %.6s\n", smp_resp + 110);
    }
    return 0;
}

#define MAX_PHY_ID 8192

static int
do_multiple(struct smp_target_obj * top, const struct opts_t * optsp)
{
    unsigned char smp_resp[128];
    unsigned long long ull;
    unsigned long long expander_sa;
    int res, len, k, j, num, off, plus, negot, adt;
    char b[256];
    int first = 1;
    int ret = 0;
    const char * cp;

    expander_sa = 0;
    num = optsp->do_num ? (optsp->phy_id + optsp->do_num) : MAX_PHY_ID;
    for (k = optsp->phy_id; k < num; ++k) {
        len = do_discover(top, k, smp_resp, sizeof(smp_resp), 1, optsp);
        if (len < 0)
            ret = (len < -2) ? (-4 - len) : len;
        if (SMP_FRES_NO_PHY == ret)
            return 0;   /* expected, end condition */
        if (ret)
            return ret;
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= smp_resp[16 + j];
        }
        if (0 == expander_sa)
            expander_sa = ull;
        else {
            if (ull != expander_sa) {
                fprintf(stderr, ">> expander's SAS address is changing?? "
                        "was=%llxh, now=%llxh\n", expander_sa, ull);
                expander_sa = ull;
            }
        }
        if (first && (! optsp->do_raw)) {
            first = 0;
            if (optsp->sa_given && (optsp->sa != expander_sa))
                printf("  <<< Warning: reported expander address is not the "
                       "one requested >>>\n");
            printf("Device <%016llx>, expander%s:\n", expander_sa,
                   (optsp->do_brief ? " (only connected phys shown)" : ""));
        }
        if (optsp->do_hex || optsp->do_raw)
            continue;
        res = ((0x70 & smp_resp[12]) >> 4);
        if ((optsp->do_brief > 0) && (0 == res))
            continue;
        if (optsp->do_list) {
            do_single_list(smp_resp, len, 0, optsp->do_brief);
            continue;
        }

        negot = smp_resp[13] & 0xf;
        switch(smp_resp[44] & 0xf) {
        case 0:
            cp = "D";
            break;
        case 1:
            cp = "S";
            break;
        case 2:
            cp = "T";
            break;
        default:
            cp = "R";
            break;
        }
        if (1 == negot) {
            printf("  phy %3d:%s:disabled\n", smp_resp[9], cp);
            continue;
        } else if (2 == negot) {
            printf("  phy %3d:%s:reset problem\n", smp_resp[9], cp);
            continue;
        } else if (3 == negot) {
            printf("  phy %3d:%s:spinup hold\n", smp_resp[9], cp);
            continue;
        } else if (4 == negot) {
            printf("  phy %3d:%s:port selector\n", smp_resp[9], cp);
            continue;
        } else if (5 == negot) {
            printf("  phy %3d:%s:reset in progress\n", smp_resp[9], cp);
            continue;
        } else if (6 == negot) {
            printf("  phy %3d:%s:unsupported phy attached\n", smp_resp[9],
                   cp);
            continue;
        }
        if (k != smp_resp[9])
            fprintf(stderr, ">> requested phy_id=%d differs from response "
                    "phy=%d\n", k, smp_resp[9]);
        ull = 0;
        for (j = 0; j < 8; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= smp_resp[24 + j];
        }
        adt = ((0x70 & smp_resp[12]) >> 4);
        if ((0 == adt) || (adt > 3)) {
            printf("  phy %3d:%s:attached:[0000000000000000:00]\n", k, cp);
            continue;
        }
        printf("  phy %3d:%s:attached:[%016llx:%02d %s%s", k, cp, ull,
               smp_resp[32], smp_short_attached_device_type[adt],
               ((smp_resp[43] & 0x80) ? " V" : ""));
        if (smp_resp[14] & 0xf) {
            off = 0;
            plus = 0;
            off += snprintf(b + off, sizeof(b) - off, " i(");
            if (smp_resp[14] & 0x8) {
                off += snprintf(b + off, sizeof(b) - off, "SSP");
                ++plus;
            }
            if (smp_resp[14] & 0x4) {
                off += snprintf(b + off, sizeof(b) - off, "%sSTP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (smp_resp[14] & 0x2) {
                off += snprintf(b + off, sizeof(b) - off, "%sSMP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (smp_resp[14] & 0x1) {
                off += snprintf(b + off, sizeof(b) - off, "%sSATA",
                                (plus ? "+" : ""));
                ++plus;
            }
            printf("%s)", b);
        }
        if (smp_resp[15] & 0xf) {
            off = 0;
            plus = 0;
            off += snprintf(b + off, sizeof(b) - off, " t(");
            if (smp_resp[15] & 0x80) {
                off += snprintf(b + off, sizeof(b) - off, "PORT_SEL");
                ++plus;
            }
            if (smp_resp[15] & 0x8) {
                off += snprintf(b + off, sizeof(b) - off, "%sSSP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (smp_resp[15] & 0x4) {
                off += snprintf(b + off, sizeof(b) - off, "%sSTP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (smp_resp[15] & 0x2) {
                off += snprintf(b + off, sizeof(b) - off, "%sSMP",
                                (plus ? "+" : ""));
                ++plus;
            }
            if (smp_resp[15] & 0x1) {
                off += snprintf(b + off, sizeof(b) - off, "%sSATA",
                                (plus ? "+" : ""));
                ++plus;
            }
            printf("%s)", b);
        }
        if (optsp->do_brief > 1) {
            printf("]\n");
            continue;
        } else
            printf("]");
        switch(negot) {
        case 8:
            cp = "  1.5 Gbps";
            break;
        case 9:
            cp = "  3 Gbps";
            break;
        case 0xa:
            cp = "  6 Gbps";
            break;
        default:
            cp = "";
            break;
        }
        printf("%s\n", cp);
    }
    return 0;
}


int
main(int argc, char * argv[])
{
    int res, c;
    long long sa_ll;
    char i_params[256];
    char device_name[512];
    struct smp_target_obj tobj;
    int subvalue = 0;
    char * cp;
    int ret = 0;
    struct opts_t opts;

    memset(&opts, 0, sizeof(opts));
    memset(device_name, 0, sizeof device_name);
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "bhHiI:lmn:p:rs:vV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'b':
            ++opts.do_brief;
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            ++opts.do_hex;
            break;
        case 'i':
            ++opts.ign_zp;
            break;
        case 'I':
            strncpy(i_params, optarg, sizeof(i_params));
            i_params[sizeof(i_params) - 1] = '\0';
            break;
        case 'l':
            ++opts.do_list;
            break;
        case 'm':
            ++opts.multiple;
            break;
        case 'n':
           opts.do_num = smp_get_num(optarg);
           if (opts.do_num < 0) {
                fprintf(stderr, "bad argument to '--num'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'p':
           opts.phy_id = smp_get_num(optarg);
           if ((opts.phy_id < 0) || (opts.phy_id > 127)) {
                fprintf(stderr, "bad argument to '--phy'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            break;
        case 'r':
            ++opts.do_raw;
            break;
        case 's':
           sa_ll = smp_get_llnum(optarg);
           if (-1LL == sa_ll) {
                fprintf(stderr, "bad argument to '--sa'\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
            opts.sa = (unsigned long long)sa_ll;
            if (opts.sa > 0)
                ++opts.sa_given;
            break;
        case 'v':
            ++opts.verbose;
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
    if ((cp = strchr(device_name, ','))) {
        *cp = '\0';
        if (1 != sscanf(cp + 1, "%d", &subvalue)) {
            fprintf(stderr, "expected number after comma in SMP_DEVICE "
                    "name\n");
            return SMP_LIB_SYNTAX_ERROR;
        }
    }
    if (0 == opts.sa) {
        cp = getenv("SMP_UTILS_SAS_ADDR");
        if (cp) {
           sa_ll = smp_get_llnum(cp);
           if (-1LL == sa_ll) {
                fprintf(stderr, "bad value in environment variable "
                        "SMP_UTILS_SAS_ADDR\n");
                fprintf(stderr, "    use 0\n");
                sa_ll = 0;
            }
            opts.sa = (unsigned long long)sa_ll;
        }
    }
    if (opts.sa > 0) {
        if (! smp_is_naa5(opts.sa)) {
            fprintf(stderr, "SAS (target) address not in naa-5 format "
                    "(may need leading '0x')\n");
            if ('\0' == i_params[0]) {
                fprintf(stderr, "    use '--interface=' to override\n");
                return SMP_LIB_SYNTAX_ERROR;
            }
        }
    }

    res = smp_initiator_open(device_name, subvalue, i_params, opts.sa,
                             &tobj, opts.verbose);
    if (res < 0)
        return SMP_LIB_FILE_ERROR;

    if (opts.multiple)
        ret = do_multiple(&tobj, &opts);
    else
        ret = do_single(&tobj, &opts);
    res = smp_initiator_close(&tobj);
    if (res < 0) {
        if (0 == ret)
            return SMP_LIB_FILE_ERROR;
    }
    return (ret >= 0) ? ret : SMP_LIB_CAT_OTHER;
}
