# release.ps1
# Release automation script for the OpenVoxTuner plugin.
# --------------------------------------------------------------------
# Usage: .\release.ps1 -Version "1.2.0"
# --------------------------------------------------------------------
param(
    [Parameter(Mandatory = $true)]
    [string]$Version
)

function Write-Log { param($msg) Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" }

Write-Log "Starting release for version $Version"

$repoRoot = Split-Path $PSScriptRoot -Parent

# --------------------------------------------------------------------
# 1. Update BuildInfo.h.in (OVT_PROJECT_VERSION macro)
# --------------------------------------------------------------------
$inFile = Join-Path $repoRoot 'Source\BuildInfo.h.in'
if (-not (Test-Path $inFile)) {
    Write-Error "File '$inFile' not found. Make sure the project is up to date."
    exit 1
}
Write-Log "Updating BuildInfo.h.in ..."
$lines = Get-Content -Path $inFile
# Replace the line containing @PROJECT_VERSION@
$updatedLines = foreach ($line in $lines) {
    if ($line -match '@PROJECT_VERSION@') {
        '#define OVT_PROJECT_VERSION "' + $Version + '"'
    } else { $line }
}
Set-Content -Path $inFile -Value $updatedLines
Write-Log "BuildInfo.h.in updated"

# --------------------------------------------------------------------
# 2. Commit & Git tag
# --------------------------------------------------------------------
# Stash local modifications before pulling
git stash push -u -m "pre-release-stash" | Out-Null

# Pull latest changes with rebase
git pull --rebase --quiet
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Pull with rebase failed. Trying to recover after stashing." 
}

# Restore the stashed changes
git stash pop -q

# Stage all changes (including BuildInfo.h.in)
git add -A
$commitMsg = "Bump plugin version to $Version"
git commit -m "$commitMsg" -q
if ($LASTEXITCODE -ne 0) {
    Write-Error "Commit failed. Check for conflicts or other uncommitted changes."
}
Write-Log "Commit done: $commitMsg"

# Tag (without the quiet option)
git tag -a v$Version -m "Release $Version"
if ($LASTEXITCODE -ne 0) { Write-Error "Tag creation failed." }
Write-Log "Tag created: v$Version"

# --------------------------------------------------------------------
# 3. Push (commit + tags)
# --------------------------------------------------------------------
# Pull with rebase before pushing to avoid rejections
git pull --rebase --quiet
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Pull failed. Please synchronize your branch before pushing."
}

git push origin main -q

git push --tags -q
Write-Log "Push completed"

# --------------------------------------------------------------------
# 4. Build the plugin
# --------------------------------------------------------------------
Write-Log "Starting build ..."
./build.ps1 | Out-Null

# --------------------------------------------------------------------
# Package the artifacts (optional)
# --------------------------------------------------------------------
$artifactsDir = Join-Path $repoRoot 'build\OpenVoxTuner_artefacts'
if (Test-Path $artifactsDir) {
    $zipName = Join-Path $repoRoot "OpenVoxTuner_$Version.zip"
    Compress-Archive -Path (Join-Path $artifactsDir '*') -DestinationPath $zipName -Force
    Write-Log "Artifacts packaged: $zipName"
    # Avoid including the zip in git
    if (Test-Path $zipName) {
        git rm --cached -q "$zipName" 2>$null
    }
} else {
    Write-Warning "Artifacts directory not found. The build may have failed."
}

# Create a GitHub release and upload the archive (if gh CLI is available)
try {
    if (Get-Command gh -ErrorAction SilentlyContinue) {
        $tag = "v$Version"
        Write-Log "Creating GitHub release for $tag..."
        gh release create $tag --title "Release $Version" --notes "" --draft | Out-Null
        if (Test-Path $zipName) {
            gh release upload $tag "$zipName" | Out-Null
            Write-Log "Archive uploaded to the release."
        }
    } else {
        Write-Warning "GitHub CLI 'gh' not found - the archive is not uploaded as a release asset."
    }
} catch {
    Write-Warning "Error while creating/uploading the GitHub release: $_"
}

Write-Host "Release $Version ready."