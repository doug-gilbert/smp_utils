
MESSAGE( STATUS ">> Entering lib/CMakeLists.txt" )

set ( smputils1_lib_lin
	smp_lib.c
        smp_lin_bsg.c
        smp_lin_sel.c
        smp_mptctl_io.c
        smp_aac_io.c
)

if ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Build shared library(s) (default)" )
    add_library(smputils1 SHARED ${smputils1_lib_lin} )

    add_library(smputils1_static STATIC ${smputils1_lib_lin} )
    set_target_properties(smputils1_static PROPERTIES
        OUTPUT_NAME smputils1
    )
    target_compile_definitions(smputils1_static PRIVATE HAVE_CONFIG_H)
    target_include_directories(smputils1_static PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
    )
else ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Build a static library" )
    add_definitions ( -static )
    add_library(smputils1 STATIC ${smputils1_lib_lin} )
endif ( BUILD_SHARED_LIBS )

target_compile_definitions(smputils1 PRIVATE HAVE_CONFIG_H)

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
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/scsi>
    PRIVATE
	${CMAKE_SOURCE_DIR}
)

target_compile_features(smputils1 PUBLIC c_std_99)

set_target_properties(smputils1 PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION 1
)

#
# Install runtime components
#

install(TARGETS smputils1
    EXPORT smputils1Targets

    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT runtime

    NAMELINK_COMPONENT development
)

if ( BUILD_SHARED_LIBS )
    install(TARGETS smputils1_static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT development
    )
endif ( BUILD_SHARED_LIBS )

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/smputils1ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

