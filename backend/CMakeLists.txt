cmake_minimum_required(VERSION 3.20)
project(currency_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)
find_package(Poco REQUIRED COMPONENTS Net NetSSL JSON Util Foundation)

# {fmt} stands in for <format>: bookworm's default g++ (GCC 12) ships a
# libstdc++ that doesn't have <format> yet (completed in libstdc++ 13), and
# there's no newer GCC available for bookworm without pulling in a second,
# incompatible distro release. libfmt-dev ships a CMake config on both the
# Debian/apt path and Nix (flake.nix), so this needs no pkg-config fallback.
find_package(fmt REQUIRED)

# ── gRPC / Protobuf ──────────────────────────────────────────────────────
# Two supported toolchains, detected automatically:
#   - Nix (flake.nix devShell): grpc/protobuf ship proper CMake config
#     files, so find_package(... CONFIG) finds them, and gRPC's own
#     find_dependency() pulls in a matching Protobuf::* target set --
#     calling find_package(Protobuf) ourselves first would cause a
#     partial-redefinition error against that pair, so it's deliberately
#     not called on this path.
#   - Debian/apt (backend/Dockerfile): libgrpc++-dev and libprotobuf-dev
#     only ship pkg-config (.pc) files, no CMake config -- so this falls
#     back to PkgConfig and builds equivalent gRPC::grpc++ /
#     gRPC::grpc++_reflection / protobuf::libprotobuf imported targets by
#     hand, so every target below can link against the same names either way.
find_package(gRPC CONFIG)
if(gRPC_FOUND)
  message(STATUS "Using gRPC/Protobuf via CMake config (Nix)")
else()
  message(STATUS "gRPC CMake config not found -- falling back to pkg-config (Debian/apt)")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GRPCXX REQUIRED IMPORTED_TARGET grpc++)
  pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)
  find_library(GRPCXX_REFLECTION_LIB grpc++_reflection REQUIRED)

  add_library(gRPC::grpc++ ALIAS PkgConfig::GRPCXX)
  add_library(protobuf::libprotobuf ALIAS PkgConfig::PROTOBUF)

  add_library(gRPC::grpc++_reflection UNKNOWN IMPORTED)
  set_target_properties(gRPC::grpc++_reflection PROPERTIES IMPORTED_LOCATION "${GRPCXX_REFLECTION_LIB}")
  target_link_libraries(gRPC::grpc++_reflection INTERFACE PkgConfig::GRPCXX)
endif()

# ── Proto codegen ────────────────────────────────────────────────────────
# Single source of truth lives at ../proto/currency.proto. Generated code
# is never committed; it is produced fresh on every build.

set(PROTO_FILE ${CMAKE_SOURCE_DIR}/../proto/currency.proto)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(PROTO_SRCS ${GENERATED_DIR}/currency.pb.cc)
set(PROTO_HDRS ${GENERATED_DIR}/currency.pb.h)
set(GRPC_SRCS ${GENERATED_DIR}/currency.grpc.pb.cc)
set(GRPC_HDRS ${GENERATED_DIR}/currency.grpc.pb.h)

find_program(PROTOC_EXECUTABLE protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
  COMMAND ${PROTOC_EXECUTABLE}
  ARGS --cpp_out=${GENERATED_DIR}
       --grpc_out=${GENERATED_DIR}
       --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
       -I ${CMAKE_SOURCE_DIR}/../proto
       ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating C++ protobuf/gRPC stubs from currency.proto"
  VERBATIM)

add_library(currency_proto STATIC ${PROTO_SRCS} ${GRPC_SRCS})
target_include_directories(currency_proto PUBLIC ${GENERATED_DIR})
target_link_libraries(currency_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)

# ── Application sources ──────────────────────────────────────────────────

add_library(currency_server_lib STATIC
  src/config/AppConfig.cpp
  src/external/FrankfurterClient.cpp
  src/application/CurrencyCatalogService.cpp
  src/application/ConversionService.cpp
  src/application/HistoryService.cpp
  src/application/ConstraintsService.cpp
  src/grpc/CurrencyServiceImpl.cpp
)

target_include_directories(currency_server_lib PUBLIC src)

target_link_libraries(currency_server_lib PUBLIC
  currency_proto
  gRPC::grpc++
  gRPC::grpc++_reflection
  protobuf::libprotobuf
  Poco::Net
  Poco::NetSSL
  Poco::JSON
  Poco::Util
  Poco::Foundation
  Threads::Threads
  fmt::fmt
)

add_executable(currency_server src/main.cpp)
target_link_libraries(currency_server PRIVATE currency_server_lib)

install(TARGETS currency_server RUNTIME DESTINATION bin)
