/*
 * Serial Attached SCSI (SAS) Expander communication user space program
 *
 * Copyright (C) 2005 Adaptec, Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Luben Tuikov
 *
 * This file is licensed under GPLv2.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id: //depot/sas-class/expander_conf.c#8 $
 */

/* This is a simple program to show how to communicate with
 * expander devices in a SAS domain.
 *
 * The process is simple:
 * 1. Build the SMP frame you want to send. The format and layout
 *    is described in the SAS spec.  Leave the CRC field equal 0.
 * 2. Open the expander's SMP portal sysfs file in RW mode.
 * 3. Write the frame you built in 1.
 * 4. Read the amount of data you expect to receive for the frame you built.
 *    If you receive different amount of data you expected to receive,
 *    then there was some kind of error.
 * All this process is shown in detail in the function do_smp_func()
 * and its callers, below.
 *
 * Modified by Douglas Gilbert for smp_utils 20060526
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

/* If this is really a 'smp_portal' file (that is ready for a request)
   then reading 4 bytes from it should yield a EINVAL error. This is
   just to be careful that the user doesn't do something like
   'smp_rep_general /dev/sda' which would overwrite the boot block
   (i.e. not good). Returns 1 if looks ok, else returns 0. */
int chk_smp_portal_file(const char * smp_portal_name, int verbose)
{
        int fd;
        ssize_t res;
        unsigned char dummy[4];

        fd = open(smp_portal_name, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
                if (verbose > 2) {
                        fprintf(stderr, "chk_smp_portal_file: "
                                "open(O_RDWR|O_NONBLOCK) failed, errno=%d\n",
                                errno);
                        fprintf(stderr, " smp_portal name: %s\n",
                                smp_portal_name);
                }
                return 0;
        }
        res = read(fd, dummy, sizeof(dummy));
        if ((-1 == res) && (EINVAL == errno)) {
                if (verbose > 3)
                        fprintf(stderr, "chk_smp_portal_file: looks good\n");
                res = 1;          /* this is indeed SAS stack's smp portal */
        } else {
                if (verbose > 2) {
                        fprintf(stderr, "chk_smp_portal_file: read %d bytes "
                                "(errno=%d)\n", ((res < 0) ? 0 : res),
                                ((res < 0) ? errno : 0));
                }
                res = 0;
        }
        close(fd);
        return res;
}

/* Returns the number of bytes read (i.e. response) or yields -1 if
   error. */
int do_smp_portal_func(const char * smp_portal_name, void *smp_req,
                       int smp_req_size, void *smp_resp, int smp_resp_size,
                       int verbose)
{
        int fd;
        ssize_t res;

        fd = open(smp_portal_name, O_RDWR);
        if (fd == -1) {
                if (verbose)
                        fprintf(stderr, "%s: opening %s: %s(%d)\n", __FUNCTION__,
                                smp_portal_name, strerror(errno), errno);
                return fd;
        }
        res = write(fd, smp_req, smp_req_size);
        if (!res) {
                if (verbose)
                        fprintf(stderr, "%s: nothing could be written to: "
                                "%s\n", __FUNCTION__, smp_portal_name);
                goto out_err;
        } else if (res == -1) {
                if (verbose)
                        fprintf(stderr, "%s: writing to %s: %s(%d)\n", __FUNCTION__,
                                smp_portal_name, strerror(errno), errno);
                goto out_err;
        }

        res = read(fd, smp_resp, smp_resp_size);
        if (!res) {
                if (verbose)
                        fprintf(stderr, "%s: nothing could be read from %s\n",
                                __FUNCTION__, smp_portal_name);
                goto out_err;
        } else if (res == -1) {
                if (verbose)
                        fprintf(stderr, "%s: reading from %s: %s(%d)\n", __FUNCTION__,
                                smp_portal_name, strerror(errno), errno);
                goto out_err;
        }
        close(fd);
        return res;
 out_err:
        close(fd);
        return -1;
}

