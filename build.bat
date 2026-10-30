@echo off
rem Compile SeoChecker.dll (64 bits) avec MinGW-w64 (g++).
setlocal
cd /d "%~dp0"
where g++ >nul 2>nul || (echo g++ introuvable : installez MinGW-w64 ^(winget install BrechtSanders.WinLibs.POSIX.UCRT^) & exit /b 1)
if not exist build mkdir build

set FLAGS=-O2 -std=c++17 -Wall -DUNICODE -D_UNICODE -static -s

g++ %FLAGS% -shared -o build\SeoChecker.dll src\Plugin.cpp src\Seo.cpp src\Rapport.cpp src\Fetch.cpp -lcomctl32 -lshell32 -lwinhttp -luser32 -lgdi32 || exit /b 1
g++ %FLAGS% -o build\test_seo.exe tests\test_seo.cpp src\Seo.cpp src\Rapport.cpp || exit /b 1

build\test_seo.exe exemple.html build\rapport_exemple.html || exit /b 1
echo.
echo OK : build\SeoChecker.dll
