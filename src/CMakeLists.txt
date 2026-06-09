
MESSAGE(STATUS ">> Entering src/CMakeLists.txt" )

if ( ENABLE_DEBUG )
    MESSAGE( STATUS ">> Compile src/ with DEBUG set" )
endif ( ENABLE_DEBUG )

if ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Build utilities based on smputils library (default)" )
else ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Build static utilities" )
    add_definitions ( -static )
endif ( BUILD_SHARED_LIBS )

function(build_one util_name source_files)

    add_executable( ${util_name} ${source_files} )

    if ( ENABLE_DEBUG )
        target_compile_definitions(${util_name} PRIVATE DEBUG)
        target_compile_options(${util_name} PRIVATE
            -Wextra -Wmisleading-indentation -Wduplicated-cond -Wlogical-op
            -Wnull-dereference -Wshadow -Wunused -Wsizeof-array-argument
            -Wduplicated-branches -Wjump-misses-init -Wparentheses)
    endif ( ENABLE_DEBUG )

    target_include_directories(${util_name} PRIVATE ${CMAKE_BINARY_DIR})
    target_include_directories(${util_name} PUBLIC ${CMAKE_SOURCE_DIR}/include)
    if ( NEED_GETOPT_LONG OR NEED_GETOPT_H )
	# need to get lib/getopt.h with #include <getopt.h>
        target_include_directories(${util_name} PRIVATE ${CMAKE_SOURCE_DIR}/lib)
    endif ()
    if ( SMP_LIB_FREEBSD )
	target_include_directories( ${util_name} PRIVATE /usr/local/include )
        target_link_libraries( ${util_name} PRIVATE cam )
    endif ( SMP_LIB_FREEBSD )

    if ( NOT BUILD_SHARED_LIBS )
        target_link_options(${util_name} PRIVATE -static)
    endif ( NOT BUILD_SHARED_LIBS )

    target_link_libraries( ${util_name} PRIVATE smputils1 )

    install(TARGETS ${util_name}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT utilities
    )
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
    build_one ( ${util} ${util}.c )
endforeach()

