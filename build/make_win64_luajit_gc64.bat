@echo off

::call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
set LUAJIT_PATH=luajit-2.1.0b3
md %LUAJIT_PATH%\src\3rd
xcopy 3rd %LUAJIT_PATH%\src\3rd /s /h /d /y


echo Swtich to x64 build env
cd %~dp0\luajit-2.1.0b3\src
call msvcbuild_mt.bat gc64 static
cd ..\..

mkdir build_lj64 & pushd build_lj64
cmake -DUSING_LUAJIT=ON -DGC64=ON -G "Visual Studio 16 2019" -DARCHITECTURE=x64 ..
IF %ERRORLEVEL% NEQ 0 cmake -DUSING_LUAJIT=ON -DGC64=ON -G "Visual Studio 16 2019" -DARCHITECTURE=x64 ..
popd
cmake --build build_lj64 --config Release
md plugin_luajit\Plugins\x86_64
copy /Y build_lj64\Release\xlua.dll plugin_luajit\Plugins\x86_64\xlua.dll
pause