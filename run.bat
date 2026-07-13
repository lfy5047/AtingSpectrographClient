@echo off

:: 包含配置文件
call config.bat %1



call make.bat %1
if %errorlevel% neq 0 (
    echo 编译失败，请检查错误信息
    exit /b %errorlevel%
)

:: 如果不存在install目录，则先安装
if not exist "%INSTALL_DIR%" (
    call install.bat %1
)

@REM :: 安装lib
@REM call install_lib.bat

:: 杀死所有进程
taskkill /IM %PROJECT_NAME%.exe /F

:: 运行可执行文件
echo Install directory: %INSTALL_DIR%
echo Running executable...
echo %INSTALL_DIR%\%PROJECT_NAME%.exe
%INSTALL_DIR%\%PROJECT_NAME%.exe
