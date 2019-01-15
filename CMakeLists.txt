cmake_minimum_required(VERSION 3.2)
PROJECT (cppTest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3")
# Find required protobuf package
find_package(Protobuf REQUIRED)
if(PROTOBUF_FOUND)
    message(STATUS "protobuf library found")
else()
    message(FATAL_ERROR "protobuf library is needed but cant be found")
endif()

find_package(Boost 1.36 REQUIRED system)
if(Boost_FOUND)
    message(STATUS "Boost library found")
else()
    message(FATAL_ERROR "Boost library is needed but cant be found")
endif()

find_package( FLTK REQUIRED )

#include_directories(${PROTOBUF_INCLUDE_DIRS})
#INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
#PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS AddressBook.proto)

include_directories(
        ${Boost_INCLUDE_DIRS}
        ${FLTK_INCLUDE_DIR}
        http
        third_party
)
#SET(SRC_LIST ws_async.cpp)
SET(SRC_LIST http_test.cpp)
#SET(SRC_LIST http_ws_async.cpp)
ADD_EXECUTABLE(cppTest ${SRC_LIST})


target_link_libraries(cppTest ${Boost_LIBRARIES} ${FLTK_LIBRARIES})
 
