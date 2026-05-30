
MESSAGE( STATUS ">> Entering lib/CMakeLists.txt" )

if ( NEED_GETOPT_LONG OR NEED_GETOPT_H )
    set ( smputils1_gen
        smp_lib.c
        getopt_long.c
    )
else ()
    set ( smputils1_gen
        smp_lib.c
    )
endif ()

# Select Hardware Abstraction Layer (HAL)
if ( PT_DUMMY )
    set ( smputils1_lib_hal
	smp_dummy.c
    )
    MESSAGE( STATUS "<<>> PT_DUMMY configured, no HAL")
elseif ( SMP_LIB_LINUX )
    set ( smputils1_lib_hal
	smp_lin_bsg.c
        smp_lin_sel.c
        smp_mptctl_io.c
        smp_aac_io.c
    )
elseif ( SMP_LIB_FREEBSD )
    set ( smputils1_lib_hal
	smp_fre_cam.c
    )
elseif ( SMP_LIB_SOLARIS )
    set ( smputils1_lib_hal
	smp_sol_usmp.c
    )
else ( )
    MESSAGE( STATUS "<<>> SMP_DUMMY set due to fall through, no HAL")
    set ( smputils1_lib_hal
	smp_dummy.c
    )
endif ( PT_DUMMY )

# For testing it can be useful to compile C code with a C++ compiler. That
# can be done with the following; plus adding CXX to project() line:
#        project(<project_name> VERSION x.y.z LANGUAGES C CXX)
## set_source_files_properties(
##     ${smputils1_gen}
##     ${smputils1_lib_hal}
##     PROPERTIES LANGUAGE CXX
## )

if ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Build shared library(s) (default)" )
    add_library(smputils1 SHARED ${smputils1_gen} ${smputils1_lib_hal} )
    set_target_properties(smputils1 PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
else ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Build a static library" )
    add_definitions ( -static )
    add_library(smputils1 STATIC ${smputils1_gen} ${smputils1_lib_hal} )
endif ( BUILD_SHARED_LIBS )

if ( ENABLE_DEBUG )
    MESSAGE( STATUS ">> Compile lib/ with DEBUG set" )

    target_compile_definitions(smputils1 PRIVATE DEBUG)
    target_compile_options(smputils1 PRIVATE
        -Wextra -Wmisleading-indentation -Wduplicated-cond -Wlogical-op
        -Wnull-dereference -Wshadow -Wunused -Wsizeof-array-argument
        -Wduplicated-branches -Wjump-misses-init -Wparentheses)
endif ( ENABLE_DEBUG )


target_include_directories(smputils1
    PUBLIC
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(smputils1 PRIVATE ${CMAKE_BINARY_DIR})

set_target_properties(smputils1 PROPERTIES
	OUTPUT_NAME "smputils1-${PROJECT_VERSION}"
	VERSION 1.0.0
	SOVERSION 1
)

#
# Install runtime components
#

install(TARGETS smputils1
    EXPORT smputils1Targets

    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT runtime
        NAMELINK_SKIP
)

# Match the autotools -release convention: the development symlink is the
# plain libsmputils1.so name (without the release version embedded).
# Create the directory first so this works when CPack stages components
# into separate trees.
install(CODE "file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_LIBDIR}\")
file(CREATE_LINK
    \"libsmputils1-${PROJECT_VERSION}.so.1\"
    \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_LIBDIR}/libsmputils1.so\"
    SYMBOLIC)"
    COMPONENT development
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/smputils1ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

