@echo off
:: Native Windows build script for SuperF4 (MinGW / LLVM-MinGW)
:: Usage:
::   build.bat           - debug build (SuperF4.exe in repo root)
::   build.bat release   - 32-bit + 64-bit binaries + NSIS installer
::   build.bat run       - debug build then launch
::
:: Optional Authenticode signing (STANDARD when cert env is set):
::   set SUPERF4_PFX=C:\path\to\cert.pfx
::   set SUPERF4_PFX_PASSWORD=secret
::   Optionally set SIGNTOOL to signtool.exe if it is not on PATH.

setlocal EnableExtensions EnableDelayedExpansion

set "PREFIX32=i686-w64-mingw32-"
set "PREFIX64=x86_64-w64-mingw32-"
set "APPVER=1.4.2"

where %PREFIX64%gcc >nul 2>&1
if errorlevel 1 (
  where gcc >nul 2>&1
  if errorlevel 1 (
    echo gcc not found on PATH.
    echo Install LLVM-MinGW or MSYS2 MinGW-w64 and ensure the mingw toolchains are available.
    exit /b 1
  )
  set "PREFIX32="
  set "PREFIX64="
)

if /I "%~1"=="release" goto :release

where %PREFIX64%windres >nul 2>&1
if errorlevel 1 (
  where windres >nul 2>&1
  if errorlevel 1 (
    echo windres not found on PATH.
    exit /b 1
  )
)

%PREFIX64%windres -o superf4.o include\app.rc
if errorlevel 1 exit /b 1
%PREFIX64%gcc -o SuperF4.exe superf4.c superf4.o -mwindows -lshlwapi -lpsapi -O2 -g -DDEBUG
if errorlevel 1 exit /b 1
del /Q superf4.o 2>nul
echo Built SuperF4.exe

if /I "%~1"=="run" (
  start "" SuperF4.exe
)
goto :eof

:release
where %PREFIX32%gcc >nul 2>&1
if errorlevel 1 (
  echo 32-bit toolchain not found: %PREFIX32%gcc
  exit /b 1
)
where %PREFIX64%gcc >nul 2>&1
if errorlevel 1 (
  echo 64-bit toolchain not found: %PREFIX64%gcc
  exit /b 1
)
where makensis >nul 2>&1
if errorlevel 1 (
  echo makensis not found on PATH. Install NSIS and retry.
  exit /b 1
)

mkdir bin\32 2>nul
mkdir bin\64 2>nul
copy /Y SuperF4.ini bin\32\ >nul
copy /Y SuperF4.ini bin\64\ >nul

echo Building 32-bit...
%PREFIX32%windres -o superf4-32.o include\app.rc
if errorlevel 1 exit /b 1
%PREFIX32%gcc -o bin\32\SuperF4.exe superf4.c superf4-32.o -mwindows -lshlwapi -lpsapi -O2 -s -fstack-protector-all -static -Wl,--dynamicbase,--nxcompat -Wp,-D_FORTIFY_SOURCE=2
if errorlevel 1 exit /b 1

echo Building 64-bit...
%PREFIX64%windres -o superf4-64.o include\app.rc
if errorlevel 1 exit /b 1
%PREFIX64%gcc -o bin\64\SuperF4.exe superf4.c superf4-64.o -mwindows -lshlwapi -lpsapi -O2 -s -fstack-protector-all -static -Wl,--dynamicbase,--nxcompat,--high-entropy-va -Wp,-D_FORTIFY_SOURCE=2
if errorlevel 1 exit /b 1

del /Q superf4-32.o superf4-64.o 2>nul

call :MaybeSign "bin\32\SuperF4.exe"
call :MaybeSign "bin\64\SuperF4.exe"

echo Building installer...
makensis /V2 /Dx64 installer.nsi
if errorlevel 1 exit /b 1

call :MaybeSign "bin\SuperF4-%APPVER%.exe"

echo.
echo Built:
echo   bin\32\SuperF4.exe
echo   bin\64\SuperF4.exe
echo   bin\SuperF4-%APPVER%.exe
if defined SUPERF4_PFX (
  echo Signed with %%SUPERF4_PFX%% when signtool succeeded.
) else (
  echo Tip: set SUPERF4_PFX and SUPERF4_PFX_PASSWORD to Authenticode-sign release binaries.
)
goto :eof

:MaybeSign
if not defined SUPERF4_PFX goto :eof
if "%~1"=="" goto :eof
set "ST=signtool"
if defined SIGNTOOL set "ST=%SIGNTOOL%"
where %ST% >nul 2>&1
if errorlevel 1 (
  echo signtool not found - skipping sign for %~1
  goto :eof
)
echo Signing %~1 ...
"%ST%" sign /fd SHA256 /f "%SUPERF4_PFX%" /p "%SUPERF4_PFX_PASSWORD%" /tr http://timestamp.digicert.com /td SHA256 "%~1"
if errorlevel 1 (
  echo WARNING: signing failed for %~1
)
goto :eof

endlocal
