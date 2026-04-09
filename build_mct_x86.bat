@echo off
setlocal EnableExtensions EnableDelayedExpansion

echo.
echo  ================================================
echo    Makeshift Capture Tool  --  x86 (32-bit) Build
echo    (with gamepad_serial DLL integration)
echo  ================================================
echo.
echo  Run from: "x86 Native Tools Command Prompt for VS 2022"
echo.

rem ---------------------------------------------------------------
rem  Paths
rem ---------------------------------------------------------------
set "VCPKG_ROOT=C:\mct\vcpkg"
set "TRIPLET=x86-windows-static"
set "BOOST_INC=%VCPKG_ROOT%\installed\%TRIPLET%\include"
set "BOOST_LIB=%VCPKG_ROOT%\installed\%TRIPLET%\lib"

rem ---------------------------------------------------------------
rem  Pre-flight checks
rem ---------------------------------------------------------------
if not exist "%BOOST_INC%\boost\filesystem.hpp" (
    echo [ERROR] Boost headers not found at:
    echo   %BOOST_INC%
    pause & exit /b 1
)

if not exist "nuklear.h" (
    echo [ERROR] nuklear.h not found in current folder.
    pause & exit /b 1
)

if not exist "makeshift_capture_tool.cpp" (
    echo [ERROR] makeshift_capture_tool.cpp not found.
    pause & exit /b 1
)

if not exist "gamepad_serial.h" (
    echo [ERROR] gamepad_serial.h not found.
    pause & exit /b 1
)

rem ---------------------------------------------------------------
rem  If gamepad_serial_x86.lib is missing, build the DLL now
rem  (requires gamepad_serial.cpp to be present)
rem ---------------------------------------------------------------
if not exist "gamepad_serial_x86.lib" (
    echo [INFO] gamepad_serial_x86.lib not found -- building gamepad DLL first...
    echo.

    if not exist "gamepad_serial.cpp" (
        echo [ERROR] gamepad_serial.cpp not found either.
        echo         Cannot auto-build the DLL. Put gamepad_serial.cpp here.
        pause & exit /b 1
    )

    cl.exe /nologo /std:c++17 /EHsc /O2 /W3 /MT /DNDEBUG /D_WIN32_WINNT=0x0601 ^
        /D GAMEPAD_SERIAL_EXPORTS /LD gamepad_serial.cpp /Fe:gamepad_serial_x86.dll ^
        /link /DLL /SUBSYSTEM:WINDOWS,6.1 /MACHINE:X86 /OPT:REF /OPT:ICF

    if !ERRORLEVEL! neq 0 (
        echo [FAILED] gamepad DLL build failed.
        pause & exit /b 1
    )
    echo [OK] gamepad_serial_x86.dll + .lib built successfully.
    echo.
)

echo [OK] Boost   : %BOOST_INC%
echo [OK] Gamepad : gamepad_serial_x86.lib found
echo [OK] Target  : x86
echo.

rem ---------------------------------------------------------------
rem  Locate Boost libraries (auto-detect names)
rem ---------------------------------------------------------------
set "BOOST_FS_LIB="
for /f "delims=" %%F in ('dir /b "%BOOST_LIB%\boost_filesystem*.lib" 2^>nul') do set "BOOST_FS_LIB=%%F"

set "BOOST_CHRONO_LIB="
for /f "delims=" %%F in ('dir /b "%BOOST_LIB%\boost_chrono*.lib" 2^>nul') do set "BOOST_CHRONO_LIB=%%F"

set "BOOST_ATOMIC_LIB="
for /f "delims=" %%F in ('dir /b "%BOOST_LIB%\boost_atomic*.lib" 2^>nul') do set "BOOST_ATOMIC_LIB=%%F"

set "BOOST_DT_LIB="
for /f "delims=" %%F in ('dir /b "%BOOST_LIB%\boost_date_time*.lib" 2^>nul') do set "BOOST_DT_LIB=%%F"

if not defined BOOST_FS_LIB (
    echo [ERROR] Cannot find boost_filesystem*.lib in %BOOST_LIB%
    pause & exit /b 1
)
if not defined BOOST_CHRONO_LIB (
    echo [ERROR] Cannot find boost_chrono*.lib in %BOOST_LIB%
    pause & exit /b 1
)
if not defined BOOST_ATOMIC_LIB (
    echo [ERROR] Cannot find boost_atomic*.lib in %BOOST_LIB%
    pause & exit /b 1
)
if not defined BOOST_DT_LIB (
    echo [ERROR] Cannot find boost_date_time*.lib in %BOOST_LIB%
    pause & exit /b 1
)

echo [OK] filesystem : !BOOST_FS_LIB!
echo [OK] chrono     : !BOOST_CHRONO_LIB!
echo [OK] atomic     : !BOOST_ATOMIC_LIB!
echo [OK] date_time  : !BOOST_DT_LIB!
echo.

rem ---------------------------------------------------------------
rem  Optional resource file (logo.png + resource.rc)
rem ---------------------------------------------------------------
set "RESFILE="
if exist "logo.png" if exist "resource.rc" (
    rc.exe /nologo resource.rc
    if !ERRORLEVEL! equ 0 (
        set "RESFILE=resource.res"
        echo [OK] Resource  : resource.res compiled
    )
)

echo.
echo [..] Compiling MakeshiftCaptureTool_x86.exe ...
echo.

rem ---------------------------------------------------------------
rem  Compile main app
rem ---------------------------------------------------------------
cl.exe /nologo /std:c++17 /EHsc /O2 /W1 /MT /GL /DNDEBUG ^
    /D_WIN32_WINNT=0x0601 ^
    /DBOOST_ALL_NO_LIB ^
    /I"%BOOST_INC%" /I. ^
    makeshift_capture_tool.cpp ^
    /Fe:MakeshiftCaptureTool_x86.exe ^
    /link /NOLOGO /LTCG /OPT:REF /OPT:ICF ^
    /SUBSYSTEM:WINDOWS,6.1 ^
    /MACHINE:X86 ^
    /LIBPATH:"%BOOST_LIB%" ^
    /LIBPATH:. ^
    "!BOOST_FS_LIB!" ^
    "!BOOST_CHRONO_LIB!" ^
    "!BOOST_ATOMIC_LIB!" ^
    "!BOOST_DT_LIB!" ^
    gamepad_serial_x86.lib ^
    kernel32.lib user32.lib gdi32.lib shell32.lib ^
    comctl32.lib ole32.lib advapi32.lib gdiplus.lib ^
    !RESFILE!

if errorlevel 1 (
    echo.
    echo [FAILED]
    pause & exit /b 1
)

echo.
echo ================================================================
echo  BUILD SUCCESSFUL
echo.
echo    MakeshiftCaptureTool_x86.exe
echo.
echo  Keep these two files together to run:
echo    MakeshiftCaptureTool_x86.exe
echo    gamepad_serial_x86.dll
echo ================================================================
echo.
pause
