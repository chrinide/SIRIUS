include(FindPackageHandleStandardArgs)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  set(_sciname "sci_gnu_mpi_mp")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "INTEL")
  set(_sciname "sci_intel_mpi_mp")
else()
  message(FATAL_ERROR "Unknown compiler. When using libsci use either GNU or INTEL compiler")
endif()

find_library(CRAY_LIBSCI_LIBRARIES
  NAMES ${_sciname}
  HINTS
  ${_SCALAPACK_LIBRARY_DIRS}
  ENV SCALAPACKROOT
  ENV CRAY_LIBSCI_PREFIX_DIR
  PATH_SUFFIXES lib
  DOC "scalapack library path")

find_package_handle_standard_args(CRAY_LIBSCI DEFAULT_MSG CRAY_LIBSCI_LIBRARIES)
