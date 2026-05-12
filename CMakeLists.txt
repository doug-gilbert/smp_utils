cmake_minimum_required ( VERSION 3.10 )

project(smp_utils VERSION 0.99.9 LANGUAGES C)
# Use version number: x.y.9 to indicate pre-release of x.(y+1)
# Example: VERSION 0.99.9 is pre-release of 1.00

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g" )

option ( BUILD_SHARED_LIBS "Build using shared libraries" ON)
option ( ENABLE_DEBUG "Turn on debugging (extra warnings)" OFF)
option ( DISABLE_FAST_LEBE "Use generic little-endian/big-endian code instead" OFF)

if ( ENABLE_DEBUG )
    MESSAGE( ">> Compile with DEBUG set" )

endif ( ENABLE_DEBUG )

add_subdirectory( include )

add_subdirectory( lib )

add_subdirectory( src )

# target_include_directories(smp_utils PUBLIC include)

# target_include_directories( smp_utils PUBLIC lib )

# target_link_libraries( ${PROJECT_NAME} smputils1 )

include ( CheckIncludeFile )
CHECK_INCLUDE_FILE( "byteswap.h" HAVE_BYTESWAP_H )

if ( HAVE_BYTESWAP_H )
  add_definitions ( -DHAVE_BYTESWAP_H )
endif ( HAVE_BYTESWAP_H )

if ( DISABLE_FAST_LEBE )
  add_definitions ( -DIGNORE_FAST_LEBE )
endif ( DISABLE_FAST_LEBE )


# set ( sourcefiles_sdparm
#         src/sdparm.c
#         src/sdparm_access.c
#         src/sdparm_cmd.c
#         src/sdparm_data.c
#         src/sdparm_data_vendor.c
#         src/sdparm_vpd.c
# )

# set ( src_nolib_sdparm
#         lib/sg_lib.c
#         lib/sg_lib_data.c
#         lib/sg_cmds_basic.c
#         lib/sg_cmds_basic2.c
#         lib/sg_cmds_mmc.c
#         lib/sg_pr2serr.c
#         lib/sg_pt_common.c
#         lib/sg_pt_linux.c
#         lib/sg_pt_linux_nvme.c
#         lib/sg_cmds_extra.c
#         lib/sg_snt.c
#         lib/sg_json_builder.c
#         lib/sg_json.c
#         lib/sg_json_sg_lib.c
# )

if ( BUILD_SHARED_LIBS )
    MESSAGE( ">> Build using shared libraries (default)" )
    find_library(LIBSMPUTILS NAMES smputils1 PATH_SUFFIXES lib)
    if ( LIBSMPUTILS )
        MESSAGE( ">> Found shared library: ${LIBSMPUTILS}" )
#         add_executable ( smp_utils ${sourcefiles_sdparm} )
#         # target_include_directories(smp_utils PUBLIC include)
#         target_link_libraries(smp_utils PRIVATE ${LIBSMPUTILS})
    else ( LIBSMPUTILS )
        MESSAGE( ">> Did not find shared library: ${LIBSMPUTILS}" )
        # add_definitions ( -DSMP_LIB_LINUX )
#         add_executable ( smp_utils ${sourcefiles_sdparm} ${src_nolib_sdparm} )
#         # target_include_directories(smp_utils PUBLIC include)
#         target_link_libraries(smp_utils)
    endif ( LIBSMPUTILS )
else ( BUILD_SHARED_LIBS )
    MESSAGE( ">> Trying to build a static executable" )
    add_definitions ( -DSMP_LIB_LINUX )
#    add_executable ( smp_utils ${sourcefiles_sdparm} ${src_nolib_sdparm} )
    add_definitions ( -static )
    # target_include_directories(smp_utils PUBLIC include)
#    target_link_libraries(smp_utils -static)
endif ( BUILD_SHARED_LIBS )

# target_compile_definitions(smp_utils PRIVATE _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
# target_compile_options(smp_utils PRIVATE -Wall -W)
# target_include_directories(smp_utils PRIVATE include ${CMAKE_SOURCE_DIR}/src)
# target_include_directories(smp_utils PRIVATE include)

# if ( ENABLE_DEBUG )
#   target_compile_definitions(smp_utils PRIVATE DEBUG)
#   target_compile_options(smp_utils PRIVATE
#     -Wextra -Wmisleading-indentation -Wduplicated-cond -Wlogical-op
#     -Wnull-dereference -Wshadow -Wunused -Wsizeof-array-argument
#     -Wduplicated-branches -Wjump-misses-init -Wparentheses)
# endif ( ENABLE_DEBUG )

# install(TARGETS smp_utils RUNTIME DESTINATION bin)

# file(ARCHIVE_CREATE OUTPUT sdparm.8.gz PATHS doc/sdparm.8 FORMAT raw COMPRESSION GZip)
# file(ARCHIVE_CREATE OUTPUT sdparm_json.8.gz PATHS doc/sdparm_json.8 FORMAT raw COMPRESSION GZip)
# install(FILES sdparm.8.gz DESTINATION "${CMAKE_INSTALL_MANDIR}/man8")
# install(FILES sdparm_json.8.gz DESTINATION "${CMAKE_INSTALL_MANDIR}/man8")
# install(FILES ChangeLog DESTINATION "${CMAKE_INSTALL_DOCDIR}")
# install(FILES README DESTINATION "${CMAKE_INSTALL_DOCDIR}")
# install(FILES AUTHORS DESTINATION "${CMAKE_INSTALL_DOCDIR}")
# install(FILES CREDITS DESTINATION "${CMAKE_INSTALL_DOCDIR}")

if ( ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" )
    MESSAGE( ">> Detected FreeBSD" )
    target_include_directories( smp_utils PRIVATE /usr/local/include )
endif ( )


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VENDOR "Douglas Gilbert")
set(CPACK_PROJECT_VERSION "${PROJECT_VERSION}")

set(CPACK_COMPONENTS_ALL
    runtime
    development
    utilities
)

set(CPACK_COMPONENT_RUNTIME_DESCRIPTION
    "libsmputils1 runtime library for smp_utils")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION
    "libsmputils1 development library and include files for smp_utils")
set(CPACK_COMPONENTS_GROUPING IGNORE)

#
# DEB
#

set(CPACK_DEBIAN_UTILITIES_PACKAGE_NAME "smp-utils")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "libsmputils1")
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME "libsmputils1-dev")
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DEPENDS
    "libsmputils1 (= ${PROJECT_VERSION})")
SET(CPACK_DEBIAN_PACKAGE_DESCRIPTION "SMP is the SAS Management Protocol. These utilities interact with s SMP device (e.g. in an enclosure)")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Douglas Gilbert")
SET(CPACK_DEBIAN_PACKAGE_SECTION "admin")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS " ")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

SET(CPACK_DEB_COMPONENT_INSTALL YES)

#
# RPM
#

set(CPACK_RPM_UTILITIES_PACKAGE_NAME "smp_utils")
set(CPACK_RPM_RUNTIME_PACKAGE_NAME "smputils1-libs")
set(CPACK_RPM_DEVELOPMENT_PACKAGE_NAME "smputils1-devel")
SET(CPACK_RPM_PACKAGE_DESCRIPTION "SMP is the SAS Management Protocol. These utilities interact with s SMP device (e.g. in an enclosure)")
SET(CPACK_RPM_PACKAGE_DEPENDS "")
SET(CPACK_RPM_PACKAGE_LICENSE "BSD-2-clause")
SET(CPACK_RPM_PACKAGE_VENDOR "Douglas Gilbert")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

set(CPACK_RPM_COMPONENT_INSTALL YES)

SET(CPACK_GENERATOR "DEB;RPM")

include(CPack)
include(CTest)
