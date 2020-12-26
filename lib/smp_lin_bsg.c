/*
 * Copyright (c) 2011-2020, Douglas Gilbert
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>
//#include <curses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
//#include <scsi/scsi.h>
#include <sys/sysmacros.h>
#ifndef major
#include <sys/types.h>
#endif
#include <scsi/sg.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "smp_lin_bsg.h"

#if defined(IGNORE_LINUX_BSG) || ! defined(HAVE_LINUX_BSG_H)

/* Returns 1 if bsg dev_name else 0 . */
int
chk_lin_bsg_device(const char * dev_name, int verbose)
{
    dev_name = dev_name;
    verbose = verbose;
    return 0;
}

/* Returns open file descriptor to dev_name bsg device or -1 */
int
open_lin_bsg_device(const char * dev_name, int verbose)
{
    dev_name = dev_name;
    verbose = verbose;
    return -1;
}

int
close_lin_bsg_device(int fd)
{
    fd = fd;
    return 0;
}

/* Returns 0 on success else -1 . */
int
send_req_lin_bsg(int fd, int subvalue, struct smp_req_resp * rresp,
                 int verbose)
{
    fd = fd;
    subvalue = subvalue;
    rresp = rresp;
    verbose = verbose;
    return -1;
}


# else  /* have <linux/bsg.h> and want to use it */

#include <linux/bsg.h>

#define DEF_TIMEOUT_MS 20000    /* 20 seconds */


/* Returns 1 if bsg dev_name else 0 . */
int
chk_lin_bsg_device(const char * dev_name, int verbose)
{
    char buff[1024];
    char sysfs_nm[1024];
    char * cp;
    struct stat st;
    int len;

    if (strlen(dev_name) > sizeof(buff)) {
        fprintf(stderr, "device name too long (greater than %d bytes)\n",
                (int)sizeof(buff));
        return 0;
    }
    len = 0;
    if ('/' != dev_name[0]) {
        if (getcwd(buff, sizeof(buff) - 1)) {
            len = strlen(buff);
            if ('/' != buff[len - 1])
                buff[len++] = '/';
        } else {
            if (verbose > 3)
                perror("chk_lin_bsg_device: getcwd failed");
            return 0;
        }
        strncpy(buff + len, dev_name, sizeof(buff) - len);
    } else
        strncpy(buff, dev_name, sizeof(buff));
    buff[sizeof(buff) - 1] = '\0';
    if ('/' == buff[strlen(buff) - 1])
        buff[strlen(buff) - 1] = '\0';
    if (0 == strncmp(buff, "/sys/", 5)) {
        if (strstr(buff, "/bsg/")) {
            if (stat(buff, &st) < 0) {
                if (verbose > 3) {
                    fprintf(stderr, "chk_lin_bsg_device: stat() on %s "
                            "failed: ", buff);
                    perror("");
                }
                return 0;
            }
            return 1;
        } else
            return 0;
    }
    if (0 == strncmp(buff, "/dev/", 5)) {
        cp = strrchr(buff, '/');
        snprintf(sysfs_nm, sizeof(sysfs_nm), "/sys/class/bsg/%s/dev", cp + 1);
        if (stat(sysfs_nm, &st) < 0) {
            if (verbose > 3) {
                fprintf(stderr, "chk_lin_bsg_device: stat() on redirected %s "
                        "failed: ", sysfs_nm);
                perror("");
            }
            return 0;
        }
        return 1;
    }
    return 0;
}

/* Returns open file descriptor to dev_name bsg device or -1 */
int
open_lin_bsg_device(const char * dev_name, int verbose)
{
    char buff[1024];
    char sysfs_nm[1024 + 8];
    int len, res, maj, min;
    int ret = -1;
    FILE * fp = NULL;
    struct timeval t;

    if (strlen(dev_name) > sizeof(buff)) {
        fprintf(stderr, "device name too long (greater than %d bytes)\n",
                (int)sizeof(buff));
        return 0;
    }
    len = 0;
    if ('/' != dev_name[0]) {
        if (getcwd(buff, sizeof(buff) - 1)) {
            len = strlen(buff);
            if ('/' != buff[len - 1])
                buff[len++] = '/';
        } else {
            if (verbose)
                perror("open_lin_bsg_device: getcwd failed");
            return 0;
        }
        strncpy(buff + len, dev_name, sizeof(buff) - len);
    } else
        strncpy(buff, dev_name, sizeof(buff));
    buff[sizeof(buff) - 1] = '\0';
    if ('/' == buff[strlen(buff) - 1])
        buff[strlen(buff) - 1] = '\0';
    if (0 == strncmp(buff, "/sys/", 5)) {
        snprintf(sysfs_nm, sizeof(sysfs_nm), "%s/dev", buff);

        fp = fopen(sysfs_nm, "r");
        if (! fp) {
            if (verbose)
                perror("open_lin_bsg_device: fopen() in sysfs failed");
            return -1;
        }
        if (! fgets(buff, sizeof(buff), fp)) {
            if (verbose)
                perror("open_lin_bsg_device: fgets() in sysfs failed");
            goto close_sysfs;
        }
        if (2 != sscanf(buff, "%d:%d", &maj, &min)) {
            if (verbose)
                perror("open_lin_bsg_device: fclose() in sysfs failed");
            goto close_sysfs;
        }
        res = gettimeofday(&t, NULL);
        if (res) {
            if (verbose)
                perror("open_lin_bsg_device: gettimeofday() failed");
            goto close_sysfs;
        }
        memset(buff, 0, sizeof(buff));
        snprintf(buff, sizeof(buff), "/tmp/bsg_%lx%lx", t.tv_sec, t.tv_usec);
        if (verbose > 2)
            fprintf(stderr, "about to make temporary device node at %s\n"
                    "\tfor char device maj:%d min:%d\n", buff, maj, min);
        res = mknod(buff, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
                    makedev(maj, min));
        if (res) {
            if (verbose)
                perror("open_lin_bsg_device: mknod() failed");
            goto close_sysfs;
        }
        ret = open(buff, O_RDWR);
        if (ret < 0) {
            if (verbose) {
                perror("open_lin_bsg_device: open() temporary device node "
                       "failed");
                fprintf(stderr, "\t\ttried to open %s\n", buff);
            }
            goto close_sysfs;
        }
        unlink(buff);
        /* should actually disappear when process dies or closes 'ret' */
    } else {
        ret = open(buff, O_RDWR);
        if (ret < 0) {
            if (verbose) {
                perror("open_lin_bsg_device: open() device node failed");
                fprintf(stderr, "\t\ttried to open %s\n", buff);
            }
            goto close_sysfs;
        }
        return ret;
    }

close_sysfs:
    if (fp)
        fclose(fp);
    return ret;
}

int
close_lin_bsg_device(int fd)
{
    return close(fd);
}

/* Returns 0 on success else -1 . */
int
send_req_lin_bsg(int fd, int subvalue, struct smp_req_resp * rresp,
                 int verbose)
{
    struct sg_io_v4 hdr;
    unsigned char cmd[16];      /* unused */
    int res;

    ++subvalue; /* suppress warning */

    memset(&hdr, 0, sizeof(hdr));
    memset(cmd, 0, sizeof(cmd));

    hdr.guard = 'Q';
    hdr.protocol = BSG_PROTOCOL_SCSI;
    hdr.subprotocol = BSG_SUB_PROTOCOL_SCSI_TRANSPORT;

    hdr.request_len = sizeof(cmd);      /* unused */
    hdr.request = (uintptr_t) cmd;

    hdr.dout_xfer_len = rresp->request_len;
    hdr.dout_xferp = (uintptr_t) rresp->request;

    hdr.din_xfer_len = rresp->max_response_len;
    hdr.din_xferp = (uintptr_t) rresp->response;

    hdr.timeout = DEF_TIMEOUT_MS;

    if (verbose > 3)
        fprintf(stderr, "send_req_lin_bsg: dout_xfer_len=%u, din_xfer_len="
                "%u, timeout=%u ms\n", hdr.dout_xfer_len, hdr.din_xfer_len,
                hdr.timeout);

    res = ioctl(fd, SG_IO, &hdr);
    if (res) {
        perror("send_req_lin_bsg: SG_IO ioctl");
        return -1;
    }
    res = hdr.din_xfer_len - hdr.din_resid;
    rresp->act_response_len = res;
    /* was: rresp->act_response_len = -1; */
    if (verbose > 3) {
        fprintf(stderr, "send_req_lin_bsg: driver_status=%u, transport_status="
                "%u\n", hdr.driver_status, hdr.transport_status);
        fprintf(stderr, "    device_status=%u, duration=%u, info=%u\n",
                hdr.device_status, hdr.duration, hdr.info);
        fprintf(stderr, "    din_resid=%d, dout_resid=%d\n",
                hdr.din_resid, hdr.dout_resid);
        fprintf(stderr, "  smp_req_resp::max_response_len=%d  "
                "act_response_len=%d\n", rresp->max_response_len, res);
        if ((verbose > 4) && (hdr.din_xfer_len > 0)) {
            fprintf(stderr, "  response (din_resid might exclude CRC):\n");
            hex2stdout(rresp->response,
                       (res > 0) ? res : (int)hdr.din_xfer_len, 1);
        }
    }
    if (hdr.driver_status)
        rresp->transport_err = hdr.driver_status;
    else if (hdr.transport_status)
        rresp->transport_err = hdr.transport_status;
    else if (hdr.device_status)
        rresp->transport_err = hdr.device_status;
    return 0;
}

#endif
