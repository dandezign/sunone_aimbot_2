cmake_minimum_required(VERSION 3.18)

project(sam3_trt LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(SAM3_WORKSPACE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SAM3_AIMBOT_MODULES_ROOT "${SAM3_WORKSPACE_ROOT}/sunone_aimbot_2/modules")

set(TensorRT_ROOT "${SAM3_AIMBOT_MODULES_ROOT}/TensorRT-10.14.1.48" CACHE PATH "Path to TensorRT root (contains include/ and lib/)")
set(SAM3_OPENCV_ROOT "${SAM3_AIMBOT_MODULES_ROOT}/opencv/build/install" CACHE PATH "Path to OpenCV install root")

if(WIN32)
  if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()

if(NOT CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES native)
endif()

if(NOT WIN32 AND NOT CMAKE_CUDA_COMPILER AND EXISTS "/usr/local/cuda/bin/nvcc")
  set(CMAKE_CUDA_COMPILER "/usr/local/cuda/bin/nvcc")
endif()

find_package(CUDAToolkit REQUIRED)
find_package(Threads REQUIRED)

set(_opencv_candidates "")
if(SAM3_OPENCV_ROOT)
  list(APPEND _opencv_candidates
    "${SAM3_OPENCV_ROOT}/x64/vc18/lib"
    "${SAM3_OPENCV_ROOT}/x64/vc17/lib"
    "${SAM3_OPENCV_ROOT}")
endif()

if(WIN32 AND (NOT OpenCV_DIR OR OpenCV_DIR STREQUAL ""))
  foreach(_cand IN LISTS _opencv_candidates)
    if(EXISTS "${_cand}/OpenCVConfig.cmake")
      set(OpenCV_DIR "${_cand}" CACHE PATH "OpenCVConfig.cmake directory" FORCE)
      break()
    endif()
  endforeach()
endif()

find_package(OpenCV QUIET CONFIG)
if(NOT OpenCV_FOUND)
  if(WIN32)
    message(FATAL_ERROR "OpenCV not found. Set OpenCV_DIR to your OpenCV CMake config path (for example: <opencv>/build/install/x64/vc18/lib).")
  endif()

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(OpenCV REQUIRED opencv4)
endif()

find_path(TENSORRT_INCLUDE_DIR
  NAMES NvInfer.h
  HINTS ${TensorRT_ROOT} $ENV{TensorRT_ROOT}
  PATH_SUFFIXES include)

find_library(TENSORRT_NVINFER_LIB
  NAMES nvinfer nvinfer_10
  HINTS ${TensorRT_ROOT} $ENV{TensorRT_ROOT}
  PATH_SUFFIXES lib lib64 lib/x64)

find_library(TENSORRT_NVONNXPARSER_LIB
  NAMES nvonnxparser nvonnxparser_10
  HINTS ${TensorRT_ROOT} $ENV{TensorRT_ROOT}
  PATH_SUFFIXES lib lib64 lib/x64)

if(NOT TENSORRT_NVINFER_LIB OR NOT TENSORRT_NVONNXPARSER_LIB)
  message(FATAL_ERROR "TensorRT libraries not found. Set TensorRT_ROOT to your TensorRT install path.")
endif()

set(EXTRA_INCLUDE_DIRS "")
if(TENSORRT_INCLUDE_DIR)
  list(APPEND EXTRA_INCLUDE_DIRS ${TENSORRT_INCLUDE_DIR})
endif()

if(DEFINED CUDNN_ROOT_DIR)
  list(APPEND EXTRA_INCLUDE_DIRS ${CUDNN_ROOT_DIR}/include)
elseif(DEFINED ENV{CUDNN_ROOT_DIR})
  list(APPEND EXTRA_INCLUDE_DIRS $ENV{CUDNN_ROOT_DIR}/include)
elseif(DEFINED ENV{CUDNN_ROOT})
  list(APPEND EXTRA_INCLUDE_DIRS $ENV{CUDNN_ROOT}/include)
endif()

set(FS_LIBS "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  list(APPEND FS_LIBS stdc++fs)
endif()

function(sam3_copy_if_exists target path)
  if(EXISTS "${path}")
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${path}" "$<TARGET_FILE_DIR:${target}>"
      VERBATIM)
  endif()
endfunction()

add_library(sam3_trt SHARED
  src/sam3/sam3_trt/sam3.cu
  src/sam3/sam3_trt/prepost.cu)

set_target_properties(sam3_trt PROPERTIES
  CUDA_SEPARABLE_COMPILATION ON
  WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(sam3_trt
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EXTRA_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS})

target_link_libraries(sam3_trt
  PUBLIC
    CUDA::cudart
    ${TENSORRT_NVINFER_LIB}
    ${TENSORRT_NVONNXPARSER_LIB}
    ${OpenCV_LIBRARIES}
    ${FS_LIBS})

add_executable(sam3_pcs_app
  src/sam3/sam3_apps/sam3_pcs_app.cpp)

set(SAM3_TOKENIZER_SCRIPT_PATH "${SAM3_WORKSPACE_ROOT}/SAM3TensorRT/python/tokenize_prompt.py")

target_compile_definitions(sam3_pcs_app PRIVATE
  SAM3_DEFAULT_TOKENIZER_SCRIPT="${SAM3_TOKENIZER_SCRIPT_PATH}")

set_target_properties(sam3_pcs_app PROPERTIES
  CUDA_SEPARABLE_COMPILATION ON)

target_link_libraries(sam3_pcs_app
  PUBLIC
    sam3_trt
    ${OpenCV_LIBRARIES}
    Threads::Threads
    ${FS_LIBS})

if(WIN32)
  sam3_copy_if_exists(sam3_pcs_app "${SAM3_TOKENIZER_SCRIPT_PATH}")
  sam3_copy_if_exists(sam3_pcs_app "${TensorRT_ROOT}/bin/nvinfer_10.dll")
  sam3_copy_if_exists(sam3_pcs_app "${TensorRT_ROOT}/bin/nvonnxparser_10.dll")
  sam3_copy_if_exists(sam3_pcs_app "${TensorRT_ROOT}/bin/nvinfer_plugin_10.dll")

  file(GLOB _trt_builder_resources "${TensorRT_ROOT}/bin/nvinfer_builder_resource_*.dll")
  foreach(_resource IN LISTS _trt_builder_resources)
    sam3_copy_if_exists(sam3_pcs_app "${_resource}")
  endforeach()

  file(GLOB _opencv_world_dll_candidates
    "${SAM3_OPENCV_ROOT}/x64/vc*/bin/opencv_world*.dll")
  if(_opencv_world_dll_candidates)
    list(SORT _opencv_world_dll_candidates COMPARE NATURAL ORDER DESCENDING)
    list(GET _opencv_world_dll_candidates 0 _opencv_world_dll)
    sam3_copy_if_exists(sam3_pcs_app "${_opencv_world_dll}")
  endif()

  if(DEFINED CUDAToolkit_BIN_DIR)
    file(GLOB _cuda_runtime_dlls
      "${CUDAToolkit_BIN_DIR}/cudart64_*.dll"
      "${CUDAToolkit_BIN_DIR}/npp*.dll"
      "${CUDAToolkit_BIN_DIR}/cublas64_*.dll"
      "${CUDAToolkit_BIN_DIR}/cublasLt64_*.dll"
      "${CUDAToolkit_BIN_DIR}/cufft64_*.dll")
    foreach(_cuda_dll IN LISTS _cuda_runtime_dlls)
      sam3_copy_if_exists(sam3_pcs_app "${_cuda_dll}")
    endforeach()
  endif()

  set(_cudnn_roots "")
  if(DEFINED CUDNN_ROOT_DIR)
    list(APPEND _cudnn_roots "${CUDNN_ROOT_DIR}")
  endif()
  if(DEFINED ENV{CUDNN_ROOT_DIR})
    list(APPEND _cudnn_roots "$ENV{CUDNN_ROOT_DIR}")
  endif()
  if(DEFINED ENV{CUDNN_ROOT})
    list(APPEND _cudnn_roots "$ENV{CUDNN_ROOT}")
  endif()
  if(DEFINED ENV{ProgramW6432})
    list(APPEND _cudnn_roots "$ENV{ProgramW6432}/NVIDIA/CUDNN/v9.20")
  endif()

  foreach(_cudnn_root IN LISTS _cudnn_roots)
    file(GLOB_RECURSE _cudnn_dlls "${_cudnn_root}/cudnn64*.dll")
    if(_cudnn_dlls)
      foreach(_cudnn_dll IN LISTS _cudnn_dlls)
        sam3_copy_if_exists(sam3_pcs_app "${_cudnn_dll}")
      endforeach()
      break()
    endif()
  endforeach()
endif()