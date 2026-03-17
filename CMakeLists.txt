cmake_minimum_required(VERSION 3.1)

project(SDFilter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
include(FetchContent)
include(CheckLanguage)


# Additional compiler flags
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	message("Clang compiler found.")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -mtune=native")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
	message("AppleClang compiler found.")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -mtune=native")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	message("GNU compiler found.")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -mtune=native")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    message("MSVC compiler found.")
    add_definitions(/DUSE_MSVC)
    add_definitions(/D_USE_MATH_DEFINES)
endif()

# Detect OpenMP environment
set(OPENMP ON CACHE BOOL "OpenMP")
if(OPENMP)
  find_package(OpenMP QUIET)
  if(OPENMP_FOUND)
      message("OpenMP found. OpenMP activated in release.")
      add_definitions(-DUSE_OPENMP)

  else()
      message("OpenMP not found.")
  endif()
endif()


# Detect Eigen3 (header-only): prefer local external/eigen, fallback to FetchContent
set(EIGEN_INCLUDE_DIR "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/eigen/Eigen/Dense")
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/external/eigen"
  )
  set(EIGEN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/eigen")
  message("Using Eigen from ${CMAKE_CURRENT_SOURCE_DIR}/external/eigen.")
else()
  FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
  )
  FetchContent_GetProperties(eigen)
  if(NOT eigen_POPULATED)
    FetchContent_Populate(eigen)
  endif()

  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${eigen_SOURCE_DIR}"
  )
  set(EIGEN_INCLUDE_DIR "${eigen_SOURCE_DIR}")
  message("Using Eigen from FetchContent at ${eigen_SOURCE_DIR}.")
endif()

# rapidOBJ
FetchContent_Declare(
  rapidobj
  GIT_REPOSITORY https://github.com/guybrush77/rapidobj.git
  GIT_TAG v1.1
)
FetchContent_GetProperties(rapidobj)
if(NOT rapidobj_POPULATED)
  FetchContent_Populate(rapidobj)
endif()

add_library(rapidobj INTERFACE)
target_include_directories(rapidobj INTERFACE "${rapidobj_SOURCE_DIR}/include")

# OpenMesh library
set(OPENMESH_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/OpenMesh")
file(GLOB_RECURSE OPENMESH_HEADERS
  "${OPENMESH_SOURCE_DIR}/Core/Geometry/*.hh"
  "${OPENMESH_SOURCE_DIR}/Core/Geometry/*.h"
  "${OPENMESH_SOURCE_DIR}/Core/Mesh/*.hh"
  "${OPENMESH_SOURCE_DIR}/Core/Mesh/*.h"
  "${OPENMESH_SOURCE_DIR}/Core/System/*.hh"
  "${OPENMESH_SOURCE_DIR}/Core/System/*.h"
  "${OPENMESH_SOURCE_DIR}/Core/Utils/*.hh"
  "${OPENMESH_SOURCE_DIR}/Core/Utils/*.h"
)
file(GLOB_RECURSE OPENMESH_SOURCES
  "${OPENMESH_SOURCE_DIR}/Core/Geometry/*.cc"
  "${OPENMESH_SOURCE_DIR}/Core/Mesh/*.cc"
  "${OPENMESH_SOURCE_DIR}/Core/System/*.cc"
  "${OPENMESH_SOURCE_DIR}/Core/Utils/*.cc"
)
add_library(OpenMesh STATIC ${OPENMESH_HEADERS} ${OPENMESH_SOURCES})
target_include_directories(OpenMesh PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external")

target_compile_definitions(OpenMesh PUBLIC OM_STATIC_BUILD)

add_library(MeshSDLibrary INTERFACE)
target_include_directories(MeshSDLibrary INTERFACE
                           ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(MeshSDLibrary
    INTERFACE
      OpenMesh
      Eigen3::Eigen
      rapidobj
)

# Optional SuiteSparse CHOLMOD backend
option(ENABLE_CHOLMOD "Enable SuiteSparse CHOLMOD backend when available" ON)
set(SDFILTER_ENABLE_CHOLMOD OFF)
set(SDFILTER_CHOLMOD_LINK_LIBS "")
if(ENABLE_CHOLMOD)
  find_package(CHOLMOD CONFIG QUIET)
  if(CHOLMOD_FOUND OR TARGET CHOLMOD::CHOLMOD)
    set(SDFILTER_ENABLE_CHOLMOD ON)
    add_compile_definitions(USE_CHOLMOD)
    message("CHOLMOD package found. Enabling CHOLMOD linear-solver backend.")
    if(TARGET CHOLMOD::CHOLMOD)
      list(APPEND SDFILTER_CHOLMOD_LINK_LIBS CHOLMOD::CHOLMOD)
    elseif(TARGET CHOLMOD)
      list(APPEND SDFILTER_CHOLMOD_LINK_LIBS CHOLMOD)
    elseif(DEFINED CHOLMOD_LIBRARY)
      list(APPEND SDFILTER_CHOLMOD_LINK_LIBS ${CHOLMOD_LIBRARY})
    elseif(DEFINED CHOLMOD_LIBRARIES)
      list(APPEND SDFILTER_CHOLMOD_LINK_LIBS ${CHOLMOD_LIBRARIES})
    endif()
    if(DEFINED CHOLMOD_INCLUDE)
      target_include_directories(MeshSDLibrary INTERFACE ${CHOLMOD_INCLUDE})
    elseif(DEFINED CHOLMOD_INCLUDE_DIR)
      target_include_directories(MeshSDLibrary INTERFACE ${CHOLMOD_INCLUDE_DIR})
    elseif(DEFINED CHOLMOD_INCLUDE_DIRS)
      target_include_directories(MeshSDLibrary INTERFACE ${CHOLMOD_INCLUDE_DIRS})
    endif()
  else()
    find_package(SuiteSparse QUIET)
    if(SuiteSparse_FOUND)
      set(SDFILTER_ENABLE_CHOLMOD ON)
      add_compile_definitions(USE_CHOLMOD)
      message("SuiteSparse found. Enabling CHOLMOD linear-solver backend.")
      if(TARGET SuiteSparse::CHOLMOD)
        list(APPEND SDFILTER_CHOLMOD_LINK_LIBS SuiteSparse::CHOLMOD)
      elseif(TARGET cholmod)
        list(APPEND SDFILTER_CHOLMOD_LINK_LIBS cholmod)
      elseif(DEFINED CHOLMOD_LIBRARIES)
        list(APPEND SDFILTER_CHOLMOD_LINK_LIBS ${CHOLMOD_LIBRARIES})
      endif()
      if(DEFINED SuiteSparse_INCLUDE_DIRS)
        target_include_directories(MeshSDLibrary INTERFACE ${SuiteSparse_INCLUDE_DIRS})
      elseif(DEFINED SUITESPARSE_INCLUDE_DIRS)
        target_include_directories(MeshSDLibrary INTERFACE ${SUITESPARSE_INCLUDE_DIRS})
      elseif(DEFINED CHOLMOD_INCLUDE_DIRS)
        target_include_directories(MeshSDLibrary INTERFACE ${CHOLMOD_INCLUDE_DIRS})
      endif()
    else()
      message("SuiteSparse/CHOLMOD not found. CHOLMOD backend disabled.")
    endif()
  endif()
  if(SDFILTER_ENABLE_CHOLMOD AND (DEFINED CHOLMOD_INCLUDE_DIRS))
    target_include_directories(MeshSDLibrary INTERFACE ${CHOLMOD_INCLUDE_DIRS})
  endif()
endif()

# Executable for denoising
add_executable(MeshDenoiser
  EigenTypes.h
  MeshTypes.h
  AppMetrics.h
  MeshIO.h
  MeshIO.cpp
  SDFilter.h
  MeshNormalFilter.h
  MeshNormalDenoising.h
  MeshDenoiser.cpp
)
target_link_libraries(MeshDenoiser MeshSDLibrary)

if(SDFILTER_ENABLE_CHOLMOD AND SDFILTER_CHOLMOD_LINK_LIBS)
  target_link_libraries(MeshDenoiser ${SDFILTER_CHOLMOD_LINK_LIBS})
endif()

if(OPENMP_FOUND)
  target_compile_definitions(MeshDenoiser PUBLIC "$<$<CONFIG:RELEASE>:USE_OPENMP>")
  target_link_libraries(MeshDenoiser OpenMP::OpenMP_CXX)
endif()
