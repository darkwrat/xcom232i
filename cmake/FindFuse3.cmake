# - Find libfuse3 library
# The module defines the following variables:
#
#  FUSE3_FOUND - true if libfuse3 was found
#  FUSE3_INCLUDE_DIRS - the directory of the libfuse3 headers
#  FUSE3_LIBRARIES - the libfuse3 library
#

find_path(FUSE3_INCLUDE_DIR fuse.h fuse_lowlevel.h PATH_SUFFIXES fuse3)
find_library(FUSE3_LIBRARY NAMES fuse3)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(fuse3
    REQUIRED_VARS FUSE3_INCLUDE_DIR FUSE3_LIBRARY)
set(FUSE3_INCLUDE_DIRS ${FUSE3_INCLUDE_DIR})
set(FUSE3_LIBRARIES ${FUSE3_LIBRARY})
mark_as_advanced(FUSE3_INCLUDE_DIR FUSE3_INCLUDE_DIRS
                 FUSE3_LIBRARY FUSE3_LIBRARIES)
