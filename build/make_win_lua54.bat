rm -rf build64_54
mkdir build64_54 & pushd build64_54
cmake -DLUA_VERSION=5.4.6 -G "Visual Studio 17 2022" -A x64  ..
popd
cmake --build build64_54 --config Release
md plugin_lua54\Plugins\x86_64
copy /Y build64_54\Release\xlua.dll plugin_lua54\Plugins\x86_64\xlua.dll

pause