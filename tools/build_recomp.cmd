@echo off
rem Builds one of the three recomps (recomp\mp1, recomp\mp2, recomp\mp3; CLAUDE.md "Builds").
rem   tools\build_recomp.cmd <1|2|3> [full|configure]
rem full = tools/recompile.sh (after a us.toml change) + CMake configure + ninja; configure = configure + ninja;
rem nothing = ninja only (patches/ or src/ changes, the collection's src\ included).
rem NINJA_JOBS limits parallel compiles (12 unless set): the recompiled game code's large files can exhaust memory.
setlocal
set "GAME=%~1"
set "MODE=%~2"
set "ROOT=%~dp0..\recomp"
if "%GAME%"=="1" (set "REPO=%ROOT%\mp1" & set "TARGET=MarioPartyRecompiled")
if "%GAME%"=="2" (set "REPO=%ROOT%\mp2" & set "TARGET=MarioParty2Recompiled")
if "%GAME%"=="3" (set "REPO=%ROOT%\mp3" & set "TARGET=MarioParty3Recompiled")
if not defined REPO (echo usage: build_recomp.cmd ^<1^|2^|3^> [full^|configure] & exit /b 2)
if not defined NINJA_JOBS set "NINJA_JOBS=12"

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=C:\Program Files\LLVM\bin;C:\Program Files\CMake\bin;C:\Program Files\Git\cmd;%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"
cd /d "%REPO%" || exit /b 1
if not "%MODE%"=="full" goto afterrecomp
echo === recompile.sh (game %GAME%)
"C:\Program Files\Git\bin\bash.exe" tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl || exit /b 1
:afterrecomp
if "%MODE%"=="" goto ninja
echo === configure
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DPATCHES_C_COMPILER=C:/msys64/clang64/bin/clang.exe "-DPATCHES_LD=C:/PROGRA~1/LLVM/bin/ld.lld.exe" || exit /b 1
:ninja
echo === ninja %TARGET%
set "PATH=%PATH%;C:\msys64\usr\bin"
ninja -C build -j %NINJA_JOBS% %TARGET% || exit /b 1
echo === BUILD OK %TARGET%
