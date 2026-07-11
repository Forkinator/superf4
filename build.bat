@echo off
:: Native Windows build script for SuperF4 (MinGW / LLVM-MinGW)
:: Usage:
::   build.bat          - debug build (SuperF4.exe in repo root)
::   build.bat release  - optimized 64-bit binary in bin\64\
::   build.bat run      - debug build then launch

setlocal EnableExtensions

where gcc >nul 2>&1
if errorlevel 1 (
  echo gcc not found on PATH.
  echo Install LLVM-MinGW or MSYS2 MinGW-w64 and ensure gcc/windres are available.
  exit /b 1
)

where windres >nul 2>&1
if errorlevel 1 (
  echo windres not found on PATH.
  exit /b 1
)

if /I "%~1"=="release" (
  mkdir bin\64 2>nul
  copy /Y SuperF4.ini bin\64\ >nul
  windres -o superf4.o include\app.rc
  if errorlevel 1 exit /b 1
  gcc -o bin\64\SuperF4.exe superf4.c superf4.o -mwindows -lshlwapi -lpsapi -O2 -s -fstack-protector-all -static -Wl,--dynamicbase,--nxcompat -Wp,-D_FORTIFY_SOURCE=2
  if errorlevel 1 exit /b 1
  del /Q superf4.o 2>nul
  echo Built bin\64\SuperF4.exe
  exit /b 0
)

windres -o superf4.o include\app.rc
if errorlevel 1 exit /b 1
gcc -o SuperF4.exe superf4.c superf4.o -mwindows -lshlwapi -lpsapi -O2 -g -DDEBUG
if errorlevel 1 exit /b 1
del /Q superf4.o 2>nul
echo Built SuperF4.exe

if /I "%~1"=="run" (
  start "" SuperF4.exe
)

endlocal
