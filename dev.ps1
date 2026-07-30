#!/usr/bin/env pwsh
# Quick start script for local development

Write-Host "🚀 OpenVoxTuner Site - Local Development" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# Check prerequisites
Write-Host "`n📋 Checking prerequisites..." -ForegroundColor Yellow

$nodeVersion = node --version 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Error "❌ Node.js not found. Install from https://nodejs.org/"
    exit 1
}
Write-Host "  ✅ Node.js $nodeVersion" -ForegroundColor Green

$pythonVersion = python --version 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Error "❌ Python not found. Install from https://python.org/"
    exit 1
}
Write-Host "  ✅ $pythonVersion" -ForegroundColor Green

# Install Astro dependencies
Write-Host "`n📦 Installing Astro dependencies..." -ForegroundColor Yellow
Set-Location "site"
if (-not (Test-Path "node_modules")) {
    npm install
} else {
    Write-Host "  ✅ node_modules already exists" -ForegroundColor Green
}

# Install MkDocs dependencies
Write-Host "`n📚 Installing MkDocs dependencies..." -ForegroundColor Yellow
Set-Location "..\docs"
if (-not (Test-Path "venv")) {
    python -m venv venv
}
.\venv\Scripts\Activate.ps1
pip install --upgrade pip -q
pip install mkdocs-material mkdocs-git-revision-date-localized-plugin mkdocs-git-committers-plugin-2 pymdown-extensions -q
Write-Host "  ✅ MkDocs dependencies installed" -ForegroundColor Green

# Start dev servers
Write-Host "`n🌐 Starting development servers..." -ForegroundColor Cyan
Write-Host "  Astro site:    http://localhost:4321" -ForegroundColor Green
Write-Host "  MkDocs docs:   http://localhost:8000" -ForegroundColor Green
Write-Host "`nPress Ctrl+C to stop both servers" -ForegroundColor Yellow

# Start Astro in background
Set-Location "..\site"
$astroJob = Start-Job -ScriptBlock { npm run dev } -WorkingDirectory (Get-Location)

# Start MkDocs in background
Set-Location "..\docs"
$mkdocsJob = Start-Job -ScriptBlock { 
    .\venv\Scripts\Activate.ps1
    mkdocs serve -a localhost:8000 
} -WorkingDirectory (Get-Location)

# Wait for user to stop
try {
    while ($true) { Start-Sleep 1 }
}
finally {
    Write-Host "`n🛑 Stopping servers..." -ForegroundColor Yellow
    $astroJob | Stop-Job | Remove-Job
    $mkdocsJob | Stop-Job | Remove-Job
    Write-Host "✅ Done" -ForegroundColor Green
}