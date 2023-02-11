cmake_minimum_required (VERSION 3.13)
if (MSVC)
set (CMAKE_SYSTEM_VERSION 10.0.22000.0)
endif (MSVC)
project (KMLHelper VERSION ${PROMETHEUS_VERSION} DESCRIPTION "KML Helper Library")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)

SET(XERCES_C_INCLUDE_DIR "error" CACHE STRING "The path to the includes from Xerces-c")
SET(LOWLEVEL_INCLUDE_DIR "error" CACHE STRING "The path to the includes from HSS LowLevel")
SET(WTIME_INCLUDE_DIR "error" CACHE STRING "The path to the WTime libraries includes")
SET(MULTITHREAD_INCLUDE_DIR "error" CACHE STRING "The path to the includes from HSS Multithread")
SET(MATH_INCLUDE_DIR "error" CACHE STRING "The path to HSS_Math include files")
SET(THIRD_PARTY_INCLUDE_DIR "error" CACHE STRING "The path to third party include files")

find_library(FOUND_XERCES_LIBRARY_PATH NAMES xerces xerces-c_3 xerces-c REQUIRED PATHS ${GDAL_LIBRARY_DIR})
if (MSVC)
find_library(FOUND_ZLIB_LIBRARY_PATH NAMES zlib z REQUIRED PATHS ${GDAL_LIBRARY_DIR})
find_library(FOUND_MINIZIP_LIBRARY_PATH NAMES minizip REQUIRED PATHS ${GDAL_LIBRARY_DIR})
endif (MSVC)
find_library(FOUND_WTIME_LIBRARY_PATH NAMES WTime REQUIRED PATHS ${LOCAL_LIBRARY_DIR})
find_library(FOUND_LOWLEVEL_LIBRARY_PATH NAMES LowLevel REQUIRED PATHS ${LOCAL_LIBRARY_DIR})

if (MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -DPROTOBUF_USE_DLLS -DBOOST_ALL_DYN_LINK -D_CRT_SECURE_NO_WARNINGS /Zc:__cplusplus")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD -D_AFXDLL")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd -D_AFXDLL /W4")
else ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_NO_MFC")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
endif (MSVC)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")

add_library(kmllib SHARED
    cpp/kmlinternal.cpp
    include/kmlinternal.h
    cpp/kmllib.cpp
    include/kmllib.h
)

target_include_directories(kmllib
    PUBLIC ${WTIME_INCLUDE_DIR}
    PUBLIC ${XERCES_C_INCLUDE_DIR}
    PUBLIC ${LOWLEVEL_INCLUDE_DIR}
    PUBLIC ${MULTITHREAD_INCLUDE_DIR}
    PUBLIC ${MATH_INCLUDE_DIR}
    PUBLIC ${THIRD_PARTY_INCLUDE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(kmllib PROPERTIES VERSION ${CMAKE_PROJECT_VERSION})
set_target_properties(kmllib PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR})
set_target_properties(kmllib PROPERTIES DEFINE_SYMBOL "KML_LIB_EXPORTS")

set_target_properties(kmllib PROPERTIES PUBLIC_HEADER include/kmllib.h)

target_link_libraries(kmllib ${FOUND_XERCES_LIBRARY_PATH} ${FOUND_WTIME_LIBRARY_PATH} ${FOUND_LOWLEVEL_LIBRARY_PATH})
if (MSVC)
target_link_libraries(kmllib ${FOUND_ZLIB_LIBRARY_PATH} ${FOUND_MINIZIP_LIBRARY_PATH})
else ()
target_link_libraries(kmllib -lstdc++fs)
endif (MSVC)
