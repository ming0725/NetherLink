@echo off
pushd "%~dp0"

set "BIN=tool\uncrustify\uncrustify.exe"
set "CFG=tool\uncrustify\uncrustify.cfg"

if not exist "%BIN%" (echo Missing %BIN% & pause & exit /b 1)
if not exist "%CFG%" (echo Missing %CFG% & pause & exit /b 1)

for /r %%f in (*.cpp *.h) do (
    echo "%%f" | find /I "\build\" >nul || (
        "%BIN%" -c "%CFG%" --no-backup "%%f"
    )
)

echo Done
pause
popd