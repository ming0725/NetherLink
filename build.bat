@echo off

setlocal EnableDelayedExpansion

set "logDate=%date:~0,4%-%date:~5,2%-%date:~8,2% %time:~0,2%:%time:~3,2%:%time:~6,2%"

call ".\script\format.bat"
if errorlevel 1 echo !logDate! [FAIL] format>>"log\BuildFail.log" & echo. >>"log\BuildFail.log"

call ".\script\build_Debug.bat"
if errorlevel 1 echo !logDate! [FAIL] debug>>"log\BuildFail.log" & echo. >>"log\BuildFail.log"

call ".\script\build_Release.bat"
if errorlevel 1 echo !logDate! [FAIL] release>>"log\BuildFail.log" & echo. >>"log\BuildFail.log"

call ".\script\doc.bat"
if errorlevel 1 echo !logDate! [FAIL] doc>>"log\BuildFail.log" & echo. >>"log\BuildFail.log"

cls
