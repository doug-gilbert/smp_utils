#ifndef SMP_AACRAID_IO_H
#define SMP_AACRAID_IO_H

#include "smp_lib.h"

/* These functions are the interface to upper level. */

extern int chk_aac_device(const char * dev_name, int verbose);

extern int open_aac_device(const char * dev_name, int verbose);

extern int close_aac_device(int fd);

extern int send_req_aac(int fd, int subvalue, const unsigned char * target_sa,
                        struct smp_req_resp * rresp, int verbose);

#endif
