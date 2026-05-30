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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stropts.h>

#include <sys/scsi/impl/usmp.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"
#include "smp_unaligned.h"

#define I_USMP 1
#define DEF_USMP_TIMEOUT 60 /* seconds  */

/* Difference between usmp.h header in openSolaris 2009 and usmp-7i.html
 * Oracle (dated 2010) */
#ifdef USMPCMD
#define USMP_IO USMPCMD
#else
#define USMP_IO USMPFUNC
#endif

int
smp_initiator_open(const char * device_name, int subvalue,
                   const char * i_params, uint64_t sa,
                   struct smp_target_obj * tobj, int verbose)
{
    int res;

    if (i_params) { ; }       /* unused, suppress warning */
    if ((NULL == tobj) || (NULL == device_name))
        return -1;
    memset(tobj, 0, sizeof(struct smp_target_obj));
    strncpy(tobj->device_name, device_name, SMP_MAX_DEVICE_NAME);
    if (sa)
        sg_put_unaligned_be64(sa, tobj->sas_addr + 0);
    tobj->interface_selector = I_USMP;
    if (I_USMP == tobj->interface_selector) {
        res = open(tobj->device_name, O_RDWR);
        if (res < 0) {
            perror("smp_initiator_open(usmp): open() failed");
            if (verbose)
                fprintf(stderr, "tried to open %s\n", tobj->device_name);
            return -1;
        }
        tobj->fd = res;
        tobj->subvalue = subvalue;
        tobj->opened = 1;
        return 0;
    } else
        fprintf(stderr, "bad interface selector: %d\n",
                tobj->interface_selector);
    return -1;
}

int
smp_send_req(const struct smp_target_obj * tobj,
             struct smp_req_resp * rresp, int verbose)
{
    struct usmp_cmd urr;

    if ((NULL == tobj) || (0 == tobj->opened)) {
        if (verbose > 2)
            fprintf(stderr, "smp_send_req: nothing open??\n");
        return -1;
    }
    if (I_USMP != tobj->interface_selector) {
        fprintf(stderr, "bad interface selector: %d\n",
                tobj->interface_selector);
        return -1;
    }
    memset(&urr, 0, sizeof(urr));
    urr.usmp_req = rresp->request;
    urr.usmp_reqsize = rresp->request_len; /* header+payload+CRC in bytes */
    urr.usmp_rsp = rresp->response;
    urr.usmp_rspsize = rresp->max_response_len;
    urr.usmp_timeout = DEF_USMP_TIMEOUT;
    if (ioctl(tobj->fd, USMP_IO, &urr) < 0) {
        perror("smp_send_req: ioctl(USMPCMD)");
        return -1;
    }
    rresp->act_response_len = -1;
    rresp->transport_err = 0;
    return 0;
}

int
smp_initiator_close(struct smp_target_obj * tobj)
{
    int res;

    if ((NULL == tobj) || (0 == tobj->opened)) {
        fprintf(stderr, "smp_initiator_close(usmp): nothing open??\n");
        return -1;
    }
    if (I_USMP != tobj->interface_selector) {
        fprintf(stderr, "bad interface selector: %d\n",
                tobj->interface_selector);
        return -1;
    }
    res = close(tobj->fd);
    if (res < 0)
        perror("smp_initiator_close(usmp): failed\n");
    tobj->opened = 0;
    return 0;
}
