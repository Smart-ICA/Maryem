cmake_minimum_required(VERSION 3.20)
project(web_dashboard VERSION 1.0.0 LANGUAGES CXX)

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "/usr/local")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

include(FetchContent)

# pugg (plugins MADS)
FetchContent_Declare(pugg 
  GIT_REPOSITORY https://github.com/pbosetti/pugg.git
  GIT_TAG        1.0.2
  GIT_SHALLOW    TRUE
)

# nlohmann/json
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

# cpp-httplib (header-only)
FetchContent_Declare(httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.15.3
  GIT_SHALLOW    TRUE
)

FetchContent_Populate(plugin 
  GIT_REPOSITORY https://github.com/pbosetti/mads_plugin.git
  GIT_TAG        HEAD
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(pugg json httplib)
include_directories(${plugin_SOURCE_DIR}/src)
include_directories(${json_SOURCE_DIR}/include)
include_directories(${httplib_SOURCE_DIR})

add_library(web_dashboard SHARED ${SRC_DIR}/web_dashboard.cpp)
target_link_libraries(web_dashboard PRIVATE pugg)
target_compile_definitions(web_dashboard PRIVATE PLUGIN_NAME="web_dashboard")
set_target_properties(web_dashboard PROPERTIES PREFIX "" SUFFIX ".plugin")

if (APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
else()
  set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib;/usr/local/lib")
endif()

install(TARGETS web_dashboard
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)

