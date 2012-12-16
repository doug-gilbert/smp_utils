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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>
// #include <sys/ata.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <fcntl.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "smp_lib.h"

#define I_CAM 1

struct tobj_cam_t {
    struct cam_device * cam_dev;
    char devname[DEV_IDLEN + 1];        /* for cam */
    int unitnum;                        /* for cam */
};


/* Note that CAM (circa FreeBSD 9) cannot directly communicate with a SAS
 * expander because it isn't a SCSI device. FreeBSD assumes each SAS
 * expander is paired with a SES (enclosure) device. This seems to be true
 * for SAS-2 expanders but not the older SAS-1 expanders. Hence device_name
 * will be something like /dev/ses0 . */
int
smp_initiator_open(const char * device_name, int subvalue,
                   const char * i_params, unsigned long long sa,
                   struct smp_target_obj * tobj, int verbose)
{
    int j;
    struct cam_device* cam_dev;
    struct tobj_cam_t * tcp;

    i_params = i_params;
    if ((NULL == tobj) || (NULL == device_name))
        return -1;
    memset(tobj, 0, sizeof(struct smp_target_obj));
    strncpy(tobj->device_name, device_name, SMP_MAX_DEVICE_NAME);
    if (sa) {
        for (j = 0; j < 8; ++j, (sa >>= 8))
            tobj->sas_addr[j] = (sa & 0xff);
    }
    tobj->interface_selector = I_CAM;
    tcp = (struct tobj_cam_t *)
                calloc(1, sizeof(struct tobj_cam_t));
    if (tcp == NULL) {
        // errno already set by call to calloc()
        return -1;
    }
    if (cam_get_device(device_name, tcp->devname, DEV_IDLEN,
                       &(tcp->unitnum)) == -1) {
        if (verbose)
            fprintf(stderr, "bad device name structure\n");
        free(tcp);
        return -1;
    }
    if (! (cam_dev = cam_open_spec_device(tcp->devname, tcp->unitnum,
                                          O_RDWR, NULL))) {
        fprintf(stderr, "cam_open_spec_device: %s\n", cam_errbuf);
        free(tcp);
        return -1;
    }
    tcp->cam_dev = cam_dev;
    tobj->vp = tcp;
    tobj->opened = 1;
    tobj->subvalue = subvalue;  /* unused */
    return 0;
}

int
smp_send_req(const struct smp_target_obj * tobj, struct smp_req_resp * rresp,
             int verbose)
{
    union ccb *ccb;
    struct tobj_cam_t * tcp;
    int retval, emsk;
    int flags = 0;

    if ((NULL == tobj) || (0 == tobj->opened) || (NULL == tobj->vp)) {
        if (verbose)
            fprintf(stderr, "smp_send_req: nothing open??\n");
        return -1;
    }
    if (I_CAM != tobj->interface_selector) {
        fprintf(stderr, "smp_send_req: unknown transport [%d]\n",
                tobj->interface_selector);
        return -1;
    }
    tcp = (struct tobj_cam_t *)tobj->vp;
    if (! (ccb = cam_getccb(tcp->cam_dev))) {
        fprintf(stderr, "cam_getccb: failed\n");
        return -1;
    }

    // clear out structure, except for header that was filled in for us
    bzero(&(&ccb->ccb_h)[1],
            sizeof(union ccb) - sizeof(struct ccb_hdr));

    flags |= CAM_DEV_QFRZDIS;
    /* CAM does not want request_len including CRC */
    cam_fill_smpio(&ccb->smpio,
                   /*retries*/ 2,       /* guess */
                   /*cbfcnp*/ NULL,
                   /*flags*/ flags,
                   /*smp_request*/ rresp->request,
                   /*smp_request_len*/ rresp->request_len - 4,
                   /*smp_response*/ rresp->response,
                   /*smp_response_len*/ rresp->max_response_len,
                   /*timeout*/ 5000);   /* milliseconds ? */

    ccb->smpio.flags = SMP_FLAG_NONE;

    emsk = 0;
    if (((retval = cam_send_ccb(tcp->cam_dev, ccb)) < 0) ||
        ((((emsk = (ccb->ccb_h.status & CAM_STATUS_MASK))) != CAM_REQ_CMP) &&
         (emsk != CAM_SMP_STATUS_ERROR))) {
        cam_error_print(tcp->cam_dev, ccb, CAM_ESF_ALL, CAM_EPF_ALL, stderr);
        cam_freeccb(ccb);
        return -1;
    }
    if (((emsk == CAM_REQ_CMP) || (emsk == CAM_SMP_STATUS_ERROR)) &&
        (rresp->max_response_len > 0)) {
        if ((emsk == CAM_SMP_STATUS_ERROR) && (verbose > 3))
            cam_error_print(tcp->cam_dev, ccb, CAM_ESF_ALL, CAM_EPF_ALL,
                            stderr);
        rresp->act_response_len = -1;
        cam_freeccb(ccb);
        return 0;
    } else {
        fprintf(stderr, "smp_send_req(cam): not sure how it got here\n");
        cam_freeccb(ccb);
        return emsk ? emsk : -1;
    }
}

int
smp_initiator_close(struct smp_target_obj * tobj)
{
    struct tobj_cam_t * tcp;

    if ((NULL == tobj) || (0 == tobj->opened)) {
        fprintf(stderr, "smp_initiator_close: nothing open??\n");
        return -1;
    }
    if (tobj->vp) {
        tcp = (struct tobj_cam_t *)tobj->vp;
        cam_close_device(tcp->cam_dev);
        free(tobj->vp);
        tobj->vp = NULL;
    }
    tobj->opened = 0;
    return 0;
}
