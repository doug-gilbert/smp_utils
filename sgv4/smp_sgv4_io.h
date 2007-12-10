#ifndef SMP_SGV4_IO_H
#define SMP_SGV4_IO_H

#include "smp_lib.h"

extern int chk_sgv4_device(const char * dev_name, int verbose);

extern int open_sgv4_device(const char * dev_name, int verbose);

extern int close_sgv4_device(int fd);

extern int send_req_sgv4(int fd, int subvalue,
                         struct smp_req_resp * rresp, int verbose);

#endif
