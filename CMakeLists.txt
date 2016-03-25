cmake_minimum_required(VERSION 2.8)

set (PROJECT asio-libs) 
 
project (${PROJECT}) 

# Setup list of files to build
file(GLOB HEADERS_L
    "lib/*.hpp"
)
file(GLOB SOURCES_L
    "src/*.cpp"
)

set (HEADERS ${HEADERS_L})
set (SOURCES ${SOURCES_L}) 

# Setup link/include deps
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS system thread coroutine REQUIRED)

include_directories("lib")
include_directories(${Boost_INCLUDE_DIRS})

#target_link_libraries(test ${Boost_LIBRARIES})

# Setup build compiler/linker options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -ggdb3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

add_library(${PROJECT} STATIC ${SOURCES})
