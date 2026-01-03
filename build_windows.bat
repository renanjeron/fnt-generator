@echo off
setlocal

echo ==========================================
echo Fechando FontExporter se estiver aberto...
echo ==========================================
taskkill /F /IM FontExporter.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo FontExporter fechado.
) else (
    echo FontExporter nao estava em execucao.
)

echo.
echo ==========================================
echo Configurando o projeto com CMake...
echo ==========================================

if not exist build (
    mkdir build
)

cd build
cmake ..
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
echo Executando FontExporter...
echo ==========================================
cd Release
FontExporter.exe

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
