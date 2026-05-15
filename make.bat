@echo off

:: 包含配置文件
call config.bat

:: 编译类型
set BUILD_TYPE=Release
if "%1" == "d" (
    set BUILD_TYPE=Debug
)

:: 创建构建目录
mkdir "%BUILD_DIR%"

:: 设置Visual Studio编译环境
call %VS_DEVCMD% -arch=x64

:: 运行CMake配置
echo Configuring project with CMake...
%CMAKE_PATH% -G "Ninja Multi-Config" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -S "%PROJECT_ROOT%" ^
    -B "%BUILD_DIR%" ^
    -DCMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH% ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=True

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b %errorlevel%
)

:: 执行编译
echo Building project...
%CMAKE_PATH% --build "%BUILD_DIR%" --config %BUILD_TYPE% --target all

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b %errorlevel%
)
