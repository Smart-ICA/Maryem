cmake_minimum_required(VERSION 3.20)
project(sound_fft_plugin LANGUAGES CXX)

# Build type par défaut
if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dépendances via FetchContent
include(FetchContent)

# nlohmann/json
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

# mads_plugin (contient filter.hpp + pugg)
FetchContent_Declare(mads_plugin
  GIT_REPOSITORY https://github.com/pbosetti/mads_plugin.git
  GIT_TAG        HEAD
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(json mads_plugin)

include_directories(${json_SOURCE_DIR}/include)
include_directories(${mads_plugin_SOURCE_DIR}/src)

# Le fichier source DOIT exister à ce chemin
add_library(sound_fft SHARED src/sound_fft.cpp)

target_link_libraries(sound_fft PRIVATE pugg)
set_target_properties(sound_fft PROPERTIES PREFIX "")
set_target_properties(sound_fft PROPERTIES SUFFIX ".plugin")

# Install
install(TARGETS sound_fft
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)












