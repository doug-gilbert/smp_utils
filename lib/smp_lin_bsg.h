#ifndef SMP_LIN_BSG_H
#define SMP_LIN_BSG_H

#include "smp_lib.h"

int chk_lin_bsg_device(const char * dev_name, int verbose);

int open_lin_bsg_device(const char * dev_name, int verbose);

int close_lin_bsg_device(int fd);

int send_req_lin_bsg(int fd, int subvalue,
		     struct smp_req_resp * rresp, int verbose);

#endif
