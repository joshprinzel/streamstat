cmake_minimum_required(VERSION 3.21)
project(streamfeat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# fastnum project (disable its tests/examples/bench when used as a dependency)
set(FASTNUM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FASTNUM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FASTNUM_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/fastnum)


find_package(Threads REQUIRED)

# IMPORTANT: no CONFIG here — use CMake's FindProtobuf module (works with apt)
find_package(Protobuf REQUIRED)

# gRPC via pkg-config (works well with apt)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPCPP REQUIRED grpc++)

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

set(PROTO_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_NAME "streamfeat.proto")
set(PROTO_FILE "${PROTO_DIR}/${PROTO_NAME}")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Create output dir at build time (avoids quoting/dir timing issues)
add_custom_target(streamfeat_gen_dir
  COMMAND ${CMAKE_COMMAND} -E make_directory "${GEN_DIR}"
)

set(PB_CC   "${GEN_DIR}/streamfeat.pb.cc")
set(PB_H    "${GEN_DIR}/streamfeat.pb.h")
set(GRPC_CC "${GEN_DIR}/streamfeat.grpc.pb.cc")
set(GRPC_H  "${GEN_DIR}/streamfeat.grpc.pb.h")

add_custom_command(
  OUTPUT "${PB_CC}" "${PB_H}"
  COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
  ARGS -I "${PROTO_DIR}"
       --cpp_out "${GEN_DIR}"
       "${PROTO_NAME}"
  WORKING_DIRECTORY "${PROTO_DIR}"
  DEPENDS streamfeat_gen_dir "${PROTO_FILE}"
  VERBATIM
)

add_custom_command(
  OUTPUT "${GRPC_CC}" "${GRPC_H}"
  COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
  ARGS -I "${PROTO_DIR}"
       --grpc_out "${GEN_DIR}"
       --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
       "${PROTO_NAME}"
  WORKING_DIRECTORY "${PROTO_DIR}"
  DEPENDS streamfeat_gen_dir "${PROTO_FILE}" "${PB_CC}" "${PB_H}"
  VERBATIM
)



add_executable(streamfeat_server
  src/main.cpp
  "${PB_CC}" "${GRPC_CC}"
)

target_include_directories(streamfeat_server PRIVATE
  "${GEN_DIR}"
  ${Protobuf_INCLUDE_DIRS}
  ${GRPCPP_INCLUDE_DIRS}
)

target_link_directories(streamfeat_server PRIVATE
  ${GRPCPP_LIBRARY_DIRS}
)

target_link_libraries(streamfeat_server PRIVATE
  Threads::Threads
  ${Protobuf_LIBRARIES}
  ${GRPCPP_LIBRARIES}
  fastnum::fastnum
)

# Optional: makes warnings from pkg-config not spam the output
target_compile_options(streamfeat_server PRIVATE ${GRPCPP_CFLAGS_OTHER})

add_library(streamfeat_core
  src/core/window_state.cpp  
)

target_include_directories(streamfeat_core PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(streamfeat_core PUBLIC fastnum::fastnum)
# ----------------------------
# Tests (Catch2)
# ----------------------------
option(STREAMFEAT_BUILD_TESTS "Build streamfeat tests" ON)

if (STREAMFEAT_BUILD_TESTS)
  include(CTest)
  enable_testing()

  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
  )
  FetchContent_MakeAvailable(Catch2)

  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp
  )

  add_executable(streamfeat_tests ${TEST_SOURCES})

  target_link_libraries(streamfeat_tests PRIVATE
    Catch2::Catch2WithMain
    streamfeat_core
  )

  target_include_directories(streamfeat_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  # Nice to keep warnings consistent (optional)
  target_compile_options(streamfeat_tests PRIVATE ${GRPCPP_CFLAGS_OTHER})

  # Catch2 test discovery
  list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
  include(Catch)
  catch_discover_tests(streamfeat_tests)
endif()

