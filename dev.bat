@echo off
setlocal

set "PROJECT_ROOT=%~dp0"
set "BUILD_DIRECTORY=%PROJECT_ROOT%build"
set "SDK_DIRECTORY=%PROJECT_ROOT%dist\rohr"
set "OPERATION=%~1"
if "%OPERATION%"=="" set "OPERATION=build"

if "%OPERATION%"=="clean" goto clean
if not "%OPERATION%"=="build" if not "%OPERATION%"=="test" if not "%OPERATION%"=="sdk" goto usage

cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIRECTORY%"
if errorlevel 1 exit /b %errorlevel%
cmake --build "%BUILD_DIRECTORY%"
if errorlevel 1 exit /b %errorlevel%

if "%OPERATION%"=="test" (
    ctest --test-dir "%BUILD_DIRECTORY%" --output-on-failure
    exit /b %errorlevel%
)
if "%OPERATION%"=="sdk" (
    cmake --install "%BUILD_DIRECTORY%" --prefix "%SDK_DIRECTORY%"
    exit /b %errorlevel%
)
exit /b 0

:clean
cmake -E remove_directory "%BUILD_DIRECTORY%"
if errorlevel 1 exit /b %errorlevel%
cmake -E remove_directory "%PROJECT_ROOT%dist"
exit /b %errorlevel%

:usage
echo usage: dev.bat [build^|test^|sdk^|clean] 1>&2
exit /b 1
