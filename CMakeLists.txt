cmake_minimum_required(VERSION 2.8)

set (PROJECT asio-libs) 
 
project (${PROJECT}) 

# Setup list of files to build
file(GLOB HEADERS_L RELATIVE path "lib/*.hpp")
file(GLOB SOURCES_L "src/*.cpp")
file(GLOB TEST_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "unit_tests/*.cpp")

set (HEADERS ${HEADERS_L}
	lib/picohttpparser.h)
set (SOURCES ${SOURCES_L}
	src/picohttpparser.c)

enable_testing()

# Setup link/include deps
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.56 COMPONENTS system thread coroutine REQUIRED)

include_directories("lib")
include_directories(${Boost_INCLUDE_DIRS})

# Setup build compiler/linker options
set(COMPILE_OPT_FLAGS "-O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -ggdb3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILE_OPT_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Werror -ggdb3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMPILE_OPT_FLAGS}")

#Suppress warning for my mac+clang+boost
if (${APPLE})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef")
endif (${APPLE})

add_library(${PROJECT} STATIC ${SOURCES})
SET_TARGET_PROPERTIES(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)

foreach( testsourcefile ${TEST_SOURCES} )
    # Simple string replace, to cut off .cpp.
    string( REPLACE ".cpp" "" testname_t ${testsourcefile} )
    string( REPLACE "unit_tests/" "" testname ${testname_t} )

    add_executable( ${testname} ${testsourcefile} )

    # Make sure project lib is linked to each app
    target_link_libraries( ${testname} ${PROJECT} )
    target_link_libraries( ${testname} ${Boost_LIBRARIES} )
    add_test(NAME ${testname} COMMAND ./unit_tests/${testname})
    set_target_properties(${testname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/unit_tests")
endforeach( testsourcefile ${TEST_SOURCES} )
