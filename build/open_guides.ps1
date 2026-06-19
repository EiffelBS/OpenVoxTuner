# open_guides.ps1
# Ouvre rapidement les guides de documentation

param(
    [ValidateSet("Debug", "Build", "QuickFix", "All")]
    [string]$Guide = "All"
)

$rootDir = Join-Path $PSScriptRoot ".."

function Open-Guide {
    param([string]$Path, [string]$Name)

    if (Test-Path $Path) {
        Write-Host "Ouverture de $Name..." -ForegroundColor Green
        Start-Process $Path
    } else {
        Write-Host "AVERTISSEMENT: $Name introuvable : $Path" -ForegroundColor Yellow
    }
}

Write-Host "=== Ouverture des Guides ===" -ForegroundColor Cyan
Write-Host ""

switch ($Guide) {
    "Debug" {
        Open-Guide (Join-Path $rootDir "DEBUG_GUIDE.md") "Guide de Debogage"
    }
    "Build" {
        Open-Guide (Join-Path $rootDir "BUILD_GUIDE.md") "Guide de Build"
    }
    "QuickFix" {
        Open-Guide (Join-Path $PSScriptRoot "QUICKFIX_ALL_BUILD_ERROR.md") "QuickFix ALL_BUILD"
    }
    "All" {
        Open-Guide (Join-Path $rootDir "README.md") "README Principal"
        Start-Sleep -Milliseconds 200
        Open-Guide (Join-Path $rootDir "BUILD_GUIDE.md") "Guide de Build"
        Start-Sleep -Milliseconds 200
        Open-Guide (Join-Path $rootDir "DEBUG_GUIDE.md") "Guide de Debogage"
        Start-Sleep -Milliseconds 200
        Open-Guide (Join-Path $PSScriptRoot "QUICKFIX_ALL_BUILD_ERROR.md") "QuickFix ALL_BUILD"
    }
}

Write-Host ""
Write-Host "Guides ouverts !" -ForegroundColor Green
