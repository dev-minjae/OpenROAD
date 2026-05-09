# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019-2025, The OpenROAD Authors

cmake_minimum_required (VERSION 3.16)

# Use standard target names
cmake_policy(SET CMP0078 NEW)

# Allows SWIG_MODULE_NAME to be set
cmake_policy(SET CMP0086 NEW)

# CMP0074: Allows find_package to use <PackageName>_ROOT variables. This
# policy was the original mechanism for letting users specify custom
# dependency locations.
cmake_policy(SET CMP0074 NEW)

# CMP0144: Standardizes the use of <PACKAGENAME>_ROOT (uppercase) variables
# in find_package. Newer CMake modules rely on this policy for consistent
# behavior. Setting both CMP0074 and CMP0144 ensures that both older and
# newer find modules correctly respect custom dependency paths, which is
# critical for builds in non-standard environments.
cmake_policy(SET CMP0144 NEW)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
  # Disallows <PACKAGENAME>_ROOT in find_package.
  # TODO: resolve absl/or-tools issue and switch to NEW
  cmake_policy(SET CMP0144 OLD)
endif()

# Allow AUTOUIC on generated source
cmake_policy(SET CMP0071 NEW)

# option() behavior
cmake_policy(SET CMP0077 NEW)

# Let AUTOMOC and AUTOUIC process GENERATED files.
cmake_policy(SET CMP0071 NEW)

# Allow to use external shared boost libraries
option(USE_SYSTEM_BOOST "Use system shared Boost libraries" OFF)

# Allow to use external shared opensta libraries
option(USE_SYSTEM_OPENSTA "Use system shared OpenSTA library" OFF)

# Allow to use external shared abc libraries
option(USE_SYSTEM_ABC "Use system shared ABC library" OFF)

# Allow disabling tests
option(ENABLE_TESTS "Enable OpenROAD tests" ON)

# Opt-in GPU acceleration via Kokkos. The actual compute backend (CUDA, HIP,
# SYCL, or host-only OpenMP/Threads) is determined by the installed Kokkos
# package; OpenROAD inspects Kokkos_ENABLE_* and turns on the matching CMake
# language and dependencies automatically. See the per-module CMakeLists for
# how individual subsystems wire their GPU sources.
option(ENABLE_GPU "Enable GPU acceleration via Kokkos" OFF)

# Allow enabling address sanitizer
option(ASAN "Enable Address Sanitizer" OFF)

# Allow enabling address sanitizer
#  On Ubuntu22.04 you need to use "sudo sysctl vm.mmap_rnd_bits=28" to
#  resolve a known fatal
option(TSAN "Enable Thread Sanitizer" OFF)

# Allow enabling address sanitizer
option(UBSAN "Enable Undefined Behavior Sanitizer" OFF)

project(OpenROAD VERSION 1
  LANGUAGES CXX
)

set(OPENROAD_HOME ${PROJECT_SOURCE_DIR})
set(OPENROAD_SHARE ${CMAKE_INSTALL_PREFIX}/share/openroad)

# Default c++ standard used unless otherwise specified in target_compile_features.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Disable compiler specific extensions like gnu++11.
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Get version string in OPENROAD_VERSION
if(NOT OPENROAD_VERSION)
  include(GetGitRevisionDescription)
  git_describe(OPENROAD_VERSION --tags --match "[0-9][0-9]Q[0-9]")
  string(FIND ${OPENROAD_VERSION} "NOTFOUND" GIT_DESCRIBE_NOTFOUND)
  if(${GIT_DESCRIBE_NOTFOUND} GREATER -1)
    message(WARNING "OpenROAD git describe failed, using sha1 instead")
    get_git_head_revision(GIT_REFSPEC OPENROAD_VERSION)
  endif()
endif()

message(STATUS "OpenROAD version: ${OPENROAD_VERSION}")

# Default to bulding optimnized/release executable.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RELEASE)
endif()

# GPU backend wiring (opt-in). When ENABLE_GPU=ON we discover the user's
# Kokkos install, inherit its compute backend, and turn on the matching CMake
# language so downstream targets can mark kernel sources with
# set_source_files_properties(... LANGUAGE CUDA|HIP). Per-module CMakeLists
# (e.g. src/gpl) key off ENABLE_GPU and Kokkos_ENABLE_*; they no longer need
# to call find_package(Kokkos) or enable_language() themselves.
if(ENABLE_GPU)
  find_package(Kokkos QUIET)
  if(NOT Kokkos_FOUND)
    message(FATAL_ERROR
      "OpenROAD: ENABLE_GPU=ON requires the Kokkos package to be "
      "installed and discoverable by CMake, but Kokkos was not found.\n"
      "  - If Kokkos is already installed: pass "
      "-DKokkos_ROOT=/path/to/kokkos (or extend CMAKE_PREFIX_PATH).\n"
      "  - If not: build and install Kokkos from "
      "https://github.com/kokkos/kokkos with the desired backend "
      "(CUDA / HIP / SYCL / OpenMP) and a target architecture that "
      "matches the host GPU.\n"
      "  - A future etc/DependencyInstaller.sh -gpu option will "
      "automate this step.")
  endif()
  message(STATUS "OpenROAD: GPU acceleration enabled (Kokkos ${Kokkos_VERSION})")
  if(Kokkos_ENABLE_CUDA)
    # Auto-discover nvcc when the user has CUDA installed at a standard
    # location but their environment does not expose it on PATH (common
    # with IDE-launched configures: the bundled CMake does not inherit
    # the shell PATH). enable_language(CUDA) below would otherwise abort
    # with "No CMAKE_CUDA_COMPILER could be found" even though Kokkos's
    # find_package already located the toolkit.
    if(NOT DEFINED CMAKE_CUDA_COMPILER AND NOT DEFINED ENV{CUDACXX})
      find_program(_OPENROAD_NVCC nvcc
        HINTS ENV CUDA_HOME ENV CUDA_PATH ENV CUDA_ROOT
              /usr/local/cuda/bin
              /usr/local/cuda-13.0/bin
              /usr/local/cuda-12.8/bin /usr/local/cuda-12.0/bin
              /opt/cuda/bin
      )
      if(_OPENROAD_NVCC)
        set(CMAKE_CUDA_COMPILER "${_OPENROAD_NVCC}" CACHE FILEPATH "")
        message(STATUS "OpenROAD: auto-discovered nvcc at ${_OPENROAD_NVCC}")
      endif()
    endif()
    # nvcc < 13 cannot parse glibc 2.38+'s _Float128 type that ships with
    # gcc 13+'s C++ standard library headers (math.h template specialization
    # for __iseqsig_type<_Float128>). When a known-broken pairing is detected,
    # pin a compatible older g++ as the CUDA host compiler (the system C++
    # compiler stays unchanged for non-CUDA TUs). Override is always
    # available via -DCMAKE_CUDA_HOST_COMPILER or CUDAHOSTCXX.
    if(NOT DEFINED CMAKE_CUDA_HOST_COMPILER AND NOT DEFINED ENV{CUDAHOSTCXX}
       AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
       AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "13.0"
       AND _OPENROAD_NVCC)
      execute_process(
        COMMAND "${_OPENROAD_NVCC}" --version
        OUTPUT_VARIABLE _OPENROAD_NVCC_VERSION_OUTPUT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(_OPENROAD_NVCC_VERSION_OUTPUT MATCHES "release ([0-9]+)")
        set(_OPENROAD_NVCC_MAJOR "${CMAKE_MATCH_1}")
        if(_OPENROAD_NVCC_MAJOR LESS 13)
          foreach(_OPENROAD_GXX_VER 12 11)
            find_program(_OPENROAD_CUDAHOST g++-${_OPENROAD_GXX_VER}
              HINTS /usr/bin /usr/local/bin)
            if(_OPENROAD_CUDAHOST)
              set(CMAKE_CUDA_HOST_COMPILER "${_OPENROAD_CUDAHOST}"
                CACHE FILEPATH "")
              message(STATUS
                "OpenROAD: pinning CUDA host compiler to "
                "${_OPENROAD_CUDAHOST} (nvcc ${_OPENROAD_NVCC_MAJOR}.x + "
                "glibc/gcc 13+ _Float128 compat)")
              break()
            endif()
            unset(_OPENROAD_CUDAHOST CACHE)
          endforeach()
          if(NOT DEFINED CMAKE_CUDA_HOST_COMPILER)
            message(FATAL_ERROR
              "OpenROAD: nvcc ${_OPENROAD_NVCC_MAJOR}.x cannot parse "
              "_Float128 declarations in glibc 2.38+ system headers used "
              "by gcc ${CMAKE_CXX_COMPILER_VERSION}, and no compatible "
              "g++-12 / g++-11 was found in /usr/bin or /usr/local/bin. "
              "Install one (e.g. apt install g++-12) or set "
              "-DCMAKE_CUDA_HOST_COMPILER=/path/to/older-g++ explicitly.")
          endif()
        endif()
      endif()
    endif()
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR "${CMAKE_CUDA_ARCHITECTURES}" STREQUAL "")
      if(DEFINED Kokkos_CUDA_ARCHITECTURES
         AND NOT "${Kokkos_CUDA_ARCHITECTURES}" STREQUAL "")
        set(CMAKE_CUDA_ARCHITECTURES "${Kokkos_CUDA_ARCHITECTURES}")
      else()
        message(FATAL_ERROR
          "OpenROAD: ENABLE_GPU=ON with Kokkos CUDA backend, but the "
          "Kokkos package does not advertise Kokkos_CUDA_ARCHITECTURES "
          "and CMAKE_CUDA_ARCHITECTURES was not provided. Set "
          "-DCMAKE_CUDA_ARCHITECTURES=<arch> explicitly (e.g. 89 for "
          "RTX 4070, 120 for RTX 5090) or rebuild Kokkos with the "
          "target architecture baked in.")
      endif()
    endif()
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    message(STATUS "OpenROAD: CUDA backend (arch=${CMAKE_CUDA_ARCHITECTURES})")
    # nvcc 12.8 cannot parse fmt 11's nontype-template-parameter user-defined
    # literals (fmt/bundled/format.h: operator""_a with fixed_string). The
    # legacy literal fallback is still available; opt into it for CUDA TUs
    # only. Project-wide CXX compilation is unaffected.
    add_compile_definitions(
      $<$<COMPILE_LANGUAGE:CUDA>:FMT_USE_NONTYPE_TEMPLATE_ARGS=0>)
  elseif(Kokkos_ENABLE_HIP)
    enable_language(HIP)
    message(STATUS "OpenROAD: HIP backend")
  elseif(Kokkos_ENABLE_SYCL)
    message(STATUS "OpenROAD: SYCL backend (driven by Kokkos host compiler)")
  else()
    message(STATUS
            "OpenROAD: host-only Kokkos backend (Serial / OpenMP / Threads)")
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.3.0")
    message(FATAL_ERROR "Insufficient gcc version. Found ${CMAKE_CXX_COMPILER_VERSION}, but require  >= 8.3.0.")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.0.0")
    message(FATAL_ERROR "Insufficient Clang version. Found ${CMAKE_CXX_COMPILER_VERSION}, but require >= 7.0.0.")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0.0")
    message(FATAL_ERROR "Insufficient AppleClang version. Found ${CMAKE_CXX_COMPILER_VERSION}, but require >= 12.0.0.")
  endif()
else()
  message(WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} is not officially supported.")
endif()

message(STATUS "System name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "C++ Standard Required: ${CMAKE_CXX_STANDARD_REQUIRED}")
message(STATUS "C++ Extensions: ${CMAKE_CXX_EXTENSIONS}")

# Detect OS distribution and version from /etc/os-release
# LTO is known to cause build errors on Rocky Linux 8
set(LTO_UNSUPPORTED_OS FALSE)
if(EXISTS "/etc/os-release")
  file(STRINGS "/etc/os-release" OS_RELEASE_CONTENTS)
  set(OS_ID "")
  set(OS_VERSION_ID "")
  foreach(line ${OS_RELEASE_CONTENTS})
    if(line MATCHES "^ID=(.*)$")
      string(REGEX REPLACE "^ID=\"?([^\"]*)\"?$" "\\1" OS_ID "${line}")
    endif()
    if(line MATCHES "^VERSION_ID=(.*)$")
      string(REGEX REPLACE "^VERSION_ID=\"?([^\"]*)\"?$" "\\1" OS_VERSION_ID "${line}")
    endif()
  endforeach()

  # Check for unsupported OS for LTO
  if(OS_ID STREQUAL "rocky" AND OS_VERSION_ID MATCHES "^8")
    set(LTO_UNSUPPORTED_OS TRUE)
    message(STATUS "Detected unsupported OS (${OS_ID} ${OS_VERSION_ID}): LTO will be disabled due to known build issues")
  endif()
endif()

option(LINK_TIME_OPTIMIZATION "Flag to control link time optimization: on by default" ON)
set(LTO_STATUS "disabled")
if (LINK_TIME_OPTIMIZATION AND NOT LTO_UNSUPPORTED_OS)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
  if(lto_supported)
    # LTO is enabled globally for Release build
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)

    string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPER_BUILD_TYPE)
    if(UPPER_BUILD_TYPE STREQUAL "RELEASE")
      set(LTO_STATUS "enabled")
    endif()
  else()
    set(LTO_STATUS "not supported: ${lto_error}")
  endif()
endif()
message(STATUS "LTO/IPO is ${LTO_STATUS}")

# configure a header file to pass some of the CMake settings
configure_file(
  ${OPENROAD_HOME}/include/ord/Version.hh.cmake
  ${OPENROAD_HOME}/include/ord/Version.hh
  )


################################################################

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1")
  MESSAGE(STATUS "Older version of GCC detected. Linking against stdc++fs")
  link_libraries(stdc++fs)
endif()

# Ask CMake to output a compile_commands.json file for use with things like clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_subdirectory(third-party)

if(ENABLE_TESTS)
  find_program (BASH_PROGRAM bash REQUIRED)

  enable_testing()
  add_custom_target(build_and_test ${CMAKE_CTEST_COMMAND} --parallel --output-on-failure -LE IntegrationTest)
  include(GoogleTest)
endif()

add_subdirectory(src)
add_subdirectory(test)

# Not currently used but may become useful again with Kokkos
target_compile_definitions(openroad PRIVATE GPU)

if(BUILD_PYTHON)
  target_compile_definitions(openroad PRIVATE BUILD_PYTHON=1)
else()
  target_compile_definitions(openroad PRIVATE BUILD_PYTHON=0)
endif()

if(BUILD_GUI)
  target_compile_definitions(openroad PRIVATE BUILD_GUI=1)
else()
  target_compile_definitions(openroad PRIVATE BUILD_GUI=0)
endif()

####################################################################

# Build man pages (Optional)

option(BUILD_MAN "Enable building man pages" OFF)
if(BUILD_MAN)
  message(STATUS "man is enabled")

  # Use the processor_count command to get the number of cores
  include(ProcessorCount)
  ProcessorCount(PROCESSOR_COUNT)
  message(STATUS "Number of processor cores: ${PROCESSOR_COUNT}")

  add_custom_target(
    man_page ALL
    COMMAND make clean && make preprocess && make all -j${PROCESSOR_COUNT}
    WORKING_DIRECTORY ${OPENROAD_HOME}/docs
  )
  
  # Based on ${CMAKE_INSTALL_PREFIX}, we want to go to ${CMAKE_INSTALL_PREFIX}/share/man
  install(DIRECTORY ${OPENROAD_HOME}/docs/cat DESTINATION ${OPENROAD_SHARE}/man)
  install(DIRECTORY ${OPENROAD_HOME}/docs/html DESTINATION ${OPENROAD_SHARE}/man)
endif()

####################################################################
# Add a configuration for ReleaseWithAsserts
set(CMAKE_CXX_FLAGS_RELEASEWITHASSERTS "${CMAKE_CXX_FLAGS_RELEASE} -UNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASEWITHASSERTS "${CMAKE_C_FLAGS_RELEASE} -UNDEBUG" CACHE STRING "" FORCE)

# ==============================================================================
# OPENROAD BUILD CONFIGURATION SUMMARY
# ==============================================================================
message(STATUS "")
message(STATUS "=========================================================")
message(STATUS " OpenROAD Configuration Summary ")
message(STATUS "=========================================================")
message(STATUS " C++ Standard   : ${CMAKE_CXX_STANDARD}")
message(STATUS " Build Type     : ${CMAKE_BUILD_TYPE}")
message(STATUS " GUI Support    : ${BUILD_GUI}")
message(STATUS " Python Bindings: ${BUILD_PYTHON}")
message(STATUS " Tests Enabled  : ${ENABLE_TESTS}")
message(STATUS "=========================================================")
message(STATUS " Configuration complete.")
message(STATUS "")
