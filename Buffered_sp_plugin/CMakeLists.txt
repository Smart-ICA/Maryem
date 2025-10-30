#   ____  _             _       
#  |  _ \| |_   _  __ _(_)_ __  
#  | |_) | | | | |/ _` | | '_ \ 
#  |  __/| | |_| | (_| | | | | |
#  |_|   |_|\__,_|\__, |_|_| |_|
#                 |___/         
# A Template for BufferedPlugin, a Source Plugin
# NOTICE: MADS Version 1.4.0

cmake_minimum_required(VERSION 3.20)
project(buffered VERSION 1.4.0 LANGUAGES CXX)

# -------- Build type / install prefix (classique) --------
if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path prefix" FORCE)
endif()

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

# -------- Options --------
option(BUILD_STANDALONE "Build standalone test executables with main()" OFF)
set(PLUGIN_SUFFIX "" CACHE STRING "Suffix for the plugin file (arm64, x86_64, etc.)")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

# -------- Dépendances --------
include(FetchContent)

FetchContent_Declare(pugg 
  GIT_REPOSITORY https://github.com/pbosetti/pugg.git
  GIT_TAG        1.0.2
  GIT_SHALLOW    TRUE
)

set(BUILD_TESTING OFF CACHE INTERNAL "")
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(serialport
  GIT_REPOSITORY https://github.com/pbosetti/serial.git
  GIT_TAG        HEAD
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(pugg json serialport)

FetchContent_Populate(plugin 
  GIT_REPOSITORY https://github.com/pbosetti/mads_plugin.git
  GIT_TAG        v1.3.5
  GIT_SHALLOW    TRUE
  SUBBUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/plugin-subbuild
  SOURCE_DIR   ${CMAKE_CURRENT_BINARY_DIR}/_deps/plugin-src
  BINARY_DIR   ${CMAKE_CURRENT_BINARY_DIR}/_deps/plugin-build
)

include_directories(
  ${plugin_SOURCE_DIR}/src
  ${json_SOURCE_DIR}/include
  ${serial_SOURCE_DIR}/include
)

# -------- Macro add_plugin corrigée --------
# Appel: add_plugin(name [SRCS ...] [LIBS ...])
macro(add_plugin name)
  set(multiValueArgs LIBS SRCS)
  cmake_parse_arguments(plugin "" "" "${multiValueArgs}" ${ARGN})

  # La librairie plugin (*.plugin)
  add_library(${name} SHARED ${SRC_DIR}/${name}.cpp ${plugin_SRCS})
  target_link_libraries(${name} PRIVATE pugg ${plugin_LIBS})
  set_target_properties(${name} PROPERTIES PREFIX "")
  if (PLUGIN_SUFFIX)
    set_target_properties(${name} PROPERTIES SUFFIX "_${PLUGIN_SUFFIX}.plugin")
    target_compile_definitions(${name} PRIVATE PLUGIN_NAME="${name}_${PLUGIN_SUFFIX}")
  else()
    set_target_properties(${name} PROPERTIES SUFFIX ".plugin")
    target_compile_definitions(${name} PRIVATE PLUGIN_NAME="${name}")
  endif()

  # L’exécutable de test (facultatif)
  if (BUILD_STANDALONE)
    add_executable(${name}_main ${SRC_DIR}/${name}.cpp ${plugin_SRCS})
    target_link_libraries(${name}_main PRIVATE pugg ${plugin_LIBS})
    set_target_properties(${name}_main PROPERTIES OUTPUT_NAME ${name})
    # >>>>> TRÈS IMPORTANT : définir ce symbole pour inclure main() <<<<<
    target_compile_definitions(${name}_main PRIVATE BUILD_STANDALONE)
    list(APPEND TARGET_LIST ${name}_main)
  endif()

  list(APPEND TARGET_LIST ${name})
endmacro()

# -------- RPATH / install --------
if (APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
  include_directories(/opt/homebrew/include)
  link_directories(/opt/homebrew/lib)
else()
  set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib;/usr/local/lib")
endif()

# -------- Cibles --------
add_plugin(buffered)
add_plugin(buffered_sp LIBS serial)

# Utilitaire de test séparé (contient son propre main, OK)
add_executable(acq_test ${SRC_DIR}/acquisitor.cpp)
target_link_libraries(acq_test PRIVATE pugg serial)

# -------- Install --------
if(APPLE)
  install(TARGETS ${TARGET_LIST}
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib
    ARCHIVE DESTINATION lib
  )
else()
  install(TARGETS ${TARGET_LIST}
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
  )
endif()

