
MESSAGE( ">> Entering src/CMakeLists.txt" )

if ( ENABLE_DEBUG )
    MESSAGE( ">> Compile src/ with DEBUG set" )
endif ( ENABLE_DEBUG )

function(simple_exe arg1)
    # message("arg1 = ${arg1}")

    add_executable( "${arg1}" "${arg1}.c" )
    target_include_directories(${arg1} PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries( ${arg1} smputils1 )
endfunction()

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
    add_executable( ${util} ${util}.c )

    if ( ENABLE_DEBUG )
        target_compile_definitions(${util} PRIVATE DEBUG)
        target_compile_options(${util} PRIVATE
            -Wextra -Wmisleading-indentation -Wduplicated-cond -Wlogical-op
            -Wnull-dereference -Wshadow -Wunused -Wsizeof-array-argument
            -Wduplicated-branches -Wjump-misses-init -Wparentheses)
    endif ( ENABLE_DEBUG )

    target_include_directories(${util} PUBLIC ${CMAKE_SOURCE_DIR}/include)

    target_link_libraries( ${util} PRIVATE smputils1 )

    install(TARGETS ${util}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT utilities
    )

endforeach()


# simple_exe( smp_conf_general )
# simple_exe( smp_conf_phy_event )
# simple_exe( smp_conf_route_info )
# simple_exe( smp_conf_zone_man_pass )
# simple_exe( smp_conf_zone_perm_tbl )
# simple_exe( smp_conf_zone_phy_info )
# simple_exe( smp_discover )
# simple_exe( smp_discover_list )
# simple_exe( smp_ena_dis_zoning )
# simple_exe( smp_phy_control )
# simple_exe( smp_phy_test )
# simple_exe( smp_read_gpio )
# simple_exe( smp_rep_broadcast )
# simple_exe( smp_rep_exp_route_tbl )
# simple_exe( smp_rep_general )
# simple_exe( smp_rep_manufacturer )
# simple_exe( smp_rep_phy_err_log )
# simple_exe( smp_rep_phy_event )
# simple_exe( smp_rep_phy_event_list )
# simple_exe( smp_rep_phy_sata )
# simple_exe( smp_rep_route_info )
# simple_exe( smp_rep_self_conf_stat )
# simple_exe( smp_rep_zone_man_pass )
# simple_exe( smp_rep_zone_perm_tbl )
# simple_exe( smp_write_gpio )
# simple_exe( smp_zone_activate )
# simple_exe( smp_zone_lock )
# simple_exe( smp_zone_unlock )
# simple_exe( smp_zoned_broadcast )

