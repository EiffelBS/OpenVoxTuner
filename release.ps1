# release.ps1
# Script d’automatisation de la release du plugin OpenVoxTuner.
# --------------------------------------------------------------------
# Usage : .\release.ps1 -Version "1.2.0"
# --------------------------------------------------------------------
param(
    [Parameter(Mandatory = $true)]
    [string]$Version
)

function Write-Log { param($msg) Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" }

Write-Log "Début de la release pour la version $Version"

# --------------------------------------------------------------------
# 1️⃣ Mettre à jour BuildInfo.h.in (macro OVT_PROJECT_VERSION)
# --------------------------------------------------------------------
$inFile = Join-Path $PSScriptRoot 'Source\BuildInfo.h.in'
if (-not (Test-Path $inFile)) {
    Write-Error "Fichier '$inFile' introuvable. Assurez‑vous que le projet est à jour."
    exit 1
}
Write-Log "Mise à jour de BuildInfo.h.in …"
$lines = Get-Content -Path $inFile
# Remplacer la ligne contenant @PROJECT_VERSION@
$updatedLines = foreach ($line in $lines) {
    if ($line -match '@PROJECT_VERSION@') {
        '#define OVT_PROJECT_VERSION "' + $Version + '"'
    } else { $line }
}
Set-Content -Path $inFile -Value $updatedLines
Write-Log "BuildInfo.h.in mis à jour"

# --------------------------------------------------------------------
# 2️⃣ Commit & tag Git
# --------------------------------------------------------------------
# Stash local modifications before pulling
git stash push -u -m "pre-release-stash" | Out-Null

# Pull latest changes with rebase
git pull --rebase --quiet
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Pull avec rebase a échoué. Tentative de reprise après stashing." 
}

# Récupérer les modifications stashed
git stash pop -q

# Stage all changes (including BuildInfo.h.in)
git add -A
$commitMsg = "Bump plugin version to $Version"
git commit -m "$commitMsg" -q
if ($LASTEXITCODE -ne 0) {
    Write-Error "Commit échoué. Vérifiez qu’il n’y a pas de conflits ou d’autres changements non committés."
}
Write-Log "Commit effectué : $commitMsg"

# Tag (sans l’option silencieuse)
git tag -a v$Version -m "Release $Version"
if ($LASTEXITCODE -ne 0) { Write-Error "Création du tag échouée." }
Write-Log "Tag créé : v$Version"

# --------------------------------------------------------------------
# 3️⃣ Push (commit + tags)
# --------------------------------------------------------------------
# Push avec rebase pour éviter les rejets
git pull --rebase --quiet
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Échec du pull. Veuillez synchroniser votre branche avant de pousser."
}

git push origin main -q

git push --tags -q
Write-Log "Push terminé"

# --------------------------------------------------------------------
# 4️⃣ Build le plugin
# --------------------------------------------------------------------
Write-Log "Lancement de la build …"
./build.ps1 | Out-Null

# --------------------------------------------------------------------
# 5️⃣ Packager les artefacts (optionnel)
# --------------------------------------------------------------------
$artifactsDir = Join-Path $PSScriptRoot 'build\OpenVoxTuner_artefacts'
if (Test-Path $artifactsDir) {
    $zipName = Join-Path $PSScriptRoot "OpenVoxTuner_$Version.zip"
    Compress-Archive -Path (Join-Path $artifactsDir '*') -DestinationPath $zipName -Force
    Write-Log "Artefacts packagés : $zipName"
} else {
    Write-Warning "Répertoire des artefacts introuvable. La build a peut‑être échoué."
}

Write-Host "Release $Version prête."