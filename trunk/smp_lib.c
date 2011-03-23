/*
 * Copyright (c) 2006-2011 Douglas Gilbert.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "smp_lib.h"


static char * version_str = "1.17 20110322";    /* spl-2 rev 0 */

/* The original SMP definition (sas-r05.pdf) didn't have request
   and response length fields (they were reserved single byte fields).
   So if zero is found in either field the original lengths
   apply (as defined in the sas-r05.pdf document).
   The unit of 'def_req_len' and 'def_resp_len' is dwords (4 bytes)
   calculated by: ((len_bytes - 8) / 4) where 'len_bytes' includes
   the 4 byte CRC at the end of each frame. The 4 byte CRC field
   does not need to be set (just space allocated (for some pass
   throughs)). */
struct smp_func_def_rrlen {
    int func;           /* '-1' for last entry */
    int def_req_len;    /* if zero in request length field use this, unless */
                        /*  -2 -> no default; -3 -> implicit length */
    int def_resp_len;   /* if zero in response length field use this, unless */
                        /*  -2 -> no default; -3 -> implicit length */
    /* N.B. Some recent functions have 0 length request or reponse lengths.
            This is noted by putting 0 in one of the two above fields. */
};

struct smp_func_def_rrlen smp_def_rrlen_arr[] = {
    /* in numerical order by 'func' */
    {SMP_FN_REPORT_GENERAL, 0, 6},
    {SMP_FN_REPORT_MANUFACTURER, 0, 14},
    {SMP_FN_READ_GPIO_REG, -3, -3}, /* obsolete, not applicable: SFF-8485 */
    {SMP_FN_REPORT_SELF_CONFIG, -2, -2},
    {SMP_FN_REPORT_ZONE_PERMISSION_TBL, -2, -2},/* variable length response */
    {SMP_FN_REPORT_ZONE_MANAGER_PASS, -2, -2},
    {SMP_FN_REPORT_BROADCAST, -2, -2},
    {SMP_FN_READ_GPIO_REG_ENH, -3, -3}, /* not applicable: SFF-8485 */
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
    {SMP_FN_WRITE_GPIO_REG_ENH, -3, -3}, /* not applicable: SFF-8485 */
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
    {SMP_FRES_DIS_PASSWORD_NOT_SUPPORTED, "Disable password not supported"},
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

int smp_is_naa5(unsigned long long addr)
{
    return (0x5 == ((addr >> 60) & 0xf));
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

/* Note the ASCII-hex output goes to stdout.
   'no_ascii' allows for 3 output types:
       > 0     each line has address then up to 16 ASCII-hex bytes
       = 0     in addition, the bytes are listed in ASCII to the right
       < 0     only the ASCII-hex bytes are listed (i.e. without address) */
void
dStrHex(const char* str, int len, int no_ascii)
{
    const char* p = str;
    unsigned char c;
    char buff[82];
    int a = 0;
    const int bpstart = 5;
    const int cpstart = 60;
    int cpos = cpstart;
    int bpos = bpstart;
    int i, k;

    if (len <= 0)
        return;
    memset(buff, ' ', 80);
    buff[80] = '\0';
    if (no_ascii < 0) {
        for (k = 0; k < len; k++) {
            c = *p++;
            bpos += 3;
            if (bpos == (bpstart + (9 * 3)))
                bpos++;
            sprintf(&buff[bpos], "%.2x", (int)(unsigned char)c);
            buff[bpos + 2] = ' ';
            if ((k > 0) && (0 == ((k + 1) % 16))) {
                printf("%.60s\n", buff);
                bpos = bpstart;
                memset(buff, ' ', 80);
            }
        }
        if (bpos > bpstart)
            printf("%.60s\n", buff);
        return;
    }
    /* no_ascii>=0, start each line with address (offset) */
    k = sprintf(buff + 1, "%.2x", a);
    buff[k + 1] = ' ';

    for (i = 0; i < len; i++) {
        c = *p++;
        bpos += 3;
        if (bpos == (bpstart + (9 * 3)))
            bpos++;
        sprintf(&buff[bpos], "%.2x", (int)(unsigned char)c);
        buff[bpos + 2] = ' ';
        if (no_ascii)
            buff[cpos++] = ' ';
        else {
            if ((c < ' ') || (c >= 0x7f))
                c = '.';
            buff[cpos++] = c;
        }
        if (cpos > (cpstart + 15)) {
            printf("%.76s\n", buff);
            bpos = bpstart;
            cpos = cpstart;
            a += 16;
            memset(buff, ' ', 80);
            k = sprintf(buff + 1, "%.2x", a);
            buff[k + 1] = ' ';
        }
    }
    if (cpos > cpstart)
        printf("%.76s\n", buff);
}

/* If the number in 'buf' can be decoded or the multiplier is unknown
   then -1 is returned. Accepts a hex prefix (0x or 0X) or a decimal
   multiplier suffix (as per GNU's dd (since 2002: SI and IEC 60027-2)).
   Main (SI) multipliers supported: K, M, G. */
int
smp_get_num(const char * buf)
{
    int res, num, n, len;
    unsigned int unum;
    char * cp;
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

/* If the number in 'buf' can be decoded or the multiplier is unknown
   then -1LL is returned. Accepts a hex prefix (0x or 0X) or a decimal
   multiplier suffix (as per GNU's dd (since 2002: SI and IEC 60027-2)).
   Main (SI) multipliers supported: K, M, G, T, P. */
long long
smp_get_llnum(const char * buf)
{
    int res, len;
    long long num, ll;
    unsigned long long unum;
    char * cp;
    char c = 'c';
    char c2, c3;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1LL;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%llx", &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%llx", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%lld%c%c%c", &num, &c, &c2, &c3);
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

const char *
smp_lib_version()
{
        return version_str;
}
