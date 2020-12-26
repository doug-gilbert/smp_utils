
## Copied from sg3_utils. Some remnants left commented out as examples
## if OS_LINUX

# binaries were in in a sbin directory (e.g. /usr/sbin) using
# sbin_PROGRAMS . Caused problems so put back in normal
# place with bin_PROGRAMS = xxxx .

bin_PROGRAMS = \
	smp_conf_general smp_conf_phy_event smp_conf_route_info \
	smp_conf_zone_man_pass smp_conf_zone_perm_tbl \
	smp_conf_zone_phy_info smp_discover smp_discover_list \
	smp_ena_dis_zoning smp_phy_control smp_phy_test \
	smp_read_gpio smp_rep_broadcast smp_rep_exp_route_tbl \
	smp_rep_general smp_rep_manufacturer smp_rep_phy_err_log \
	smp_rep_phy_event smp_rep_phy_event_list smp_rep_phy_sata \
	smp_rep_route_info smp_rep_self_conf_stat \
	smp_rep_zone_man_pass smp_rep_zone_perm_tbl smp_write_gpio \
	smp_zone_activate smp_zoned_broadcast smp_zone_lock \
	smp_zone_unlock

## distclean-local:
## 	rm -f sg_scan.c

## sg_scan.c : sg_scan.c.linux
## 	cp sg_scan.c.linux sg_scan.c

## endif

# for testing with various compilers
## CC = gcc
## CC = g++
## CC = clang
## CC = clang++

# -std=<s> can be c99, c11, gnu11, etc. Default is gnu89 (gnu90 is the same)
AM_CPPFLAGS = -iquote ${top_srcdir}/include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
AM_CFLAGS = -Wall -W
## AM_CFLAGS = -Wall -W -pedantic -std=c11
## AM_CFLAGS = -Wall -W -pedantic -std=c++14
## AM_CFLAGS = -Wall -W -pedantic -std=gnu++1z

smp_conf_general_SOURCES =	smp_conf_general.c
smp_conf_general_LDADD = ../lib/libsmputils1.la

smp_conf_phy_event_SOURCES =	smp_conf_phy_event.c
smp_conf_phy_event_LDADD = ../lib/libsmputils1.la

smp_conf_route_info_SOURCES =	smp_conf_route_info.c
smp_conf_route_info_LDADD = ../lib/libsmputils1.la

smp_conf_zone_man_pass_SOURCES = smp_conf_zone_man_pass.c
smp_conf_zone_man_pass_LDADD = ../lib/libsmputils1.la

smp_conf_zone_perm_tbl_SOURCES = smp_conf_zone_perm_tbl.c
smp_conf_zone_perm_tbl_LDADD = ../lib/libsmputils1.la

smp_conf_zone_phy_info_SOURCES = smp_conf_zone_phy_info.c
smp_conf_zone_phy_info_LDADD = ../lib/libsmputils1.la

smp_discover_SOURCES = smp_discover.c
smp_discover_LDADD = ../lib/libsmputils1.la

smp_discover_list_SOURCES = smp_discover_list.c
smp_discover_list_LDADD = ../lib/libsmputils1.la

smp_ena_dis_zoning_SOURCES = smp_ena_dis_zoning.c
smp_ena_dis_zoning_LDADD = ../lib/libsmputils1.la

smp_phy_control_SOURCES = smp_phy_control.c
smp_phy_control_LDADD = ../lib/libsmputils1.la

smp_phy_test_SOURCES = smp_phy_test.c
smp_phy_test_LDADD = ../lib/libsmputils1.la

smp_read_gpio_SOURCES = smp_read_gpio.c
smp_read_gpio_LDADD = ../lib/libsmputils1.la

smp_rep_broadcast_SOURCES = smp_rep_broadcast.c
smp_rep_broadcast_LDADD = ../lib/libsmputils1.la

smp_rep_exp_route_tbl_SOURCES = smp_rep_exp_route_tbl.c
smp_rep_exp_route_tbl_LDADD = ../lib/libsmputils1.la

smp_rep_general_SOURCES = smp_rep_general.c
smp_rep_general_LDADD = ../lib/libsmputils1.la

smp_rep_manufacturer_SOURCES = smp_rep_manufacturer.c
smp_rep_manufacturer_LDADD = ../lib/libsmputils1.la

smp_rep_phy_err_log_SOURCES = smp_rep_phy_err_log.c
smp_rep_phy_err_log_LDADD = ../lib/libsmputils1.la

smp_rep_phy_event_SOURCES = smp_rep_phy_event.c
smp_rep_phy_event_LDADD = ../lib/libsmputils1.la

smp_rep_phy_event_list_SOURCES = smp_rep_phy_event_list.c
smp_rep_phy_event_list_LDADD = ../lib/libsmputils1.la

smp_rep_phy_sata_SOURCES = smp_rep_phy_sata.c
smp_rep_phy_sata_LDADD = ../lib/libsmputils1.la

smp_rep_route_info_SOURCES = smp_rep_route_info.c
smp_rep_route_info_LDADD = ../lib/libsmputils1.la

smp_rep_self_conf_stat_SOURCES = smp_rep_self_conf_stat.c
smp_rep_self_conf_stat_LDADD = ../lib/libsmputils1.la

smp_rep_zone_man_pass_SOURCES = smp_rep_zone_man_pass.c
smp_rep_zone_man_pass_LDADD = ../lib/libsmputils1.la

smp_rep_zone_perm_tbl_SOURCES = smp_rep_zone_perm_tbl.c
smp_rep_zone_perm_tbl_LDADD = ../lib/libsmputils1.la

smp_write_gpio_SOURCES = smp_write_gpio.c
smp_write_gpio_LDADD = ../lib/libsmputils1.la

smp_zone_activate_SOURCES = smp_zone_activate.c
smp_zone_activate_LDADD = ../lib/libsmputils1.la

smp_zoned_broadcast_SOURCES = smp_zoned_broadcast.c
smp_zoned_broadcast_LDADD = ../lib/libsmputils1.la

smp_zone_lock_SOURCES = smp_zone_lock.c
smp_zone_lock_LDADD = ../lib/libsmputils1.la

smp_zone_unlock_SOURCES = smp_zone_unlock.c
smp_zone_unlock_LDADD = ../lib/libsmputils1.la


distclean-local:
	rm -rf .deps
