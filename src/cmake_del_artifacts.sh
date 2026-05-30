#!/bin/sh

echo "Entering src/cmake_del_artifacts.sh"

rm -rf \
	CMakeFiles \
	cmake_install.cmake \
	Makefile

rm -f       smp_conf_general
rm -f       smp_conf_phy_event
rm -f       smp_conf_route_info
rm -f       smp_conf_zone_man_pass
rm -f       smp_conf_zone_perm_tbl
rm -f       smp_conf_zone_phy_info
rm -f       smp_discover
rm -f       smp_discover_list
rm -f       smp_ena_dis_zoning
rm -f       smp_phy_control
rm -f       smp_phy_test
rm -f       smp_read_gpio
rm -f       smp_rep_broadcast
rm -f       smp_rep_exp_route_tbl
rm -f       smp_rep_general
rm -f       smp_rep_manufacturer
rm -f       smp_rep_phy_err_log
rm -f       smp_rep_phy_event
rm -f       smp_rep_phy_event_list
rm -f       smp_rep_phy_sata
rm -f       smp_rep_route_info
rm -f       smp_rep_self_conf_stat
rm -f       smp_rep_zone_man_pass
rm -f       smp_rep_zone_perm_tbl
rm -f       smp_write_gpio
rm -f       smp_zone_activate
rm -f       smp_zone_lock
rm -f       smp_zone_unlock
rm -f       smp_zoned_broadcast

