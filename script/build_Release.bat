@echo off

setlocal EnableDelayedExpansion

set "SRC_DIR=%~dp0.."
for %%I in ("%SRC_DIR%") do set "SRC_DIR=%%~fI"

set "BUILD_DIR=%SRC_DIR%\build\Release"
set "BIN_DIR=%SRC_DIR%\bin\Release"

if not exist "%BUILD_DIR%" md "%BUILD_DIR%"
pushd "%BUILD_DIR%"

cmake "%SRC_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 pause & popd & exit /b 1

ninja
if errorlevel 1 pause & popd & exit /b 1

popd

if not exist "%BIN_DIR%" md "%BIN_DIR%"
copy /y "%BUILD_DIR%\NetherLink-Client.exe" "%BIN_DIR%\NetherLink-Client.exe"
windeployqt "%BIN_DIR%\NetherLink-Client.exe"

echo.
echo ===== Release build finished =====
echo.

timeout /t 10
