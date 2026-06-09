cmake_minimum_required(VERSION 3.10)
project(AtingSpectrographClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("/utf-8")
endif()

add_definitions(-DNEW_ALGORITHM)

include(libs/libsdefine.cmake)
include(cmake/GitVersion.cmake)

# Qt
find_package(Qt5 COMPONENTS Widgets PrintSupport Network Test REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
enable_testing()

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AppVersion.h.in"
    "${GENERATED_DIR}/AppVersion.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/setup.iss.in"
    "${CMAKE_CURRENT_BINARY_DIR}/setup.iss"
    @ONLY
)
if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AtingSpectrographClient.rc.in"
        "${GENERATED_DIR}/AtingSpectrographClient.rc"
        @ONLY
    )
    set(APP_WINDOWS_RESOURCES "${GENERATED_DIR}/AtingSpectrographClient.rc")
endif()

file(GLOB_RECURSE CLIENT_SRCS
    src/main.cpp
    src/Client/*.h   src/Client/*.cpp
    src/Ui/*.h       src/Ui/*.cpp
)

set(RESOURCES resources/resources.qrc)

add_executable(${PROJECT_NAME} ${CLIENT_SRCS} ${RESOURCES} ${APP_WINDOWS_RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GENERATED_DIR}
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

add_executable(ImageFrameUtilsTest
    tests/ImageFrameUtilsTest.cpp
    src/Ui/ImageFrameUtils.cpp
)
target_include_directories(ImageFrameUtilsTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client/rpc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Ui
)
target_link_libraries(ImageFrameUtilsTest PRIVATE
    Qt5::Test
    Qt5::Gui
)
add_test(NAME ImageFrameUtilsTest COMMAND ImageFrameUtilsTest)

add_executable(AppVersionTest
    tests/AppVersionTest.cpp
)
target_include_directories(AppVersionTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GENERATED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(AppVersionTest PRIVATE
    Qt5::Test
    Qt5::Core
)
add_test(NAME AppVersionTest COMMAND AppVersionTest)

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
