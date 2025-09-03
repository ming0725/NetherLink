@echo off

setlocal EnableDelayedExpansion

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\build\Debug"
if not exist "%BUILD_DIR%" md "%BUILD_DIR%"

pushd "%BUILD_DIR%"

set "LOG=%ROOT%\log\DebugFail.log"
if not exist "%ROOT%\log"  md "%ROOT%\log"

cmake %ROOT% -G Ninja -DCMAKE_BUILD_TYPE=Debug>cmake.out 2>&1
set CMAKE_ERR=%errorlevel%
type cmake.out
type cmake.out | findstr /i "warning error" >>"%LOG%" & echo. >>"%LOG%"

ninja >ninja.out 2>&1
set NINJA_ERR=%errorlevel%
type ninja.out
type ninja.out | findstr /i "warning error" >>"%LOG%" & echo. >>"%LOG%"

popd

if %CMAKE_ERR% neq 0 (
    type "%LOG%" | findstr /i "warning error"
    exit /b %CMAKE_ERR%
)
if %NINJA_ERR% neq 0 (
    type "%LOG%" | findstr /i "warning error"
    exit /b %NINJA_ERR%
)

set "BIN_DIR=%ROOT%\bin\Debug"
if not exist "%BIN_DIR%"   md "%BIN_DIR%"
copy /y "%BUILD_DIR%\NetherLink-Client.exe" "%BIN_DIR%\NetherLink-Client.exe" >nul
windeployqt "%BIN_DIR%\NetherLink-Client.exe" >nul

cls
