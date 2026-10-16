@echo off
cd /d "%~dp0"
if not exist dist mkdir dist
if not exist build mkdir build
windres -I native native\app.rc -O coff -o build\app_res.o || exit /b 1
g++ -std=c++17 -O2 -municode -mwindows -static -static-libgcc -static-libstdc++ -finput-charset=UTF-8 native\main.cpp native\core.cpp build\app_res.o -lcomctl32 -o dist\LobotomyCorpCal.exe || exit /b 1
g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ tests\core_test.cpp native\core.cpp -o build\core_test.exe || exit /b 1
build\core_test.exe || exit /b 1
g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -finput-charset=UTF-8 tests\ego_search_test.cpp -o build\ego_search_test.exe || exit /b 1
build\ego_search_test.exe || exit /b 1
echo Build and tests passed.
