cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LIBRAZOBILL_PROJECT_VERSION)
string(STRIP "${LIBRAZOBILL_PROJECT_VERSION}" LIBRAZOBILL_PROJECT_VERSION)

project(librazobill VERSION ${LIBRAZOBILL_PROJECT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

if(MSVC)
  add_compile_options(/W4 /wd4996 /arch:AVX2)
elseif(APPLE)
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Woverloaded-virtual)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Woverloaded-virtual -march=native)
endif()

# Core library: librazobill (STATIC by default, or SHARED if BUILD_SHARED_LIBS=ON)
add_library(librazobill
  src/Slots.cpp
  src/Stats.cpp
  src/Channel.cpp
  src/Dialect.cpp
  src/BackendAdbc.cpp
  src/Razobill.cpp
  src/RazobillC.cpp
)
add_library(librazobill::librazobill ALIAS librazobill)
set_target_properties(librazobill PROPERTIES OUTPUT_NAME razobill)

target_include_directories(librazobill
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    thirdparty/adbc
    thirdparty/duckdbcapi
)
target_link_libraries(librazobill PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# DuckDB Native Loadable Extension (C API)
add_library(RazobillDuckdb SHARED ext/duckdb/DuckdbExtension.cpp)
target_include_directories(RazobillDuckdb PRIVATE include thirdparty/duckdbcapi)
target_compile_definitions(RazobillDuckdb PRIVATE DUCKDB_EXTENSION_NAME=razobill)
if(NOT MSVC)
  target_compile_options(RazobillDuckdb PRIVATE -fvisibility=hidden -Wno-missing-field-initializers)
endif()
target_link_libraries(RazobillDuckdb PRIVATE librazobill)
set_target_properties(RazobillDuckdb PROPERTIES PREFIX "" OUTPUT_NAME razobill ARCHIVE_OUTPUT_NAME razobill_duckdb)

find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)
if(WIN32)
  set(DUCKDB_PLATFORM "windows_amd64")
elseif(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM|aarch64|arm64")
    set(DUCKDB_PLATFORM "osx_arm64")
  else()
    set(DUCKDB_PLATFORM "osx_amd64")
  endif()
else()
  set(DUCKDB_PLATFORM "linux_amd64")
endif()

# Sign the DuckDB extension with ABI metadata (both razobill and librazobill names)
add_custom_command(TARGET RazobillDuckdb POST_BUILD
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/AppendExtensionMetadata.py -l $<TARGET_FILE:RazobillDuckdb> -n razobill
          -o ${CMAKE_BINARY_DIR}/razobill.duckdb_extension -p ${DUCKDB_PLATFORM} -dv v1.2.0 -ev v${LIBRAZOBILL_PROJECT_VERSION} --abi-type C_STRUCT
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/razobill.duckdb_extension ${CMAKE_BINARY_DIR}/librazobill.duckdb_extension
  COMMENT "Signing DuckDB loadable extension metadata for community distribution" VERBATIM)

# Python Bindings: pyrazobill via pybind11
execute_process(
  COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
  OUTPUT_VARIABLE pybind11_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(Core lib/python/PyRazobill.cpp)
target_include_directories(Core PRIVATE include)
target_link_libraries(Core PRIVATE librazobill)
if(NOT MSVC)
  target_compile_options(Core PRIVATE -Wno-missing-field-initializers)
endif()
if(WIN32 AND MINGW)
  target_link_options(Core PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic -l:libwinpthread.a -Wl,-Bdynamic)
endif()

# Copy compiled Core extension to lib/python/pyrazobill package directory
add_custom_command(TARGET Core POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Core> ${CMAKE_SOURCE_DIR}/lib/python/pyrazobill/
  COMMENT "Copying Core extension into python distribution" VERBATIM)

# Testing Suites
enable_testing()

add_executable(UnitTests
  tests/UnitCore.cpp
  tests/UnitStats.cpp
  tests/UnitArena.cpp
  tests/UnitCAPI.cpp
  tests/UnitChannel.cpp
)
target_include_directories(UnitTests PRIVATE thirdparty include)
if(NOT MSVC)
  target_compile_options(UnitTests PRIVATE -Wno-missing-field-initializers)
endif()
target_link_libraries(UnitTests PRIVATE librazobill)
add_test(NAME unit COMMAND UnitTests)

add_executable(IntegrationDuckdb tests/IntegrationDuckdb.cpp)
target_include_directories(IntegrationDuckdb PRIVATE thirdparty include tests)
if(NOT MSVC)
  target_compile_options(IntegrationDuckdb PRIVATE -Wno-missing-field-initializers)
endif()
target_link_libraries(IntegrationDuckdb PRIVATE librazobill)
add_test(NAME integration_duckdb COMMAND IntegrationDuckdb)
set_tests_properties(integration_duckdb PROPERTIES ENVIRONMENT "RZB_EXT=${CMAKE_BINARY_DIR}/librazobill.duckdb_extension")

add_executable(IntegrationChdb tests/IntegrationChdb.cpp)
target_include_directories(IntegrationChdb PRIVATE thirdparty include tests)
if(NOT MSVC)
  target_compile_options(IntegrationChdb PRIVATE -Wno-missing-field-initializers)
endif()
target_link_libraries(IntegrationChdb PRIVATE librazobill)
add_test(NAME integration_chdb COMMAND IntegrationChdb)

add_executable(BenchCore bench/BenchCore.cpp)
target_include_directories(BenchCore PRIVATE include)
if(NOT MSVC)
  target_compile_options(BenchCore PRIVATE -Wno-missing-field-initializers)
endif()
target_link_libraries(BenchCore PRIVATE librazobill)

add_executable(BenchStorage bench/BenchStorage.cpp)
target_include_directories(BenchStorage PRIVATE thirdparty include tests)
if(NOT MSVC)
  target_compile_options(BenchStorage PRIVATE -Wno-missing-field-initializers)
endif()
target_link_libraries(BenchStorage PRIVATE librazobill)

# ==============================================================================
# Public Installation & Packaging (CMake Export, pkg-config, CPack)
# ==============================================================================

# Install librazobill library
install(TARGETS librazobill
  EXPORT librazobillTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install public headers
install(DIRECTORY include/librazobill
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install DuckDB extension if built
install(FILES "${CMAKE_BINARY_DIR}/librazobill.duckdb_extension"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/razobill/extensions"
  OPTIONAL
)

# CMake Package Export
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/librazobillConfigVersion.cmake"
  VERSION ${LIBRAZOBILL_PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/librazobillConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/librazobillConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/librazobill"
)

install(EXPORT librazobillTargets
  FILE librazobillTargets.cmake
  NAMESPACE librazobill::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/librazobill"
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/librazobillConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/librazobillConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/librazobill"
)

# pkg-config generation
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/librazobill.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/librazobill.pc"
  @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/librazobill.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

# CPack binary distribution configuration
set(CPACK_PACKAGE_NAME "librazobill")
set(CPACK_PACKAGE_VENDOR "M. M. Atta and Razobill Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance C++20 log parsing and analytical database compaction engine")
set(CPACK_PACKAGE_VERSION ${LIBRAZOBILL_PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)
