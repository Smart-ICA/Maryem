cmake_minimum_required(VERSION 3.20)
project(accel_fft_alarm_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# >>>>>> ICI: on pointe vers src/accel_fft_alarm_gui.cpp
add_library(accel_fft_alarm_gui SHARED ${CMAKE_CURRENT_LIST_DIR}/src/accel_fft_alarm_gui.cpp)
target_link_libraries(accel_fft_alarm_gui PRIVATE pugg)
set_target_properties(accel_fft_alarm_gui PROPERTIES PREFIX "")
set_target_properties(accel_fft_alarm_gui PROPERTIES SUFFIX ".plugin")

install(TARGETS accel_fft_alarm_gui
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)

