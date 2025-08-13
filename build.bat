@echo off

setlocal EnableDelayedExpansion

set "TOTAL=0"
set "FAIL=0"

start "format" /wait cmd /c ".\script\format.bat"
set /a TOTAL+=1
if errorlevel 1 set /a FAIL+=1

start "build_Debug" /wait cmd /c ".\script\build_Debug.bat"
set /a TOTAL+=1
if errorlevel 1 set /a FAIL+=1

start "build_Release" /wait cmd /c ".\script\build_Release.bat"
set /a TOTAL+=1
if errorlevel 1 set /a FAIL+=1

start "doc" /wait cmd /c ".\script\doc.bat"
set /a TOTAL+=1
if errorlevel 1 set /a FAIL+=1

echo ===== Summary =====
echo Total : %TOTAL%
echo Failed: %FAIL%
echo ===================

pause

cls
