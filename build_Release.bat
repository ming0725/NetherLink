@echo off
if not exist "build/Release" (
    md "build/Release"
)
cd "build/Release"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ../..
ninja
pause