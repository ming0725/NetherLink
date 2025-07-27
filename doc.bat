@echo off

doxygen Doxyfile

copy /y ".\doc\html\NetherLink-Client.chm" ".\doc\NetherLink-Client.chm"
