cmake_minimum_required(VERSION 3.20)
project(m3u8dl VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG FALSE)
set(CMAKE_PREFER_SYSTEM_LIBRARIES ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic -O3)
endif()

# Disable LTO by default to avoid ar wrapper issues
# Enable with -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON if desired
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON CACHE BOOL "Enable LTO")

find_package(PkgConfig REQUIRED)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

find_package(pugixml)
if(NOT pugixml_FOUND)
    pkg_check_modules(pugixml REQUIRED IMPORTED_TARGET pugixml)
    add_library(pugixml::pugixml ALIAS PkgConfig::pugixml)
endif()

find_package(CLI11)
if(NOT CLI11_FOUND)
    pkg_check_modules(CLI11 REQUIRED IMPORTED_TARGET CLI11)
    add_library(CLI11::CLI11 ALIAS PkgConfig::CLI11)
endif()

find_package(spdlog)
if(NOT spdlog_FOUND)
    pkg_check_modules(spdlog REQUIRED IMPORTED_TARGET spdlog)
    add_library(spdlog::spdlog ALIAS PkgConfig::spdlog)
endif()

include_directories(${CMAKE_SOURCE_DIR})

add_library(m3u8dl_common STATIC
    src/common/logger.cpp
    src/common/http_client.cpp
    src/common/crypto.cpp
    src/common/util.cpp
    src/common/types.cpp
    src/common/stream_quality.cpp
    src/common/segment_disguise.cpp
)

target_include_directories(m3u8dl_common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(m3u8dl_common PUBLIC
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    ZLIB::ZLIB
)

add_library(m3u8dl_parser STATIC
    src/parser/stream_extractor.cpp
    src/parser/hls_extractor.cpp
    src/parser/dash_extractor.cpp
)

target_include_directories(m3u8dl_parser PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(m3u8dl_parser PUBLIC
    m3u8dl_common
    pugixml::pugixml
)

add_library(m3u8dl_downloader STATIC
    src/downloader/simple_downloader.cpp
    src/downloader/download_manager.cpp
)

target_include_directories(m3u8dl_downloader PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(m3u8dl_downloader PUBLIC
    m3u8dl_common
    m3u8dl_parser
)

add_executable(m3u8dl
    src/cli/main.cpp
    src/cli/command_line.cpp
)

target_link_libraries(m3u8dl PRIVATE
    m3u8dl_downloader
    CLI11::CLI11
)

install(TARGETS m3u8dl DESTINATION bin)
