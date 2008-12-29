#ifndef SMP_PORTAL_INTF_H
#define SMP_PORTAL_INTF_H

extern int chk_smp_portal_file(const char *smp_portal_name, int verbose);

extern int do_smp_portal_func(const char *smp_portal_name,
                              void *smp_req, int smp_req_size,
                              void *smp_resp, int smp_resp_size, int verbose);

#endif
