cmake_minimum_required(VERSION 3.21)
project(streamfeat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# ----------------------------
# Dependencies
# ----------------------------

# fastnum project (disable its tests/examples/bench when used as a dependency)
set(FASTNUM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FASTNUM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FASTNUM_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/fastnum)

# ----------------------------
# Options
# ----------------------------
option(STREAMFEAT_BUILD_TESTS "Build streamfeat tests" ON)
option(STREAMFEAT_BUILD_SERVER "Build gRPC server" ON)
option(STREAMFEAT_BUILD_BENCH  "Build benchmarks/loadgen" ON)

# ----------------------------
# Library: core + runtime (NO gRPC)
# ----------------------------

add_library(streamfeat
  # core
  src/core/window_state.cpp

  # runtime (add as you implement)
  # src/runtime/shard.cpp
  # src/runtime/backend.cpp
)

# Prefer an include/ directory long-term; for now you can keep src public.
# If you adopt include/streamfeat/..., change this to ${CMAKE_CURRENT_SOURCE_DIR}/include.
target_include_directories(streamfeat
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)


target_link_libraries(streamfeat PUBLIC
  fastnum::fastnum
  Threads::Threads
)

# (Optional but recommended) warnings for your code
target_compile_options(streamfeat PRIVATE -Wall -Wextra -Wpedantic)

# ----------------------------
# Bench/loadgen executable (NO gRPC)
# ----------------------------
if (STREAMFEAT_BUILD_BENCH)
  add_executable(streamfeat_loadgen
    bench/loadgen_main.cpp
  )
  target_link_libraries(streamfeat_loadgen PRIVATE streamfeat)
endif()

# ----------------------------
# Server executable (gRPC) - optional
# ----------------------------
if (STREAMFEAT_BUILD_SERVER)
  # Protobuf + gRPC only needed for server build
  find_package(Protobuf REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GRPCPP REQUIRED grpc++)
  find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

  set(PROTO_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/proto")
  set(PROTO_NAME "streamfeat.proto")
  set(PROTO_FILE "${PROTO_DIR}/${PROTO_NAME}")

  set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_directories(streamfeat_server PRIVATE
    ${GRPCPP_LIBRARY_DIRS}
  )

  # IMPORTANT: link your streamfeat library here
  target_link_libraries(streamfeat_server PRIVATE
    streamfeat
    ${Protobuf_LIBRARIES}
    ${GRPCPP_LIBRARIES}
  )

  target_compile_options(streamfeat_server PRIVATE
    ${GRPCPP_CFLAGS_OTHER}
  )
endif()

# ----------------------------
# Tests (Catch2) - optional
# ----------------------------
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
    streamfeat
  )

  target_include_directories(streamfeat_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)


  list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
  include(Catch)
  catch_discover_tests(streamfeat_tests)
endif()
