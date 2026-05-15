cmake_minimum_required(VERSION 3.16)

add_library(Common
    src/SerialPort.cpp
    src/TimerManager.cpp
    src/Utils.cpp
)

target_include_directories(Common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/libs/public/nlohmann
)

target_link_libraries(Common
    PUBLIC
        ${LIB_LIST}
)
