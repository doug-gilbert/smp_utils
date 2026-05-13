
MESSAGE( STATUS ">> Entering include/CMakeLists.txt" )

install(
    FILES
        smp_lib.h
        smp_unaligned.h
        smp_pr2serr.h
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT development
)

#
# Install headers
#

# install(
#     DIRECTORY .
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#     COMPONENT development
#     FILES_MATCHING PATTERN "*.h"
# )
