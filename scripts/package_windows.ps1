# Script de Empacotamento Portatil Standalone para Windows
# TCC - MIDI 2.0 UMP Bridge v5.1.0

$ErrorActionPreference = "Stop"

function Log-Step($msg) {
    Write-Host "`n>>> [ETAPA] $msg" -ForegroundColor Cyan
}

function Log-Success($msg) {
    Write-Host " [OK] $msg" -ForegroundColor Green
}

function Log-Warn($msg) {
    Write-Host " [AVISO] $msg" -ForegroundColor Yellow
}

function Log-Error($msg) {
    Write-Host " [ERRO] $msg" -ForegroundColor Red
}

try {
    $RootDir = Resolve-Path "$PSScriptRoot\.."
    Set-Location $RootDir
    Write-Host "Diretorio raiz: $RootDir"

    Log-Step "1. Configurando e Compilando em modo Release (MSVC/CMake)"
    if (-not (Test-Path "build")) { 
        New-Item -ItemType Directory -Path "build" | Out-Null 
    }
    Set-Location "build"

    $cmakeArgs = @("-DCMAKE_BUILD_TYPE=Release")
    $qtFound = $null
    if ($env:CMAKE_PREFIX_PATH -and (Test-Path $env:CMAKE_PREFIX_PATH)) {
        $qtFound = $env:CMAKE_PREFIX_PATH
    } else {
        $candidates = Get-ChildItem -Path "C:\Qt\*\msvc*" -Directory -ErrorAction SilentlyContinue
        if ($candidates) {
            $qtFound = $candidates[0].FullName
        }
    }
    if ($qtFound) {
        $cmakeArgs += "-DCMAKE_PREFIX_PATH=$qtFound"
        Write-Host "Usando CMAKE_PREFIX_PATH: $qtFound"
    }

    Write-Host "Executando: cmake $cmakeArgs .."
    & cmake @cmakeArgs ..
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure falhou com codigo $LASTEXITCODE"
    }

    Write-Host "Executando: cmake --build . --config Release --parallel"
    & cmake --build . --config Release --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build falhou com codigo $LASTEXITCODE"
    }
    Log-Success "Compilacao concluida com sucesso!"

    Set-Location $RootDir

    Log-Step "2. Criando pasta de distribuicao Standalone"
    $DistDir = Join-Path $RootDir "dist\MIDI2Bridge-v5.1.0-Windows-x64"
    if (Test-Path $DistDir) { 
        Remove-Item -Recurse -Force $DistDir 
    }
    New-Item -ItemType Directory -Path $DistDir | Out-Null

    $Targets = @("MIDI2Bridge.exe", "ump_oscilloscope.exe", "ump_audio_sink.exe")
    foreach ($target in $Targets) {
        $found = $false
        $pathsToCheck = @("build\Release\$target", "build\$target")
        foreach ($p in $pathsToCheck) {
            if (Test-Path $p) {
                Copy-Item $p -Destination $DistDir
                Log-Success "Copiado $target de $p para o pacote"
                $found = $true
                break
            }
        }
        if (-not $found) {
            throw "Executavel obrigatorio $target nao encontrado apos build!"
        }
    }

    Log-Step "3. Executando windeployqt para embutir DLLs do Qt6"
    $windeployqt = (Get-Command windeployqt.exe -ErrorAction SilentlyContinue).Source
    if (-not $windeployqt) {
        $candidates = Get-ChildItem -Path "C:\Qt\*\msvc*\bin\windeployqt.exe" -File -ErrorAction SilentlyContinue
        if ($candidates) {
            $windeployqt = $candidates[0].FullName
        }
    }

    if (-not $windeployqt) {
        Log-Warn "windeployqt.exe nao encontrado no sistema."
    } else {
        Write-Host "Executando windeployqt para MIDI2Bridge.exe..."
        & $windeployqt --release --no-translations --compiler-runtime "$DistDir\MIDI2Bridge.exe"
        if ($LASTEXITCODE -ne 0) {
            Log-Warn "windeployqt finalizou com codigo $LASTEXITCODE em MIDI2Bridge.exe"
        }

        Write-Host "Executando windeployqt para ump_oscilloscope.exe..."
        & $windeployqt --release --no-translations "$DistDir\ump_oscilloscope.exe"
        Log-Success "DLLs e plugins do Qt6 embutidos com sucesso!"
    }

    $Readme = @"
===============================================================
  MIDI 2.0 UMP Bridge - Versao 5.1.0 (Pacote Portatil Windows)
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
    Set-Content -Path "$DistDir\LEIA-ME.txt" -Value $Readme -Encoding utf8

    Log-Step "4. Compactando arquivo ZIP para GitHub Releases"
    $distParent = Join-Path $RootDir "dist"
    if (-not (Test-Path $distParent)) { 
        New-Item -ItemType Directory -Path $distParent | Out-Null 
    }
    $ZipOutput = Join-Path $distParent "MIDI2Bridge-v5.1.0-Windows-x64.zip"
    if (Test-Path $ZipOutput) { 
        Remove-Item -Force $ZipOutput 
    }

    Compress-Archive -Path "$DistDir\*" -DestinationPath $ZipOutput -CompressionLevel Optimal
    if (-not (Test-Path $ZipOutput)) {
        throw "Falha ao gerar o arquivo compactado $ZipOutput"
    }

    $zipSize = (Get-Item $ZipOutput).Length / 1MB
    Log-Success "Pacote Windows gerado com sucesso: $ZipOutput ($([math]::Round($zipSize, 2)) MB)"

} catch {
    Log-Error "FALHA CRITICA: $_"
    exit 1
}
