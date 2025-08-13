@echo off

setlocal EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"

set "BIN=%ROOT%\tool\Uncrustify\uncrustify.exe"
set "CFG=%ROOT%\tool\Uncrustify\uncrustify.cfg"

set "TOTAL=0"
set "FAIL=0"

for %%D in ("%ROOT%\src" "%ROOT%\include") do (
    if exist "%%~D" (
        echo [INFO] Scanning %%~D
        echo.
        echo [FORMAT]
        echo.
        for /f "usebackq delims=" %%F in (`
            dir "%%~D\*.cpp" "%%~D\*.h" "%%~D\*.hpp" "%%~D\*.c" "%%~D\*.cc" "%%~D\*.cxx" /b /s 2^>nul
        `) do (
            "%BIN%" -c "%CFG%" --no-backup "%%F"
            if errorlevel 1 (
                echo    [FAIL] %%F
                set /a FAIL+=1
            )
            set /a TOTAL+=1
        )
        echo.
    ) else (
        echo [WARN] Directory not found: %%~D
    )
)

echo ===== Summary =====
echo Total : %TOTAL%
echo Failed: %FAIL%
echo ===================

endlocal

timeout /t 10
