@echo off

:: 包含配置文件
call config.bat


if exist "%BUILD_DIR%" (
    echo Cleaning build directory...
    rmdir /s /q "%BUILD_DIR%"
)