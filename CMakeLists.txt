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

find_package(Boost 1.68 REQUIRED system filesystem iostreams)
if(Boost_FOUND)
    message(STATUS "Boost library found")
else()
    message(FATAL_ERROR "Boost library is needed but cant be found")
endif()

include_directories(${PROTOBUF_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS protobuf/game.proto)

find_package(SDL REQUIRED)
find_package(SDL_image REQUIRED)

find_package(yaml-cpp REQUIRED)

include_directories(
        ${Boost_INCLUDE_DIRS}
        include
        third_party
        tests
        ${SDL_image_INCLUDE_DIRS}
        ${YAML_CPP_INCLUDE_DIRS}
        ${PROTO_HDRS}
)

message(STATUS "Boost library found  ${yaml-cpp_LIBRARIES}")

SET(SRC_LIST
        ${PROTO_SRCS}
#        tests/http_test.cpp
#        tests/map_loader.cpp
#        tests/databus_test.cpp
#        tests/databus_test1.cpp
        protobuf/protobuf_test.cpp
        )
#SET(SRC_LIST http_ws_async.cpp)
ADD_EXECUTABLE(cppTest ${SRC_LIST})


target_link_libraries(cppTest
        ${Boost_LIBRARIES}
        ${SDL_LIBRARY}
        ${SDL_IMAGE_LIBRARIES}
        ${YAML_CPP_LIBRARIES}
        ${PROTOBUF_LIBRARIES}
        )
 
