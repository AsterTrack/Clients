@echo off & SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

:: Make sure we have the environment variables necessary to build
if NOT "%VSCMD_ARG_TGT_ARCH%" == "x64" (
	echo Please use the x64 Native Tools Command Prompt for vcvars64 environment!
	exit /B
)

:: Make sure source directory is valid
set SRC_PATH=source
if not exist "%SRC_PATH%" (
	echo %SRC_PATH% does not exist!
	exit /B
)

:: Select release or debug mode
set MODE=%1
if "%MODE%"=="" (
	set MODE=release
)

if not exist win\build\%MODE% md win\build\%MODE%
if not exist win\install\%MODE% md win\install\%MODE%

echo -----------------------------------------
echo Building VRPN
echo -----------------------------------------

if "%MODE%"=="debug" (
	set CRT=MultiThreadedDebug
) else (
	set CRT=MultiThreaded
)
:: -DCMAKE_MSVC_RUNTIME_LIBRARY is completely ignored unless you set this stupid policy, thank you CMake for nothing

pushd win\build\%MODE%
cmake -DCMAKE_MSVC_RUNTIME_LIBRARY=%CRT% -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
	-DVRPN_USE_STD_CHRONO=ON -DVRPN_USE_GPM_MOUSE=OFF -DVRPN_BUILD_PYTHON_HANDCODED_3X=ON ^
	-DCMAKE_BUILD_TYPE=%MODE% -G "NMake Makefiles" ..\..\..\%SRC_PATH%
nmake quat vrpn vrpn-python
popd
pushd win\install\%MODE%
cmake -DCMAKE_INSTALL_COMPONENT=clientsdk -DCMAKE_INSTALL_PREFIX=./cpp -P ..\..\build\%MODE%\cmake_install.cmake
:: cmake -DCMAKE_INSTALL_COMPONENT=python -DCMAKE_INSTALL_PREFIX=./python -P ..\..\build\%MODE%\cmake_install.cmake
:: cmake_install for this component is broken (lines commented out), can just copy it directly
popd

echo -----------------------------------------
echo Build completed
echo -----------------------------------------

:: Try to verify output
if not exist win\install\%MODE%\cpp\lib (
	echo Build failed - win\install\%MODE%\cpp\lib does not exist!
	exit /B
)

:: Copy resulting files to project directory
if "%CD:~-29%" neq "\dependencies\buildfiles\vrpn" (
	echo Not in dependencies\buildfiles\vrpn subfolder, can't automatically install files into project folder!
	exit /B
)

echo Installing dependency files...
if not exist ..\..\..\cpp\include\vrpn md ..\..\..\cpp\include\vrpn
robocopy win\install\%MODE%\cpp\include ..\..\..\cpp\include\vrpn /E /NFL /NDL /NJH /NJS
if not exist ..\..\..\cpp\lib\win\%MODE%\vrpn md ..\..\..\cpp\lib\win\%MODE%\vrpn
robocopy win\install\%MODE%\cpp\lib ..\..\..\cpp\lib\win\%MODE%\vrpn /E /NFL /NDL /NJH /NJS

if not exist ..\..\..\python\lib md ..\..\..\python\lib
:: copy win\install\%MODE%\python\lib\pythondist-packages\vrpn.pyd ..\..\..\python\lib
copy win\build\%MODE%\python\vrpn.pyd ..\..\..\python\lib

echo Now you can call clean.bat if everything succeeded.