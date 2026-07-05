# Cross-platform build for the File ML library.
# It also installs the library so other CMake projects can use it via find_package(FileML).
cmake_minimum_required(VERSION 3.15)
project(FileML VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# library target
add_library(fileml STATIC
    "src/File.cpp"
)

# An alias so it can be linked as FileML::fileml whether it is used via
# add_subdirectory() or installed and found with find_package().
add_library(FileML::fileml ALIAS fileml)

# The header location differs between building here and being installed
# elsewhere, so describe both with generator expressions.
target_include_directories(fileml PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<INSTALL_INTERFACE:include>"
)

# No external dependencies: File ML is a pure <filesystem> + <fstream> wrapper.
# Some older toolchains need an explicit link to the stdc++fs / c++fs library;
# link it when the linker actually provides it.
include(CheckCXXSourceCompiles)
if(NOT MSVC)
    find_library(STDCXXFS_LIB NAMES stdc++fs)
    if(STDCXXFS_LIB)
        target_link_libraries(fileml PUBLIC ${STDCXXFS_LIB})
    endif()
endif()

# install and export
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 1. Install the compiled library and record it in an export set.
install(TARGETS fileml
    EXPORT FileMLTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 2. Install the public header.
install(FILES "src/fileml.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 3. Generate and install FileMLTargets.cmake (the importable targets).
install(EXPORT FileMLTargets
    FILE FileMLTargets.cmake
    NAMESPACE FileML::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FileML
)

# 4. Generate the package config and version files from the template.
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FileMLConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FileMLConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FileML
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FileMLConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FileMLConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FileMLConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FileML
)

# Optional in-tree smoke tests (built only when explicitly requested, so downstream
# consumers via add_subdirectory / FetchContent never build them by default).
option(FILEML_BUILD_TESTS "Build the File ML smoke tests (tools/tests)" OFF)
if(FILEML_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tools/tests)
endif()

# Building, installing, and consuming instructions for this library are as follows:
#
#   Build:
#     cmake -B build -S .
#     cmake --build build --config Release
#
#   Install (to a prefix of your choice):
#     cmake --install build --prefix /path/to/install
#
#   Then, from another CMake project:
#     find_package(FileML REQUIRED)
#     target_link_libraries(myapp PRIVATE FileML::fileml)
