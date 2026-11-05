@echo off
REM Local Windows build (Visual Studio 2022 + CMake required). The GitHub Actions workflow does the same automatically.
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build build --config Release --parallel || goto :error
echo.
echo Done. Plugin: build\MSCVoxity_artefacts\Release\VST3\MSC VOXITY 2.0.vst3
echo Copy that folder to C:\Program Files\Common Files\VST3\
exit /b 0
:error
echo Build failed.
exit /b 1
