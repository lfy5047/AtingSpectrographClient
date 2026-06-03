@echo off
setlocal enabledelayedexpansion

:: ============================================================
::  一键编译打包脚本
::  用法:
::    package.bat         仅打包 (使用已编译的 Release)
::    package.bat /b      编译 Release + 打包
::    package.bat /rebuild 清理重建 + 打包
:: ============================================================

call config.bat

set SCRIPT_DIR=%~dp0
set WINDEPLOYQT=%CMAKE_PREFIX_PATH:"=%\bin\windeployqt.exe
set ISS_EXE=C:\Program Files\Inno Setup 7\ISCC.exe
set ISS_SCRIPT=%PROJECT_ROOT%\setup.iss
set BUILD_TYPE=Release

echo.
echo ============================================
echo   AtingSpectrographClient 编译打包
echo ============================================
echo.

:: ---------- 清理重建 ----------
if /i "%1" == "/rebuild" (
    echo [1/3] 清理旧构建...
    if exist "%BUILD_DIR%" (
        rmdir /s /q "%BUILD_DIR%"
    )
    echo [2/3] 编译 Release...
    call "%PROJECT_ROOT%\make.bat" r
    if !errorlevel! neq 0 (
        echo 编译失败!
        exit /b !errorlevel!
    )
    echo [3/3] 打包...
    goto :deploy
)

:: ---------- 编译+打包 ----------
if /i "%1" == "/b" (
    echo [1/2] 编译 Release...
    call "%PROJECT_ROOT%\make.bat" r
    if !errorlevel! neq 0 (
        echo 编译失败!
        exit /b !errorlevel!
    )
    echo [2/2] 打包...
    goto :deploy
)

:: ---------- 仅打包 ----------
echo [1/1] 打包 (使用已有 Release 文件)...

:deploy
echo.
echo 运行 windeployqt...
if not exist "%WINDEPLOYQT%" (
    echo 警告: 找不到 windeployqt "%WINDEPLOYQT%", 跳过 Qt 部署
    goto :package
)

"%WINDEPLOYQT%" --compiler-runtime "%BUILD_DIR%\Release\%PROJECT_NAME%.exe"
if !errorlevel! neq 0 (
    echo windeployqt 失败!
    exit /b !errorlevel!
)

:package
if not exist "%ISS_EXE%" (
    echo 错误: 找不到 Inno Setup 编译器 "%ISS_EXE%"
    exit /b 1
)

if not exist "%ISS_SCRIPT%" (
    echo 错误: 找不到打包脚本 "%ISS_SCRIPT%"
    exit /b 1
)

if not exist "%BUILD_DIR%\Release\%PROJECT_NAME%.exe" (
    echo 错误: 找不到 Release 程序, 请先编译或使用 /b /rebuild 参数
    exit /b 1
)

"%ISS_EXE%" "%ISS_SCRIPT%"
if %errorlevel% neq 0 (
    echo 打包失败!
    exit /b %errorlevel%
)

echo.
echo ============================================
echo   打包完成!
echo   安装包: installer\AtingSpectrographClient_Setup_v*.exe
echo ============================================

endlocal
