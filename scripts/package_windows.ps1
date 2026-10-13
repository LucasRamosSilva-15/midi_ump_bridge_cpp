
$ErrorActionPreference = "Stop"

$RootDir = Resolve-Path "$PSScriptRoot\.."
Set-Location $RootDir

Write-Host "=== 1. Compilando alvos em modo Release (MSVC/CMake) ===" -ForegroundColor Cyan
if (-not (Test-Path "build")) { New-Item -ItemType Directory -Path "build" | Out-Null }
Set-Location "build"

if ($env:CMAKE_PREFIX_PATH) {
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$env:CMAKE_PREFIX_PATH" ..
} elseif (Test-Path "C:\\Qt\\6.7.2\\msvc2022_64") {
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\\Qt\\6.7.2\\msvc2022_64" ..
} else {
    cmake -DCMAKE_BUILD_TYPE=Release ..
}
cmake --build . --config Release --parallel

Set-Location $RootDir

Write-Host "=== 2. Criando pasta de distribuicao Standalone ===" -ForegroundColor Cyan
$DistDir = "dist\MIDI2Bridge-v5.0.0-Windows-x64"
if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
New-Item -ItemType Directory -Path $DistDir | Out-Null

$Binaries = @("build\Release\MIDI2Bridge.exe", "build\Release\ump_oscilloscope.exe", "build\Release\ump_audio_sink.exe")
if (-not (Test-Path $Binaries[0])) {
    $Binaries = @("build\MIDI2Bridge.exe", "build\ump_oscilloscope.exe", "build\ump_audio_sink.exe")
}

foreach ($bin in $Binaries) {
    if (Test-Path $bin) {
        Copy-Item $bin -Destination $DistDir
        Write-Host "Copiado: $bin" -ForegroundColor Green
    }
}

Write-Host "=== 3. Executando windeployqt para coletar todas as DLLs do Qt6 ===" -ForegroundColor Cyan
$windeployqt = (Get-Command windeployqt.exe -ErrorAction SilentlyContinue).Source
if (-not $windeployqt -and (Test-Path "C:\\Qt\\6.7.2\\msvc2022_64\\bin\\windeployqt.exe")) {
    $windeployqt = "C:\\Qt\\6.7.2\\msvc2022_64\\bin\\windeployqt.exe"
}
if (-not $windeployqt) {
    Write-Host "AVISO: windeployqt.exe nao encontrado no PATH." -ForegroundColor Yellow
    Write-Host "Certifique-se de executar este script dentro do terminal do Qt ou Developer Command Prompt do VS." -ForegroundColor Yellow
} else {
    & $windeployqt --release --no-translations --compiler-runtime "$DistDir\MIDI2Bridge.exe"
    & $windeployqt --release --no-translations "$DistDir\ump_oscilloscope.exe"
    Write-Host "DLLs do Qt6 e plugins embutidos com sucesso!" -ForegroundColor Green
}

$Readme = @"
===============================================================
  MIDI 2.0 UMP Bridge - Versao 5.0.0 (Pacote Portatil Windows)
===============================================================

Este pacote contem todos os executaveis, DLLs do Qt6 e plugins
necessarios para rodar em qualquer computador com Windows 10/11
sem precisar instalar o Qt ou ferramentas de compilacao.

COMO EXECUTAR:
--------------
1. Aplicacao Principal:
   Dois cliques em MIDI2Bridge.exe

2. Osciloscopio Digital Acustico (Dual-Trace):
   Dois cliques em ump_oscilloscope.exe

3. Receptor em Linha de Comando:
   Execute ump_audio_sink.exe via Prompt de Comando (cmd).
"@

Set-Content -Path "$DistDir\LEIA-ME.txt" -Value $Readme

Write-Host "=== 4. Compactando arquivo ZIP para o GitHub Releases ===" -ForegroundColor Cyan
$ZipOutput = "dist\MIDI2Bridge-v5.0.0-Windows-x64.zip"
if (Test-Path $ZipOutput) { Remove-Item $ZipOutput }
Compress-Archive -Path "$DistDir\*" -DestinationPath $ZipOutput -CompressionLevel Optimal

Write-Host "Pacote Windows gerado com sucesso em: $ZipOutput" -ForegroundColor Green
