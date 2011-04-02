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


#ifdef SMP_UTILS_LINUX

#include "mpt/smp_mptctl_io.h"
#include "sgv4/smp_sgv4_io.h"


#define I_MPT 2
#define I_SGV4 4

int
smp_initiator_open(const char * device_name, int subvalue,
                   const char * i_params, unsigned long long sa,
                   struct smp_target_obj * tobj, int verbose)
{
    int force = 0;
    char * cp;
    int res, j;

    if ((NULL == tobj) || (NULL == device_name))
        return -1;
    memset(tobj, 0, sizeof(struct smp_target_obj));
    strncpy(tobj->device_name, device_name, SMP_MAX_DEVICE_NAME);
    if (sa) {
        for (j = 0; j < 8; ++j, (sa >>= 8))
            tobj->sas_addr[j] = (sa & 0xff);
#if 0
        if (verbose > 0) {
            fprintf(stderr, "    given SAS address: 0x");
            for (j = 0; j < 8; ++j)
                fprintf(stderr, "%02x", tobj->sas_addr[7 - j]);
            fprintf(stderr, "\n");
        }
#endif
    }
    if (i_params[0]) {
        if (0 == strncmp("mpt", i_params, 3))
            tobj->interface_selector = I_MPT;
        else if (0 == strncmp("sgv4", i_params, 2))
            tobj->interface_selector = I_SGV4;
        else if (0 == strncmp("for", i_params, 3))
            force = 1;
        else if (verbose > 3)
            fprintf(stderr, "smp_initiator_open: interface not recognized\n");
        cp = strchr(i_params, ',');
        if (cp) {
            if ((tobj->interface_selector > 0) &&
                (0 == strncmp("for", cp + 1, 3)))
                force = 1;
        }
    }
    if ((I_SGV4 == tobj->interface_selector) ||
        (0 == tobj->interface_selector)) { 
        res = chk_sgv4_device(device_name, verbose);
        if (res || force) {
            if (0 == tobj->interface_selector)
                tobj->interface_selector = I_SGV4;
            if ((0 == res) && force)
                fprintf(stderr, "... overriding failed check due "
                        "to 'force'\n");
            res = open_sgv4_device(device_name, verbose);
            if (res < 0)
                goto err_out;
            tobj->fd = res;
            tobj->subvalue = subvalue;
            tobj->opened = 1;
            return 0;
        } else if (verbose > 2)
            fprintf(stderr, "chk_sgv4_device: failed\n");
    }
    if ((I_MPT == tobj->interface_selector) ||
        (0 == tobj->interface_selector)) { 
        res = chk_mpt_device(device_name, verbose);
        if (res || force) {
            if (0 == tobj->interface_selector)
                tobj->interface_selector = I_MPT;
            if ((0 == res) && force)
                fprintf(stderr, "... overriding failed check due "
                        "to 'force'\n");
            res = open_mpt_device(device_name, verbose);
            if (res < 0)
                goto err_out;
            tobj->fd = res;
            tobj->subvalue = subvalue;
            tobj->opened = 1;
            return 0;
        } else if (verbose > 2)
            fprintf(stderr, "smp_initiator_open: chk_mpt_device failed\n");
    }
err_out:
    fprintf(stderr, "smp_initiator_open: failed to open %s\n", device_name);
    return -1;
}

int
smp_send_req(const struct smp_target_obj * tobj,
             struct smp_req_resp * rresp, int verbose)
{
    if ((NULL == tobj) || (0 == tobj->opened)) {
        if (verbose > 2)
            fprintf(stderr, "smp_send_req: nothing open??\n");
        return -1;
    }
    if (I_SGV4 == tobj->interface_selector)
        return send_req_sgv4(tobj->fd, tobj->subvalue, rresp, verbose);
    else if (I_MPT == tobj->interface_selector)
        return send_req_mpt(tobj->fd, tobj->subvalue, tobj->sas_addr,
                            rresp, verbose);
    else {
        if (verbose)
            fprintf(stderr, "smp_send_req: no transport??\n");
        return -1;
    }
}

int
smp_initiator_close(struct smp_target_obj * tobj)
{
    int res;

    if ((NULL == tobj) || (0 == tobj->opened)) {
        fprintf(stderr, "smp_initiator_close: nothing open??\n");
        return -1;
    }
    if (I_MPT == tobj->interface_selector) {
        res = close_mpt_device(tobj->fd);
        if (res < 0)
            fprintf(stderr, "close_mpt_device: failed\n");
    }
    tobj->opened = 0;
    return 0;
}

#endif


#ifdef SMP_UTILS_FREEBSD

#include "mpt/smp_mptctl_io.h"


#define I_MPT 2

int
smp_initiator_open(const char * device_name, int subvalue,
                   const char * i_params, unsigned long long sa,
                   struct smp_target_obj * tobj, int verbose)
{
    int force = 0;
    char * cp;
    int res, j;

    if ((NULL == tobj) || (NULL == device_name))
        return -1;
    memset(tobj, 0, sizeof(struct smp_target_obj));
    strncpy(tobj->device_name, device_name, SMP_MAX_DEVICE_NAME);
    if (sa) {
        for (j = 0; j < 8; ++j, (sa >>= 8))
            tobj->sas_addr[j] = (sa & 0xff);
#if 0
        if (verbose > 3) {
            fprintf(stderr, "    given SAS address: 0x");
            for (j = 0; j < 8; ++j)
                fprintf(stderr, "%02x", tobj->sas_addr[7 - j]);
            fprintf(stderr, "\n");
        }
#endif
    }
    if (i_params[0]) {
        if (0 == strncmp("mpt", i_params, 3))
            tobj->interface_selector = I_MPT;
        else if (0 == strncmp("for", i_params, 3))
            force = 1;
        else if (verbose > 3)
            fprintf(stderr, "chk_mpt_device: interface not recognized\n");
        cp = strchr(i_params, ',');
        if (cp) {
            if ((tobj->interface_selector > 0) &&
                (0 == strncmp("for", cp + 1, 3)))
                force = 1;
        }
    }
    if ((I_MPT == tobj->interface_selector) ||
        (0 == tobj->interface_selector)) { 
        res = chk_mpt_device(device_name, verbose);
        if (res || force) {
            if (0 == tobj->interface_selector)
                tobj->interface_selector = I_MPT;
            if ((0 == res) && force)
                fprintf(stderr, "... overriding failed check due "
                        "to 'force'\n");
            res = open_mpt_device(device_name, verbose);
            if (res < 0)
                goto err_out;
            tobj->fd = res;
            tobj->subvalue = subvalue;
            tobj->opened = 1;
            return 0;
        } else if (verbose > 2)
            fprintf(stderr, "chk_mpt_device: failed\n");
    }
err_out:
    fprintf(stderr, "smp_initiator_open: failed to open %s\n", device_name);
    return -1;
}

int
smp_send_req(const struct smp_target_obj * tobj, struct smp_req_resp * rresp,
             int verbose)
{
    if ((NULL == tobj) || (0 == tobj->opened)) {
        if (verbose)
            fprintf(stderr, "smp_send_req: nothing open??\n");
        return -1;
    }
    if (I_MPT == tobj->interface_selector) {
        return send_req_mpt(tobj->fd, tobj->subvalue, tobj->sas_addr,
                            rresp, verbose);
    } else {
        if (verbose)
            fprintf(stderr, "smp_send_req: no transport??\n");
        return -1;
    }
}

int
smp_initiator_close(struct smp_target_obj * tobj)
{
    int res;

    if ((NULL == tobj) || (0 == tobj->opened)) {
        fprintf(stderr, "smp_initiator_close: nothing open??\n");
        return -1;
    }
    if (I_MPT == tobj->interface_selector) {
        res = close_mpt_device(tobj->fd);
        if (res < 0)
            fprintf(stderr, "close_mpt_device: failed\n");
    }
    tobj->opened = 0;
    return 0;
}

#endif
