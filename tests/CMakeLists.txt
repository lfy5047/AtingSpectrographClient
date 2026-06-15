find_package(Qt5 COMPONENTS Core Gui Network Test Widgets REQUIRED)

set(TEST_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated")
set(TEST_RESOURCES "${PROJECT_SOURCE_DIR}/resources/resources.qrc")

add_executable(ImageFrameUtilsTest
    ImageFrameUtilsTest.cpp
    ${PROJECT_SOURCE_DIR}/src/Ui/ImageFrameUtils.cpp
)
target_include_directories(ImageFrameUtilsTest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/Client/rpc
    ${PROJECT_SOURCE_DIR}/src/Ui
)
target_link_libraries(ImageFrameUtilsTest PRIVATE
    Qt5::Test
    Qt5::Gui
)
add_test(NAME ImageFrameUtilsTest COMMAND ImageFrameUtilsTest)

add_executable(AppVersionTest
    AppVersionTest.cpp
)
target_include_directories(AppVersionTest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${TEST_GENERATED_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(AppVersionTest PRIVATE
    Qt5::Test
    Qt5::Core
)
add_test(NAME AppVersionTest COMMAND AppVersionTest)

add_executable(RecordServiceTest
    RecordServiceTest.cpp
    ${PROJECT_SOURCE_DIR}/src/Client/rpc/ControlClient.cpp
    ${PROJECT_SOURCE_DIR}/src/Client/services/RecordService.cpp
)
target_include_directories(RecordServiceTest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/Client
    ${PROJECT_SOURCE_DIR}/src/Client/core
    ${PROJECT_SOURCE_DIR}/src/Client/rpc
    ${PROJECT_SOURCE_DIR}/src/Client/services
    ${PROJECT_SOURCE_DIR}/Common/include
)
target_link_libraries(RecordServiceTest PRIVATE
    Qt5::Test
    Qt5::Network
)
add_test(NAME RecordServiceTest COMMAND RecordServiceTest)

add_executable(LocalRecordScannerTest
    LocalRecordScannerTest.cpp
    ${PROJECT_SOURCE_DIR}/src/Client/recording/LocalRecordScanner.cpp
)
target_include_directories(LocalRecordScannerTest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/Client
    ${PROJECT_SOURCE_DIR}/libs/public
)
target_link_libraries(LocalRecordScannerTest PRIVATE
    Qt5::Test
    Qt5::Core
)
add_test(NAME LocalRecordScannerTest COMMAND LocalRecordScannerTest)

add_executable(CollectServiceTest
    CollectServiceTest.cpp
    ${PROJECT_SOURCE_DIR}/src/Client/rpc/ControlClient.cpp
    ${PROJECT_SOURCE_DIR}/src/Client/services/CollectService.cpp
)
target_include_directories(CollectServiceTest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/Client
    ${PROJECT_SOURCE_DIR}/src/Client/core
    ${PROJECT_SOURCE_DIR}/src/Client/rpc
    ${PROJECT_SOURCE_DIR}/src/Client/services
    ${PROJECT_SOURCE_DIR}/Common/include
)
target_link_libraries(CollectServiceTest PRIVATE
    Qt5::Test
    Qt5::Network
)
add_test(NAME CollectServiceTest COMMAND CollectServiceTest)

add_executable(DeleteVerificationTest
    DeleteVerificationTest.cpp
    ${PROJECT_SOURCE_DIR}/src/Ui/DeleteVerification.cpp
)
target_include_directories(DeleteVerificationTest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/Ui
)
target_link_libraries(DeleteVerificationTest PRIVATE
    Qt5::Test
    Qt5::Core
)
add_test(NAME DeleteVerificationTest COMMAND DeleteVerificationTest)

add_executable(ThemeManagerTest
    ThemeManagerTest.cpp
    ${PROJECT_SOURCE_DIR}/src/Ui/ThemeManager.cpp
    ${TEST_RESOURCES}
)
target_include_directories(ThemeManagerTest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/Ui
)
target_link_libraries(ThemeManagerTest PRIVATE
    Qt5::Test
    Qt5::Widgets
)
add_test(NAME ThemeManagerTest COMMAND ThemeManagerTest)

add_executable(ImageViewThemeTest
    ImageViewThemeTest.cpp
    ${PROJECT_SOURCE_DIR}/src/Ui/ImageFrameUtils.cpp
    ${PROJECT_SOURCE_DIR}/src/Ui/widgets/TopBarWidget.cpp
    ${PROJECT_SOURCE_DIR}/src/Ui/widgets/ViewerAreaWidget.cpp
    ${PROJECT_SOURCE_DIR}/src/Ui/widgets/ImageView.cpp
    ${PROJECT_SOURCE_DIR}/src/Ui/ThemeManager.cpp
    ${TEST_RESOURCES}
)
target_include_directories(ImageViewThemeTest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/Client/rpc
    ${PROJECT_SOURCE_DIR}/src/Ui
)
target_link_libraries(ImageViewThemeTest PRIVATE
    Qt5::Test
    Qt5::Widgets
)
add_test(NAME ImageViewThemeTest COMMAND ImageViewThemeTest)
