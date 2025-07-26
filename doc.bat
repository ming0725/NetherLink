@echo off

doxygen Doxyfile

move /y ".\doc\html\NetherLink-Client.chm" ".\doc\NetherLink-Client.chm"
