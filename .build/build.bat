@echo off
g++ -fdiagnostics-color=always -g ..\*.cpp -o build.exe -lgdi32
if %errorlevel% equ 0 (
    echo Build successful! Running program...
    build.exe
) else (
    echo Build failed!
    pause
)