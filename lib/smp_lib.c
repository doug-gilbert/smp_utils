/*
 * Copyright (c) 2006-2019, Douglas Gilbert
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "smp_lib.h"
#include "sg_unaligned.h"
#include "sg_pr2serr.h"


static const char * version_str = "1.30 20190710";    /* spl-5 rev 8 */

/* Assume original SAS implementations were based on SAS-1.1 . In SAS-2
 * and later, SMP responses should contain an accurate "response length"
 * field. However is SAS-1.1 (sas1r10.pdf) the "response length field
 * (byte 3) is always 0 irrespective of the response's length. There is
 * a similar problem with the "request length" field in the request.
 * So if zero is found in either the request/response fields this table
 * is consulted.
 * The units of 'def_req_len' and 'def_resp_len' are dwords (4 bytes)
 * calculated by: ((len_bytes - 8) / 4) where 'len_bytes' includes
 * the 4 byte CRC at the end of each frame. The 4 byte CRC field
 * does not need to be set (just space allocated (for some pass
 * throughs)). */
struct smp_func_def_rrlen {
    int func;           /* '-1' for last entry */
    int def_req_len;    /* if 0==<request_length> use this value, unless */
                        /*  -2 -> no default; -3 -> different format */
    int def_resp_len;   /* if 0==<response_length> use this value, unless */
                        /*  -2 -> no default; -3 -> different format */
    /* N.B. Some SAS-2 functions have 8 byte request or response lengths.
            This is noted by putting 0 in one of the two above fields. */
};

/* Positive request and response lengths match SAS-1.1 (sas1r10.pdf) */
struct smp_func_def_rrlen smp_def_rrlen_arr[] = {
    /* in numerical order by 'func' */
    {SMP_FN_REPORT_GENERAL, 0, 6},
    {SMP_FN_REPORT_MANUFACTURER, 0, 14},
    {SMP_FN_READ_GPIO_REG, -3, -3}, /* obsolete, not applicable: SFF-8485 */
    {SMP_FN_REPORT_SELF_CONFIG, -2, -2},
    {SMP_FN_REPORT_ZONE_PERMISSION_TBL, -2, -2},/* variable length response */
    {SMP_FN_REPORT_ZONE_MANAGER_PASS, -2, -2},
    {SMP_FN_REPORT_BROADCAST, -2, -2},
    {SMP_FN_READ_GPIO_REG_ENH, -2, -2}, /* SFF-8485 should explain */
    {SMP_FN_DISCOVER, 2, 0xc},
    {SMP_FN_REPORT_PHY_ERR_LOG, 2, 6},
    {SMP_FN_REPORT_PHY_SATA, 2, 13},
    {SMP_FN_REPORT_ROUTE_INFO, 2, 9},
    {SMP_FN_REPORT_PHY_EVENT, -2, -2},     /* variable length response */
    {SMP_FN_DISCOVER_LIST, -2, -2},
    {SMP_FN_REPORT_PHY_EVENT_LIST, -2, -2},
    {SMP_FN_REPORT_EXP_ROUTE_TBL_LIST, -2, -2},
    {SMP_FN_CONFIG_GENERAL, 3, 0},
    {SMP_FN_ENABLE_DISABLE_ZONING, -2, 0},
    {SMP_FN_WRITE_GPIO_REG, -3, -3}, /* obsolete, not applicable: SFF-8485 */
    {SMP_FN_WRITE_GPIO_REG_ENH, -2, -2}, /* SFF-8485 should explain */
    {SMP_FN_ZONED_BROADCAST, -2, 0},            /* variable length request */
    {SMP_FN_ZONE_LOCK, -2, -2},
    {SMP_FN_ZONE_ACTIVATE, -2, 0},
    {SMP_FN_ZONE_UNLOCK, -2, 0},
    {SMP_FN_CONFIG_ZONE_MANAGER_PASS, -2, 0},
    {SMP_FN_CONFIG_ZONE_PHY_INFO, -2, 0},       /* variable length request */
    {SMP_FN_CONFIG_ZONE_PERMISSION_TBL, -2, 0}, /* variable length request */
    {SMP_FN_CONFIG_ROUTE_INFO, 9, 0},
    {SMP_FN_PHY_CONTROL, 9, 0},
    {SMP_FN_PHY_TEST_FUNCTION, 9, 0},
    {SMP_FN_CONFIG_PHY_EVENT, -2, 0},      /* variable length request */
    {-1, -1, -1},
};

#if defined(__GNUC__) || defined(__clang__)
static int scnpr(char * cp, int cp_max_len, const char * fmt, ...)
                 __attribute__ ((format (printf, 3, 4)));
#else
static int scnpr(char * cp, int cp_max_len, const char * fmt, ...);
#endif

/* Want safe, 'n += snprintf(b + n, blen - n, ...)' style sequence of
 * functions. Returns number of chars placed in cp excluding the
 * trailing null char. So for cp_max_len > 0 the return value is always
  * < cp_max_len; for cp_max_len <= 1 the return value is 0 and no chars are
 * written to cp. Note this means that when cp_max_len = 1, this function
 * assumes that cp[0] is the null character and does nothing (and returns
 * 0). Linux kernel has a similar function called  scnprintf().  */
static int
scnpr(char * cp, int cp_max_len, const char * fmt, ...)
{
    va_list args;
    int n;

    if (cp_max_len < 2)
        return 0;
    va_start(args, fmt);
    n = vsnprintf(cp, cp_max_len, fmt, args);
    va_end(args);
    return (n < cp_max_len) ? n : (cp_max_len - 1);
}

/* Simple ASCII printable (does not use locale), includes space and excludes
 * DEL (0x7f). */
static inline int my_isprint(int ch)
{
    return ((ch >= ' ') && (ch < 0x7f));
}

static void
trimTrailingSpaces(char * b)
{
    int k;

    for (k = ((int)strlen(b) - 1); k >= 0; --k) {
        if (' ' != b[k])
            break;
    }
    if ('\0' != b[k + 1])
        b[k + 1] = '\0';
}


int
smp_get_func_def_req_len(int func_code)
{
    struct smp_func_def_rrlen * drlp;

    for (drlp = smp_def_rrlen_arr; drlp->func >= 0; ++drlp) {
        if (func_code == drlp->func)
            return drlp->def_req_len;
    }
    return -1;
}

int
smp_get_func_def_resp_len(int func_code)
{
    struct smp_func_def_rrlen * drlp;

    for (drlp = smp_def_rrlen_arr; drlp->func >= 0; ++drlp) {
        if (func_code == drlp->func)
            return drlp->def_resp_len;
    }
    return -1;
}


static struct smp_val_name smp_func_results[] =
{
    {SMP_FRES_FUNCTION_ACCEPTED, "SMP function accepted"},
    {SMP_FRES_UNKNOWN_FUNCTION, "Unknown SMP function"},
    {SMP_FRES_FUNCTION_FAILED, "SMP function failed"},
    {SMP_FRES_INVALID_REQUEST_LEN, "Invalid request frame length"},
    {SMP_FRES_INVALID_EXP_CHANGE_COUNT, "Invalid expander change count"},
    {SMP_FRES_BUSY, "Busy"},
    {SMP_FRES_INCOMPLETE_DESCRIPTOR_LIST, "Incomplete descriptor list"},
    {SMP_FRES_NO_PHY, "Phy does not exist"},
    {SMP_FRES_NO_INDEX, "Index does not exist"},
    {SMP_FRES_NO_SATA_SUPPORT, "Phy does not support SATA"},
    {SMP_FRES_UNKNOWN_PHY_OP, "Unknown phy operation"},
    {SMP_FRES_UNKNOWN_PHY_TEST_FN, "Unknown phy test function"},
    {SMP_FRES_PHY_TEST_IN_PROGRESS, "Phy test function in progress"},
    {SMP_FRES_PHY_VACANT, "Phy vacant"},
    {SMP_FRES_UNKNOWN_PHY_EVENT_SRC,
     "Unknown phy event source"},
    {SMP_FRES_UNKNOWN_DESCRIPTOR_TYPE, "Unknown descriptor type"},
    {SMP_FRES_UNKNOWN_PHY_FILTER, "Unknown phy filter"},
    {SMP_FRES_AFFILIATION_VIOLATION, "Affiliation violation"},
    {SMP_FRES_SMP_ZONE_VIOLATION, "SMP zone violation"},
    {SMP_FRES_NO_MANAGEMENT_ACCESS, "No management access rights"},
    {SMP_FRES_UNKNOWN_EN_DIS_ZONING_VAL,
     "Unknown enable disable zoning value"},
    {SMP_FRES_ZONE_LOCK_VIOLATION, "Zone lock violation"},
    {SMP_FRES_NOT_ACTIVATED, "Not activated"},
    {SMP_FRES_ZONE_GROUP_OUT_OF_RANGE, "Zone group out of range"},
    {SMP_FRES_NO_PHYSICAL_PRESENCE, "No physical presence"},
    {SMP_FRES_SAVING_NOT_SUPPORTED, "Saving not supported"},
    {SMP_FRES_SOURCE_ZONE_GROUP, "Source zone group does not exist"},
    {SMP_FRES_DIS_PASSWORD_NOT_SUPPORTED, "Disabled password not supported"},
    {SMP_FRES_INVALID_FIELD_IN_REQUEST, "Invalid field in SMP request"},
    {0x0, NULL},
};

char *
smp_get_func_res_str(int func_res, int buff_len, char * buff)
{
    struct smp_val_name * vnp;

    for (vnp = smp_func_results; vnp->name; ++vnp) {
        if (func_res == vnp->value) {
            snprintf(buff, buff_len, "%s", vnp->name);
            return buff;
        }
    }
    snprintf(buff, buff_len, "Unknown function result code=0x%x\n", func_res);
    return buff;
}

/* spl5r04.pdf says a valid SAS address can be NAA-5 or NAA-3 (locally
 * assigned). It prefers NAA-5. */
bool
smp_is_sas_naa(uint64_t addr)
{
    uint8_t top_nibble = ((addr >> 60) & 0xf);

    return ((0x5 == top_nibble) || (0x3 == top_nibble));
}

/* Better to use smp_is_sas_naa() to replace this one. */
bool
smp_is_naa5(uint64_t addr)
{
    return (0x5 == ((addr >> 60) & 0xf));
}

/* Connector names are taken from the most recent SES draft; in this case
 * ses4r01. If plink is true the "(<maximum >physical links: <n>)" is
 * appended to connector type string. <n> is 0 if conn_type is 0 or not
 * found. <maximum > only prints "maximum " when <n> is greater than 1 .
 * Returns buff as its result and its length (including a trailing null
 * character) will not exceed buff_len. */
char *
smp_get_connector_type_str(int conn_type, bool plink, int buff_len,
                           char * buff)
{
    int pl_num = 0;
    int n;

    if ((NULL == buff) || (buff_len < 1))
        return buff;
    switch (conn_type) {
/* External connectors */
    case 0x0:
        snprintf(buff, buff_len, "No information");
        break;
    case 0x1:
        snprintf(buff, buff_len, "SAS 4x receptacle (SFF-8470)");
        pl_num = 4;
        break;
    case 0x2:
        snprintf(buff, buff_len, "Mini SAS 4x receptacle (SFF-8088)");
        pl_num = 4;
        break;
    case 0x3:
        snprintf(buff, buff_len, "QSFP+ receptacle (SFF-8436)");
        pl_num = 4;
        break;
    case 0x4:
        snprintf(buff, buff_len, "Mini SAS 4x active receptacle (SFF-8088)");
        pl_num = 4;
        break;
    case 0x5:
        snprintf(buff, buff_len, "Mini SAS HD 4x receptacle (SFF-8644)");
        pl_num = 4;
        break;
    case 0x6:
        snprintf(buff, buff_len, "Mini SAS HD 8x receptacle (SFF-8644)");
        pl_num = 8;
        break;
    case 0x7:
        snprintf(buff, buff_len, "Mini SAS HD 16x receptacle (SFF-8644)");
        pl_num = 16;
        break;
    case 0xf:
        snprintf(buff, buff_len, "Vendor specific external connector");
        pl_num = -1;
        break;
/* Internal wide connectors */
    case 0x10:
        snprintf(buff, buff_len, "SAS 4i plug (SFF-8484)");
        pl_num = 4;
        break;
    case 0x11:
        snprintf(buff, buff_len, "Mini SAS 4i receptacle (SFF-8087)");
        pl_num = 4;
        break;
    case 0x12:
        snprintf(buff, buff_len, "Mini SAS HD 4i receptacle (SFF-8643)");
        pl_num = 4;
        break;
    case 0x13:
        snprintf(buff, buff_len, "Mini SAS HD 8i receptacle (SFF-8643)");
        pl_num = 8;
        break;
    case 0x14:
        snprintf(buff, buff_len, "Mini SAS HD 16i receptacle (SFF-8643)");
        pl_num = 16;
        break;
    case 0x15:	/* was 'SAS SlimLine', changed ses4r03 */
        snprintf(buff, buff_len, "SlimSAS 4i (SFF-8654)");
        pl_num = 4;
        break;
    case 0x16:	/* was 'SAS SlimLine', changed ses4r03 */
        snprintf(buff, buff_len, "SlimSAS 8i (SFF-8654)");
        pl_num = 8;
        break;
    case 0x17:
        snprintf(buff, buff_len, "SAS MiniLink 4i (SFF-8612)");
        pl_num = 4;
        break;
    case 0x18:
        snprintf(buff, buff_len, "SAS MiniLink 8i (SFF-8612)");
        pl_num = 8;
        break;
/* Internal connectors to end devices */
    case 0x20:
        snprintf(buff, buff_len, "SAS Drive backplane receptacle (SFF-8482)");
        pl_num = 2;
        break;
    case 0x21:
        snprintf(buff, buff_len, "SATA host plug");
        pl_num = 1;
        break;
    case 0x22:
        snprintf(buff, buff_len, "SAS Drive plug (SFF-8482)");
        pl_num = 2;
        break;
    case 0x23:
        snprintf(buff, buff_len, "SATA device plug");
        pl_num = 1;
        break;
    case 0x24:
        snprintf(buff, buff_len, "Micro SAS receptacle");
        pl_num = 2;
        break;
    case 0x25:
        snprintf(buff, buff_len, "Micro SATA device plug");
        pl_num = 1;
        break;
    case 0x26:
        snprintf(buff, buff_len, "Micro SAS plug (SFF-8486");
        pl_num = 2;
        break;
    case 0x27:
        snprintf(buff, buff_len, "Micro SAS/SATA plug (SFF-8486)");
        pl_num = 2;
        break;
    case 0x28:
        snprintf(buff, buff_len, "12 Gb/s SAS Drive backplane receptacle "
                 "(SFF-8680)");
        pl_num = 2;
        break;
    case 0x29:
        snprintf(buff, buff_len, "12Gb/s SAS Drive Plug (SFF-8680) ");
        pl_num = 2;
        break;
    case 0x2a:
        snprintf(buff, buff_len, "Multifunction 12 Gb/s 6x Unshielded "
                 "receptacle (SFF-8639)");
        pl_num = 6;
        break;
    case 0x2b:
        snprintf(buff, buff_len, "Multifunction 12 Gb/s 6x Unshielded plug "
                "(SFF-8639)");
        pl_num = 6;
        break;
    case 0x2c:
        snprintf(buff, buff_len, "SAS MultiLink drive backplane receptacle "
                 "(SFF-8630)");
        pl_num = 4;
        break;
    case 0x2d:
        snprintf(buff, buff_len, "SAS MultiLink drive backplane plug "
                 "(SFF-8630)");
        pl_num = 4;
        break;
    case 0x2f:
        snprintf(buff, buff_len, "SAS virtual connector");
        pl_num = 1;
        break;
    case 0x3f:
        snprintf(buff, buff_len, "Vendor specific internal connector");
        pl_num = -1;
        break;
    case 0x40:
        snprintf(buff, buff_len, "SAS high density drive backplane "
                 "receptacle (SFF-8631)");
        pl_num = 8;
        break;
    case 0x41:
        snprintf(buff, buff_len, "SAS high density drive backplane "
                 "plug (SFF-8631)");
        pl_num = 8;
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
        else if (conn_type < 0x3f)
            snprintf(buff, buff_len, "unknown internal connector"
                     ", type: 0x%x", conn_type);
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
    if (! plink)
        return buff;
    n = strlen(buff);
    if (n >= (buff_len - 1))
        return buff;    /* no room for suffix */
    if (pl_num < 1)
        snprintf(buff + n, buff_len - n, "(physical links: 0)");
    else if (pl_num < 2)
        snprintf(buff + n, buff_len - n, "(physical links: 1)");
    else
        snprintf(buff + n, buff_len - n, "(maximum physical links: %d)",
                 pl_num);
    return buff;
}

static const char * phy_pwr_cond_arr[4] = {
    "active",
    "partial",
    "slumber",
    "reserved",
};

/* Returns pointer to phy power condition string or "illegal" if the
 * 'phy_pwr_cond' value is out of range. Pointer value returned is same
 * as 'buff'. String placed in 'buff' is null terminated and its length
 * (including terminator) does not exceed 'buff_len'. Does nothing if
 *'buff' is NULL or 'buff_len' less than 1. If 'buff_len' is 1 then just
 * puts null character in 'buff'. */
char *
smp_get_phy_pwr_cond_str(int phy_pwr_cond, int buff_len, char * buff)
{
    if ((NULL == buff) || (buff_len < 1))
        return buff;
    if ((phy_pwr_cond < 0) || (phy_pwr_cond > 3))
        snprintf(buff, buff_len, "illegal");
    else
        snprintf(buff, buff_len, "%s", phy_pwr_cond_arr[phy_pwr_cond]);
    return buff;
}

static const char * pwr_dis_signal_arr[4] = {
    "not capable",
    "reserved",
    "negated",
    "asserted",
};

/* Returns pointer to pwr_dis signal string or "illegal" if the
 * 'pwr_dis_signal' value is out of range. Pointer value returned is same
 * as 'buff'. String placed in 'buff' is null terminated and its length
 * (including terminator) does not exceed 'buff_len'. Does nothing if
 *'buff' is NULL or 'buff_len' less than 1. If 'buff_len' is 1 then just
 * puts null character in 'buff'. */
char *
smp_get_pwr_dis_signal_str(int pwr_dis_signal, int buff_len, char * buff)
{
    if ((NULL == buff) || (buff_len < 1))
        return buff;
    if ((pwr_dis_signal < 0) || (pwr_dis_signal > 3))
        snprintf(buff, buff_len, "illegal");
    else
        snprintf(buff, buff_len, "%s", pwr_dis_signal_arr[pwr_dis_signal]);
    return buff;
}

/* safe_strerror() contributed by Clayton Weaver <cgweav at email dot com>
   Allows for situation in which strerror() is given a wild value (or the
   C library is incomplete) and returns NULL. Still not thread safe.
 */

static char safe_errbuf[64] = {'u', 'n', 'k', 'n', 'o', 'w', 'n', ' ',
                               'e', 'r', 'r', 'n', 'o', ':', ' ', 0};

char *
safe_strerror(int errnum)
{
    size_t len;
    char * errstr;

    if (errnum < 0)
        errnum = -errnum;
    errstr = strerror(errnum);
    if (NULL == errstr) {
        len = strlen(safe_errbuf);
        snprintf(safe_errbuf + len, sizeof(safe_errbuf) - len, "%i", errnum);
        safe_errbuf[sizeof(safe_errbuf) - 1] = '\0';  /* bombproof */
        return safe_errbuf;
    }
    return errstr;
}

/* Note the ASCII-hex output goes to stream identified by 'fp'. This usually
 * be either stdout or stderr.
 * 'no_ascii' allows for 3 output types:
 *     > 0     each line has address then up to 16 ASCII-hex bytes
 *     = 0     in addition, the bytes are listed in ASCII to the right
 *     < 0     only the ASCII-hex bytes are listed (i.e. without address) */
static void
dStrHexFp(const char* str, int len, int no_ascii, FILE * fp)
{
    const char * p = str;
    const char * formatstr;
    unsigned char c;
    char buff[82];
    int a = 0;
    int bpstart = 5;
    const int cpstart = 60;
    int cpos = cpstart;
    int bpos = bpstart;
    int i, k, blen;

    if (len <= 0)
        return;
    blen = (int)sizeof(buff);
    if (0 == no_ascii)  /* address at left and ASCII at right */
        formatstr = "%.76s\n";
    else if (no_ascii > 0)
        formatstr = "%s\n";     /* was: "%.58s\n" */
    else /* negative: no address at left and no ASCII at right */
        formatstr = "%s\n";     /* was: "%.48s\n"; */
    memset(buff, ' ', 80);
    buff[80] = '\0';
    if (no_ascii < 0) {
        bpstart = 0;
        bpos = bpstart;
        for (k = 0; k < len; k++) {
            c = *p++;
            if (bpos == (bpstart + (8 * 3)))
                bpos++;
            scnpr(&buff[bpos], blen - bpos, "%.2x", (int)(unsigned char)c);
            buff[bpos + 2] = ' ';
            if ((k > 0) && (0 == ((k + 1) % 16))) {
                trimTrailingSpaces(buff);
                fprintf(fp, formatstr, buff);
                bpos = bpstart;
                memset(buff, ' ', 80);
            } else
                bpos += 3;
        }
        if (bpos > bpstart) {
            buff[bpos + 2] = '\0';
            trimTrailingSpaces(buff);
            fprintf(fp, "%s\n", buff);
        }
        return;
    }
    /* no_ascii>=0, start each line with address (offset) */
    k = scnpr(buff + 1, blen - 1, "%.2x", a);
    buff[k + 1] = ' ';

    for (i = 0; i < len; i++) {
        c = *p++;
        bpos += 3;
        if (bpos == (bpstart + (9 * 3)))
            bpos++;
        scnpr(&buff[bpos], blen - bpos, "%.2x", (int)(unsigned char)c);
        buff[bpos + 2] = ' ';
        if (no_ascii)
            buff[cpos++] = ' ';
        else {
            if (! my_isprint(c))
                c = '.';
            buff[cpos++] = c;
        }
        if (cpos > (cpstart + 15)) {
            if (no_ascii)
                trimTrailingSpaces(buff);
            fprintf(fp, formatstr, buff);
            bpos = bpstart;
            cpos = cpstart;
            a += 16;
            memset(buff, ' ', 80);
            k = scnpr(buff + 1, blen - 1, "%.2x", a);
            buff[k + 1] = ' ';
        }
    }
    if (cpos > cpstart) {
        buff[cpos] = '\0';
        if (no_ascii)
            trimTrailingSpaces(buff);
        fprintf(fp, "%s\n", buff);
    }
}

void
dStrHex(const char* str, int len, int no_ascii)
{
    dStrHexFp(str, len, no_ascii, stdout);
}

void
dStrHexErr(const char* str, int len, int no_ascii)
{
    dStrHexFp(str, len, no_ascii, stderr);
}

#define DSHS_LINE_BLEN 160
#define DSHS_BPL 16

/* Read 'len' bytes from 'str' and output as ASCII-Hex bytes (space
 * separated) to 'b' not to exceed 'b_len' characters. Each line
 * starts with 'leadin' (NULL for no leadin) and there are 16 bytes
 * per line with an extra space between the 8th and 9th bytes. 'format'
 * is 0 for repeat in printable ASCII ('.' for non printable) to
 * right of each line; 1 don't (so just output ASCII hex). Returns
 * number of bytes written to 'b' excluding the trailing '\0'. */
int
dStrHexStr(const char * str, int len, const char * leadin, int format,
           int b_len, char * b)
{
    unsigned char c;
    int bpstart, bpos, k, n, prior_ascii_len;
    bool want_ascii;
    char buff[DSHS_LINE_BLEN + 2];
    char a[DSHS_BPL + 1];
    const char * p = str;

    if (len <= 0) {
        if (b_len > 0)
            b[0] = '\0';
        return 0;
    }
    if (b_len <= 0)
        return 0;
    want_ascii = !format;
    if (want_ascii) {
        memset(a, ' ', DSHS_BPL);
        a[DSHS_BPL] = '\0';
    }
    if (leadin) {
        bpstart = strlen(leadin);
        /* Cap leadin at (DSHS_LINE_BLEN - 70) characters */
        if (bpstart > (DSHS_LINE_BLEN - 70))
            bpstart = DSHS_LINE_BLEN - 70;
    } else
        bpstart = 0;
    bpos = bpstart;
    prior_ascii_len = bpstart + (DSHS_BPL * 3) + 1;
    n = 0;
    memset(buff, ' ', DSHS_LINE_BLEN);
    buff[DSHS_LINE_BLEN] = '\0';
    if (bpstart > 0)
        memcpy(buff, leadin, bpstart);
    for (k = 0; k < len; k++) {
        c = *p++;
        if (bpos == (bpstart + ((DSHS_BPL / 2) * 3)))
            bpos++;     /* for extra space in middle of each line's hex */
        scnpr(buff + bpos, (int)sizeof(buff) - bpos, "%.2x",
              (int)(unsigned char)c);
        buff[bpos + 2] = ' ';
        if (want_ascii)
            a[k % DSHS_BPL] = my_isprint(c) ? c : '.';
        if ((k > 0) && (0 == ((k + 1) % DSHS_BPL))) {
            trimTrailingSpaces(buff);
            if (want_ascii) {
                n += scnpr(b + n, b_len - n, "%-*s   %s\n", prior_ascii_len,
                           buff, a);
                memset(a, ' ', DSHS_BPL);
            } else
                n += scnpr(b + n, b_len - n, "%s\n", buff);
            if (n >= (b_len - 1))
                return n;
            memset(buff, ' ', DSHS_LINE_BLEN);
            bpos = bpstart;
            if (bpstart > 0)
                memcpy(buff, leadin, bpstart);
        } else
            bpos += 3;
    }
    if (bpos > bpstart) {
        trimTrailingSpaces(buff);
        if (want_ascii)
            n += scnpr(b + n, b_len - n, "%-*s   %s\n", prior_ascii_len,
                       buff, a);
        else
            n += scnpr(b + n, b_len - n, "%s\n", buff);
    }
    return n;
}

void
hex2stdout(const uint8_t * b_str, int len, int no_ascii)
{
    dStrHex((const char *)b_str, len, no_ascii);
}

void
hex2stderr(const uint8_t * b_str, int len, int no_ascii)
{
    dStrHexErr((const char *)b_str, len, no_ascii);
}

int
hex2str(const uint8_t * b_str, int len, const char * leadin, int format,
        int b_len, char * b)
{
    return dStrHexStr((const char *)b_str, len, leadin, format, b_len, b);
}

uint32_t
smp_get_page_size(void)
{
#if defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
    return sysconf(_SC_PAGESIZE); /* POSIX.1 (was getpagesize()) */
#elif defined(SG_LIB_WIN32)
    if (! got_page_size) {
        SYSTEM_INFO si;

        GetSystemInfo(&si);
        win_page_size = si.dwPageSize;
        got_page_size = true;
    }
    return win_page_size;
#elif defined(SG_LIB_FREEBSD)
    return PAGE_SIZE;
#else
    return 4096;     /* give up, pick likely figure */
#endif
}

/* Returns pointer to heap (or NULL) that is aligned to a align_to byte
 * boundary. Sends back *buff_to_free pointer in third argument that may be
 * different from the return value. If it is different then the *buff_to_free
 * pointer should be freed (rather than the returned value) when the heap is
 * no longer needed. If align_to is 0 then aligns to OS's page size. Sets all
 * returned heap to zeros. If num_bytes is 0 then set to page size. */
uint8_t *
smp_memalign(uint32_t num_bytes, uint32_t align_to, uint8_t ** buff_to_free,
             bool vb)
{
    size_t psz;
    uint8_t * res;

    if (buff_to_free)   /* make sure buff_to_free is NULL if alloc fails */
        *buff_to_free = NULL;
    psz = (align_to > 0) ? align_to : smp_get_page_size();
    if (0 == num_bytes)
        num_bytes = psz;        /* ugly to handle otherwise */

#ifdef HAVE_POSIX_MEMALIGN
    {
        int err;
        void * wp = NULL;

        err = posix_memalign(&wp, psz, num_bytes);
        if (err || (NULL == wp)) {
            fprintf(stderr, "%s: posix_memalign: error [%d], out of "
                    "memory?\n", __func__, err);
            return NULL;
        }
        memset(wp, 0, num_bytes);
        if (buff_to_free)
            *buff_to_free = (uint8_t *)wp;
        res = (uint8_t *)wp;
        if (vb) {
            fprintf(stderr, "%s: posix_ma, len=%d, ", __func__, num_bytes);
            if (buff_to_free)
                fprintf(stderr, "wrkBuffp=%p, ", (void *)res);
            fprintf(stderr, "psz=%u, rp=%p\n", (unsigned int)psz,
                    (void *)res);
        }
        return res;
    }
#else
    {
        void * wrkBuff;
        smp_uintptr_t align_1 = psz - 1;

        wrkBuff = (uint8_t *)calloc(num_bytes + psz, 1);
        if (NULL == wrkBuff) {
            if (buff_to_free)
                *buff_to_free = NULL;
            return NULL;
        } else if (buff_to_free)
            *buff_to_free = (uint8_t *)wrkBuff;
        res = (uint8_t *)(void *)
            (((smp_uintptr_t)wrkBuff + align_1) & (~align_1));
        if (vb) {
            fprintf(stderr, "%s: hack, len=%d, ", __func__, num_bytes);
            if (buff_to_free)
                fprintf(stderr, "buff_to_free=%p, ", wrkBuff);
            fprintf(stderr, "align_1=%lu, rp=%p\n", (unsigned long)align_1,
                    (void *)res);
        }
        return res;
    }
#endif
}

bool
smp_is_aligned(const void * pointer, int byte_count)
{
    return 0 == ((smp_uintptr_t)pointer %
                 ((byte_count > 0) ? (uint32_t)byte_count :
                                     smp_get_page_size()));
}

/* Returns true when executed on big endian machine; else returns false.
 * Useful for displaying ATA identify words (which need swapping on a
 * big endian machine). */
bool
smp_is_big_endian()
{
    union u_t {
        uint16_t s;
        unsigned char c[sizeof(uint16_t)];
    } u;

    u.s = 0x0102;
    return (u.c[0] == 0x01);     /* The lowest address contains
                                    the most significant byte */
}

bool
smp_all_zeros(const uint8_t * bp, int b_len)
{
    if ((NULL == bp) || (b_len <= 0))
        return false;
    for (--b_len; b_len >= 0; --b_len) {
        if (0x0 != bp[b_len])
            return false;
    }
    return true;
}

bool
smp_all_ffs(const uint8_t * bp, int b_len)
{
    if ((NULL == bp) || (b_len <= 0))
        return false;
    for (--b_len; b_len >= 0; --b_len) {
        if (0xff != bp[b_len])
            return false;
    }
    return true;
}

/* If the number in 'buf' can be decoded or the multiplier is unknown
 * then -1 is returned. Accepts a hex prefix (0x or 0X) or suffix (h or
 * H) or a decimal multiplier suffix (as per GNU's dd (since 2002: SI and
 * IEC 60027-2)). Main (SI) multipliers supported: K, M, G. */
int
smp_get_num(const char * buf)
{
    int res, num, n, len;
    unsigned int unum;
    const char * cp;
    char c = 'c';
    char c2, c3;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%x", &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%x", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%d%c%c%c", &num, &c, &c2, &c3);
    if (res < 1)
        return -1LL;
    else if (1 == res)
        return num;
    else {
        if (res > 2)
            c2 = toupper(c2);
        if (res > 3)
            c3 = toupper(c3);
        switch (toupper(c)) {
        case ',':
        case 'C':
            return num;
        case 'W':
            return num * 2;
        case 'B':
            return num * 512;
        case 'K':
            if (2 == res)
                return num * 1024;
            if (('B' == c2) || ('D' == c2))
                return num * 1000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1024;
            return -1;
        case 'M':
            if (2 == res)
                return num * 1048576;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1048576;
            return -1;
        case 'G':
            if (2 == res)
                return num * 1073741824;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1073741824;
            return -1;
        case 'X':
            cp = strchr(buf, 'x');
            if (NULL == cp)
                cp = strchr(buf, 'X');
            if (cp) {
                n = smp_get_num(cp + 1);
                if (-1 != n)
                    return num * n;
            }
            return -1;
        default:
            fprintf(stderr, "unrecognized multiplier\n");
            return -1;
        }
    }
}

/* If the number in 'buf' can not be decoded then -1 is returned. Accepts a
 * hex prefix (0x or 0X) or a 'h' (or 'H') suffix; otherwise decimal is
 * assumed. Does not accept multipliers. Accept a comma (","), hyphen ("-"),
 * a whitespace or newline as terminator. */
int
smp_get_num_nomult(const char * buf)
{
    int res, len, num;
    unsigned int unum;
    char * commap;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    len = strlen(buf);
    commap = (char *)strchr(buf + 1, ',');
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%x", &unum);
        num = unum;
    } else if (commap && ('H' == toupper((int)*(commap - 1)))) {
        res = sscanf(buf, "%x", &unum);
        num = unum;
    } else if ((NULL == commap) && ('H' == toupper((int)buf[len - 1]))) {
        res = sscanf(buf, "%x", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%d", &num);
    if (1 == res)
        return num;
    else
        return -1;
}


/* If the number in 'buf' can be decoded or the multiplier is unknown
   then -1LL is returned. Accepts a hex prefix (0x or 0X) or a decimal
   multiplier suffix (as per GNU's dd (since 2002: SI and IEC 60027-2)).
   Main (SI) multipliers supported: K, M, G, T, P. */
int64_t
smp_get_llnum(const char * buf)
{
    int res, len;
    int64_t num, ll;
    uint64_t unum;
    const char * cp;
    char c = 'c';
    char c2, c3;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1LL;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%" SCNx64, &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%" SCNx64, &unum);
        num = unum;
    } else
        res = sscanf(buf, "%" SCNd64 "%c%c%c", &num, &c, &c2, &c3);
    if (res < 1)
        return -1LL;
    else if (1 == res)
        return num;
    else {
        if (res > 2)
            c2 = toupper(c2);
        if (res > 3)
            c3 = toupper(c3);
        switch (toupper(c)) {
        case 'C':
            return num;
        case 'W':
            return num * 2;
        case 'B':
            return num * 512;
        case 'K':
            if (2 == res)
                return num * 1024;
            if (('B' == c2) || ('D' == c2))
                return num * 1000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1024;
            return -1LL;
        case 'M':
            if (2 == res)
                return num * 1048576;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1048576;
            return -1LL;
        case 'G':
            if (2 == res)
                return num * 1073741824;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1073741824;
            return -1LL;
        case 'T':
            if (2 == res)
                return num * 1099511627776LL;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000000000LL;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1099511627776LL;
            return -1LL;
        case 'P':
            if (2 == res)
                return num * 1099511627776LL * 1024;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000000000LL * 1000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1099511627776LL * 1024;
            return -1LL;
        case 'X':
            cp = strchr(buf, 'x');
            if (NULL == cp)
                cp = strchr(buf, 'X');
            if (cp) {
                ll = smp_get_llnum(cp + 1);
                if (-1LL != ll)
                    return num * ll;
            }
            return -1LL;
        default:
            fprintf(stderr, "unrecognized multiplier\n");
            return -1LL;
        }
    }
}

/* If the number in 'buf' can not be decoded then -1 is returned. Accepts a
 * hex prefix (0x or 0X) or a 'h' (or 'H') suffix; otherwise decimal is
 * assumed. Does not accept multipliers. Accept a comma (","), hyphen ("-"),
 * a whitespace or newline as terminator. Only decimal numbers can represent
 * negative numbers and '-1' must be treated separately. */
int64_t
smp_get_llnum_nomult(const char * buf)
{
    int res, len;
    int64_t num;
    uint64_t unum;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%" SCNx64 "", &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%" SCNx64 "", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%" SCNd64 "", &num);
    return (1 == res) ? num : -1;
}

/* If the non-negative number in 'buf' can be decoded in decimal (default)
 * or hex then it is returned, else -1 is returned. Skips leading and
 * trailing spaces, tabs and commas. Hex numbers are indicated by a "0x"
 * or "0X" prefix, or by a 'h' or 'H' suffix. */
int
smp_get_dhnum(const char * buf)
{
    int res, n, len;
    unsigned int unum;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    buf += strspn(buf, " ,\t");
    if (('0' == buf[0]) && ('X' == toupper(buf[1]))) {
        res = sscanf(buf + 2, "%x", &unum);
        return res ? (int)unum : -1;
    }
    len = strcspn(buf, " ,\t");
    if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%x", &unum);
        return res ? (int)unum : -1;
    }
    res = sscanf(buf, "%d", &n);
    return res ? n : -1;
}

/* Want safe, 'n += snprintf(b + n, blen - n, ...)' style sequence of
 * functions. Returns number of chars placed in cp excluding the
 * trailing null char. So for cp_max_len > 0 the return value is always
 * < cp_max_len; for cp_max_len <= 1 the return value is 0 and no chars are
 * written to cp. Note this means that when cp_max_len = 1, this function
 * assumes that cp[0] is the null character and does nothing (and returns
 * 0). Linux kernel has a similar function called  scnprintf(). Public
 * declaration in sg_pr2serr.h header  */
int
sg_scnpr(char * cp, int cp_max_len, const char * fmt, ...)
{
    va_list args;
    int n;

    if (cp_max_len < 2)
        return 0;
    va_start(args, fmt);
    n = vsnprintf(cp, cp_max_len, fmt, args);
    va_end(args);
    return (n < cp_max_len) ? n : (cp_max_len - 1);
}

int
pr2serr(const char * fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vfprintf(stderr, fmt, args);
    va_end(args);
    return n;
}

int
pr2ws(const char * fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vfprintf(stderr, fmt, args);
    va_end(args);
    return n;
}

const char *
smp_lib_version()
{
        return version_str;
}


