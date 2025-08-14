@echo off

cd /d "%~dp0.."

doxygen Doxyfile

copy /y ".\doc\html\NetherLink-Client.chm" ".\doc\NetherLink-Client.chm"

cls
