# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, The OpenROAD Authors

# GPU backend sources for src/gpl. Built only when ENABLE_GPU=ON; the parent
# src/gpl/CMakeLists.txt invokes add_subdirectory(src/gpu) under that gate,
# and the matching CPU implementations (../hpwl.cpp, ...) stay out of the
# build. CMake-enforced ODR keeps exactly one definition per build.

target_sources(gpl_lib PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/hpwl.cpp
)

# Private gpl headers (nesterovBase.h, placerBase.h, ...) live in the parent
# src/gpl/src/ directory; expose it to gpl_lib so kernels under src/gpu/
# can use plain "..h" includes without "../"-style relative paths.
target_include_directories(gpl_lib PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Tag kernel sources with the right CMake language. Source-file properties
# are scoped to the directory that calls set_source_files_properties;
# gpl_lib was created in the parent directory, so TARGET_DIRECTORY routes
# the property to where gpl_lib's sources are actually compiled.
if(Kokkos_ENABLE_CUDA)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/hpwl.cpp
    TARGET_DIRECTORY gpl_lib
    PROPERTIES LANGUAGE CUDA)
elseif(Kokkos_ENABLE_HIP)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/hpwl.cpp
    TARGET_DIRECTORY gpl_lib
    PROPERTIES LANGUAGE HIP)
endif()

# Mirror sgizler 80b04e1c1 / 678a69199 / Issue #10336 stability work:
# disable FP contraction so kernels are bit-stable across compilers
# (clang vs LLVM-from-BCR).
target_compile_options(gpl_lib PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-ffp-contract=off>
  $<$<COMPILE_LANGUAGE:CUDA>:--fmad=false>
  $<$<COMPILE_LANGUAGE:HIP>:-ffp-contract=off>
)

target_link_libraries(gpl_lib Kokkos::kokkos)
if(Kokkos_ENABLE_CUDA)
  # CUDA runtime symbols (cudaLaunchKernel, cudaSetDevice, ...) are referenced
  # from the CUDA TU; make cudart visible to consumers (the openroad binary)
  # via the static-archive link path. CUDAToolkit is discovered at project
  # root when ENABLE_GPU=ON and Kokkos_ENABLE_CUDA.
  target_link_libraries(gpl_lib CUDA::cudart)
endif()
