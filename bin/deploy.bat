@echo off

for /d %%D in ("..\build\*Debug") do (
    copy /y "%%~fD\NetherLink-Client.exe" ".\Debug\NetherLink-Client.exe"
)

for /d %%R in ("..\build\*Release") do (
    copy /y "%%~fR\NetherLink-Client.exe" ".\Release\NetherLink-Client.exe"
)

windeployqt ./Debug/NetherLink-CLient.exe

windeployqt ./Release/NetherLink-Client.exe
