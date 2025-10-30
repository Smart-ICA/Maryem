cmake_minimum_required(VERSION 3.20)
project(overpower_email_plugin LANGUAGES CXX)

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# <<< ICI: on pointe vers le dossier src/ >>>
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

include(FetchContent)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(mads_plugin
  GIT_REPOSITORY https://github.com/pbosetti/mads_plugin.git
  GIT_TAG        HEAD
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(json mads_plugin)

include_directories(${json_SOURCE_DIR}/include)
include_directories(${mads_plugin_SOURCE_DIR}/src)

add_library(overpower_email SHARED ${SRC_DIR}/overpower_email.cpp)
target_link_libraries(overpower_email PRIVATE pugg)
set_target_properties(overpower_email PROPERTIES PREFIX "" SUFFIX ".plugin")

install(TARGETS overpower_email
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)
