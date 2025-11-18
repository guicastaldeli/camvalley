@echo off

echo Building with Visual Studio 2022
echo ================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

cl /EHsc /std:c++17 /DUNICODE /D_UNICODE /I".." /I"..\controller" /I"..\device" ..\*.cpp ..\controller\*.cpp ..\device\*.cpp ^
   /link mf.lib mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib shlwapi.lib user32.lib gdi32.lib

if %errorlevel% equ 0 (
    echo Build successful! Running program...
    main.exe
) else (
    echo Build failed!
    pause
)