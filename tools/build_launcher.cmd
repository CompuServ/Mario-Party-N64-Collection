@echo off
rem Builds the collection's launcher (src\launcher\collection_launcher.cpp) into build\launcher\MarioPartyCollection.exe,
rem which tools\package_build.ps1 copies into each bin\build-NNN as "Mario Party Collection.exe".
setlocal
set "ROOT=%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=C:\Program Files\LLVM\bin;%PATH%"
if not exist "%ROOT%\build\launcher" mkdir "%ROOT%\build\launcher"
rem The icon (src\launcher\collection_launcher.rc: Mario Party 1 Recompiled's app.ico).
llvm-rc /nologo /fo "%ROOT%\build\launcher\collection_launcher.res" "%ROOT%\src\launcher\collection_launcher.rc" || exit /b 1
clang-cl /nologo /O2 /EHsc /std:c++20 /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS "%ROOT%\src\launcher\collection_launcher.cpp" /Fo"%ROOT%\build\launcher\\" /Fe"%ROOT%\build\launcher\MarioPartyCollection.exe" /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib "%ROOT%\build\launcher\collection_launcher.res" || exit /b 1
echo === BUILD OK MarioPartyCollection.exe
