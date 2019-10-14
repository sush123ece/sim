cmake_minimum_required(VERSION 3.0.2)
set (CMAKE_CXX_STANDARD 11)
project(sim)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
endif()

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES_FATTREE datacentre/fattree)
set(SOURCE_FILES_F10 datacentre/F10)
set(SOURCE_FILES_ASPEN datacentre/Aspen)

set(SOURCE_FILES_MPTCP mptcp)

include_directories(${SOURCE_FILES_MPTCP} ${SOURCE_FILES_FATTREE} ${SOURCE_FILES_F10} ${SOURCE_FILES_ASPEN})

add_subdirectory(${SOURCE_FILES_FATTREE})
add_subdirectory(${SOURCE_FILES_ASPEN})
add_subdirectory(${SOURCE_FILES_F10})
add_subdirectory(${SOURCE_FILES_MPTCP})

add_executable(mainDynamic mainDynamic.cpp)
add_executable(mainSteady mainSteady.cpp)
target_link_libraries (mainDynamic LINK_PUBLIC mptcp f10 fattree aspen)
target_link_libraries (mainSteady LINK_PUBLIC mptcp f10 fattree aspen)