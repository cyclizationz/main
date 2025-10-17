cmake_minimum_required(VERSION 3.20)
project(hidra_daemon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})

# msquic
find_path(MSQUIC_INCLUDE_DIR msquic.h PATHS /usr/local/include /usr/include)
find_library(MSQUIC_LIB msquic PATHS /usr/local/lib /usr/lib)
if (MSQUIC_INCLUDE_DIR AND MSQUIC_LIB)
  add_definitions(-DHAVE_MSQUIC)
  include_directories(${MSQUIC_INCLUDE_DIR})
  message(STATUS "Found msquic")
else()
  message(WARNING "msquic not found; building without QUIC")
endif()

file(GLOB SRC src/*.cpp)
add_executable(daemon ${SRC})
target_include_directories(daemon PRIVATE include)
target_link_libraries(daemon PRIVATE Threads::Threads ${Protobuf_LIBRARIES} ${Boost_LIBRARIES})
if (MSQUIC_LIB)
  target_link_libraries(daemon PRIVATE ${MSQUIC_LIB} ssl crypto)
endif()
