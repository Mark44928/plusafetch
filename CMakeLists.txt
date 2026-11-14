cmake_minimum_required(VERSION 3.16)
project(plusafetch VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-O3 -Wall -Wextra -Wpedantic -pthread)
elseif(MSVC)
  add_compile_options(/O2 /W4)
endif()

find_package(Threads REQUIRED)

# nlohmann/json single-include is vendored at third_party/nlohmann/json.hpp
set(JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_executable(plusafetch
  src/main.cpp
  src/registry.cpp
  src/detection/platform.cpp
  src/detection/platform_android.cpp
  src/detection/platform_linux.cpp
  src/detection/common_detect.cpp
  src/modules/os.cpp
  src/modules/cpu.cpp
  src/modules/memory.cpp
  src/modules/battery.cpp
  src/modules/sensors.cpp
  src/modules/telephony.cpp
  src/modules/wifi.cpp
  src/modules/core_modules.cpp
)

target_include_directories(plusafetch PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${JSON_INCLUDE_DIR}
)

target_link_libraries(plusafetch PRIVATE Threads::Threads)

install(TARGETS plusafetch RUNTIME DESTINATION bin)
