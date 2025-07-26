@echo off
if not exist "build/Debug" (
    md "build/Debug"
)
cd "build/Debug"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../..
ninja
pause