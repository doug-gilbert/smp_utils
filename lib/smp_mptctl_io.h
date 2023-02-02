#ifndef SMP_MPTCTL_IO_H
#define SMP_MPTCTL_IO_H

#include "smp_lib.h"

/* These functions are the interface to upper level. */

extern int chk_mpt_device(const char * dev_name, int verbose);

extern int open_mpt_device(const char * dev_name, int verbose);

extern int close_mpt_device(int fd);

extern int send_req_mpt(int fd, int subvalue, uint64_t target_sa,
                        struct smp_req_resp * rresp, int verbose);

#endif
