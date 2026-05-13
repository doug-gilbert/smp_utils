
MESSAGE( STATUS ">> Entering doc/CMakeLists.txt" )

file(ARCHIVE_CREATE OUTPUT doc/smp_utils.8.gz PATHS doc/smp_utils.8 FORMAT raw COMPRESSION GZip)
install(FILES smp_utils.8.gz DESTINATION "${CMAKE_INSTALL_MANDIR}/man8" COMPONENT utilities)

set(SMP_UTILS
        smp_conf_general
        smp_conf_phy_event
        smp_conf_route_info
        smp_conf_zone_man_pass
        smp_conf_zone_perm_tbl
        smp_conf_zone_phy_info
        smp_discover
        smp_discover_list
        smp_ena_dis_zoning
        smp_phy_control
        smp_phy_test
        smp_read_gpio
        smp_rep_broadcast
        smp_rep_exp_route_tbl
        smp_rep_general
        smp_rep_manufacturer
        smp_rep_phy_err_log
        smp_rep_phy_event
        smp_rep_phy_event_list
        smp_rep_phy_sata
        smp_rep_route_info
        smp_rep_self_conf_stat
        smp_rep_zone_man_pass
        smp_rep_zone_perm_tbl
        smp_write_gpio
        smp_zone_activate
        smp_zone_lock
        smp_zone_unlock
        smp_zoned_broadcast
)

foreach(util ${SMP_UTILS})

    file(ARCHIVE_CREATE
         OUTPUT doc/${util}.8.gz
         PATHS doc/${util}.8
         FORMAT raw
         COMPRESSION GZip
    )
    install(FILES ${util}.8.gz
            DESTINATION "${CMAKE_INSTALL_MANDIR}/man8"
            COMPONENT utilities
    )

endforeach()
