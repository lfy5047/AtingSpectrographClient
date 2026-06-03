cmake_minimum_required(VERSION 3.10)
project(AtingSpectrographClient VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("/utf-8")
endif()

add_definitions(-DNEW_ALGORITHM)

include(libs/libsdefine.cmake)

# Qt
find_package(Qt5 COMPONENTS Widgets PrintSupport Network REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

file(GLOB_RECURSE CLIENT_SRCS
    src/main.cpp
    src/Client/*.h   src/Client/*.cpp
    src/Ui/*.h       src/Ui/*.cpp
)

set(RESOURCES resources/resources.qrc)

add_executable(${PROJECT_NAME} ${CLIENT_SRCS} ${RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client/rpc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client/stream
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Common/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Widgets
    Qt5::PrintSupport
    Qt5::Network
    ${LIB_LIST}
)

if(WIN32 AND EXISTS "${LIBTIFF_DLL}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIBTIFF_DLL}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            WIN32_EXECUTABLE false   # Debug 显示控制台
        )
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES
            WIN32_EXECUTABLE true    # Release 隐藏控制台
        )
    endif()
endif()
