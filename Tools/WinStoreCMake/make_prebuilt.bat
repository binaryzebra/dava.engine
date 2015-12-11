@echo off
set SCRIPT_DIR=%~dp0
set PREBUILT_DIR=%SCRIPT_DIR%cmake_prebuilt
set BUILD_DIR=%SCRIPT_DIR%cmake_build
set REPO_DIR=%SCRIPT_DIR%cmake_repo

::pre-build cleanup
rmdir /s /q %PREBUILT_DIR%
::rmdir /s /q %BUILD_DIR%
::rmdir /s /q %REPO_DIR%

::build cmake
::call %SCRIPT_DIR%rebuild_cmake.bat

::make prebuilt
mkdir %PREBUILT_DIR%

xcopy /Q /I /E %BUILD_DIR%\bin %PREBUILT_DIR%\bin
xcopy /Q /I /E %REPO_DIR%\Modules %PREBUILT_DIR%\bin\share\cmake-3.3\Modules
xcopy /Q /I /E %REPO_DIR%\Templates %PREBUILT_DIR%\bin\share\cmake-3.3\Templates

::post-build cleanup
::rmdir /s /q %BUILD_DIR%
::rmdir /s /q %REPO_DIR%
