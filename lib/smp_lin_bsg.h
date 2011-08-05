#ifndef SMP_LIN_BSG_H
#define SMP_LIN_BSG_H

#include "smp_lib.h"

extern int chk_lin_bsg_device(const char * dev_name, int verbose);

extern int open_lin_bsg_device(const char * dev_name, int verbose);

extern int close_lin_bsg_device(int fd);

extern int send_req_lin_bsg(int fd, int subvalue,
                            struct smp_req_resp * rresp, int verbose);

#endif
