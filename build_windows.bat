@echo off
setlocal

echo ==========================================
echo Fechando FntGenerator se estiver aberto...
echo ==========================================
taskkill /F /IM FntGenerator.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo FntGenerator fechado.
) else (
    echo FntGenerator nao estava em execucao.
)

echo.
echo ==========================================
echo Configurando o projeto com CMake...
echo ==========================================

if not exist build (
    mkdir build
)

cd build

:: Detect Architecture
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    echo Detectado sistema x64. Configurando para x64...
    cmake .. -A x64
) else (
    echo Detectado sistema x86. Configurando para Win32...
    cmake .. -A Win32
)
if %errorlevel% neq 0 (
    echo Erro na configuracao do CMake.
    pause
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo Compilando (Release)...
echo ==========================================
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo Erro na compilacao.
    pause
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo Executando FntGenerator...
echo ==========================================
cd Release
FntGenerator.exe

if %errorlevel% neq 0 (
    echo O programa fechou com erro ou nao foi encontrado.
    pause
)

cd ..\..
echo.
echo ==========================================
echo Concluido.
echo ==========================================
pause
