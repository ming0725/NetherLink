@echo off

setlocal EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"

set "BIN=%ROOT%\tool\Uncrustify\uncrustify.exe"
set "CFG=%ROOT%\tool\Uncrustify\uncrustify.cfg"

set "FAIL=0"

for %%D in ("%ROOT%\include" "%ROOT%\src") do (
    if exist "%%~D" (
        for /f "usebackq delims=" %%F in (`
            dir "%%~D\*.h" "%%~D\*.hpp" "%%~D\*.c" "%%~D\*.cc" "%%~D\*.cpp" "%%~D\*.cxx" /b /s 2^>nul
        `) do (
            "%BIN%" -c "%CFG%" --no-backup "%%F"
            if errorlevel 1 (
                set /a FAIL+=1
            )
        )
    )
)

if !FAIL! neq 0 (
    if not exist "%ROOT%\log" mkdir "%ROOT%\log"
    set "logDate=%date:~0,4%-%date:~5,2%-%date:~8,2% %time:~0,2%:%time:~3,2%:%time:~6,2%"
    echo !logDate! [FAIL] !FAIL!>>"%ROOT%\log\FormatFail.log"
    echo. >>"%ROOT%\log\FormatFail.log"
)

endlocal

cls
