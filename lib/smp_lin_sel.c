/*
 * Copyright (c) 2006-2026, Douglas Gilbert
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"
#include "smp_unaligned.h"

#include "smp_aac_io.h"
#include "smp_mptctl_io.h"
#include "smp_lin_bsg.h"


#define I_MPT 2
#define I_SGV4 4
#define I_AAC  6

int
smp_initiator_open(const char * device_name, int subvalue,
                   const char * i_params, uint64_t sa,
                   struct smp_target_obj * tobj, int verbose)
{
    int force = 0;
    int res;
    int len = device_name ? (strlen(device_name) + 1) : 0;
    char * cp;

    if ((NULL == tobj) || (NULL == device_name))
        return -1;
    memset(tobj, 0, sizeof(struct smp_target_obj));
    memcpy(tobj->device_name, device_name,
           ((len > SMP_MAX_DEVICE_NAME) ? SMP_MAX_DEVICE_NAME : len));
    if (sa) {
        tobj->sas_addr64 = sa;
        sg_put_unaligned_be64(sa, tobj->sas_addr + 0);
    }
    if (i_params[0]) {
        if (0 == strncmp("aac", i_params, 3))
            tobj->interface_selector = I_AAC;
        else if(0 == strncmp("mpt",i_params,3))
            tobj->interface_selector = I_MPT;
        else if ((0 == strncmp("sgv4", i_params, 2)) ||
                 (0 == strncmp("bsg", i_params, 3)))
            tobj->interface_selector = I_SGV4;
        else if (0 == strncmp("for", i_params, 3))
            force = 1;
        else if (verbose > 3)
            fprintf(stderr, "smp_initiator_open: interface not recognized\n");
        cp = (char *)strchr(i_params, ','); /* cast to stop C++ error */
        if (cp) {
            if ((tobj->interface_selector > 0) &&
                (0 == strncmp("for", cp + 1, 3)))
                force = 1;
        }
    }
    if ((I_SGV4 == tobj->interface_selector) ||
        (0 == tobj->interface_selector)) {
        res = chk_lin_bsg_device(device_name, verbose);
        if (res || force) {
            if (0 == tobj->interface_selector)
                tobj->interface_selector = I_SGV4;
            if ((0 == res) && force)
                fprintf(stderr, "... overriding failed check due "
                        "to 'force'\n");
            res = open_lin_bsg_device(device_name, verbose);
            if (res < 0)
                goto err_out;
            tobj->fd = res;
            tobj->subvalue = subvalue;
            tobj->opened = 1;
            return 0;
        } else if (verbose > 2)
            fprintf(stderr, "chk_lin_bsg_device: failed\n");
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

    if((I_AAC == tobj->interface_selector) ||
      (0 == tobj->interface_selector)) {
       res = chk_aac_device(device_name,verbose);
       if(res || force) {
           if (0 == tobj->interface_selector)
               tobj->interface_selector = I_AAC;
           if ((0 == res) && force)
               fprintf(stderr,"... overriding failed check due"
                       "to 'force' \n");
           res = open_aac_device(device_name,verbose);
           if (res < 0)
               goto err_out;
           tobj->fd = res;
           tobj->subvalue = subvalue;
           tobj->opened  = 1;
           return 0;
        } else if (verbose > 2)
            fprintf(stderr,"smp_initiator_open: chk_aac_device failed\n");
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
        return send_req_lin_bsg(tobj->fd, tobj->subvalue, rresp, verbose);
    else if (I_MPT == tobj->interface_selector)
        return send_req_mpt(tobj->fd, tobj->subvalue, tobj->sas_addr64,
                            rresp, verbose);
    else if (I_AAC == tobj->interface_selector)
        return send_req_aac(tobj->fd, tobj->subvalue, tobj->sas_addr,
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
    }else if(I_AAC == tobj->interface_selector){
        res = close_aac_device(tobj->fd);
        if (res < 0)
            fprintf(stderr,"close_aac_device: failed\n");
    }


    tobj->opened = 0;
    return 0;
}
