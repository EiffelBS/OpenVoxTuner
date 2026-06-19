# GitHub Setup & Release Checklist

Repository: `https://github.com/EiffelBS/OpenVoxTuner`

## 1) First push

From project root:

```bash
git init
git branch -M main
git remote add origin https://github.com/EiffelBS/OpenVoxTuner.git
```

If `origin` already exists:

```bash
git remote set-url origin https://github.com/EiffelBS/OpenVoxTuner.git
```

Then:

```bash
git add .
git commit -m "Initial repository setup"
git push -u origin main
```

## 2) CI

A GitHub Actions workflow is available in:
- `.github/workflows/ci.yml`

It builds:
- Windows: VST3
- macOS: VST3 + AU + Standalone

## 3) Issue templates

Configured in:
- `.github/ISSUE_TEMPLATE/bug_report.yml`
- `.github/ISSUE_TEMPLATE/feature_request.yml`

## 4) Release notes workflow

Templates:
- `.github/RELEASE_TEMPLATE.md`
- `docs/releases/v0.1.1.md`

Suggested release flow:

```bash
git checkout main
git pull
# update docs/releases/vX.Y.Z.md
git add .
git commit -m "Prepare release vX.Y.Z"
git tag vX.Y.Z
git push origin main --tags
```

Then create GitHub Release from tag `vX.Y.Z` and paste notes based on the template.
