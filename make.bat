@echo off
setlocal enableextensions

set "CONFIG=Release"
if not "%1"=="" set "CONFIG=%1"

if exist build rmdir /s /q build

cmake -S . -B build
cmake --build build --config %CONFIG%
ctest --test-dir build -C %CONFIG%
