@echo off

:: 包含配置文件
call config.bat

@REM if exist "%INSTALL_DIR%" (
@REM     echo Removing existing installation...
@REM     rmdir /s /q "%INSTALL_DIR%"
@REM )
@REM mkdir "%INSTALL_DIR%"

:: 编译类型
set BUILD_TYPE=Debug
if "%1" == "r" (
    set BUILD_TYPE=Release
)

:: 安装
echo Installing binaries...
%CMAKE_PATH% --install "%BUILD_DIR%" --config %BUILD_TYPE% --prefix "%INSTALL_DIR%"