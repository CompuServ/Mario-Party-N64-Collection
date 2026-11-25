@echo off
rem Builds the collection's launcher (src\launcher\collection_launcher.cpp) into build\launcher\MarioPartyCollection.exe,
rem which tools\package_build.ps1 copies into each bin\build-NNN beside Mario Party 1's exe.
setlocal
set "ROOT=%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=C:\Program Files\LLVM\bin;%PATH%"
if not exist "%ROOT%\build\launcher" mkdir "%ROOT%\build\launcher"
clang-cl /nologo /O2 /EHsc /std:c++20 /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS "%ROOT%\src\launcher\collection_launcher.cpp" /Fo"%ROOT%\build\launcher\\" /Fe"%ROOT%\build\launcher\MarioPartyCollection.exe" /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib || exit /b 1
echo === BUILD OK MarioPartyCollection.exe
