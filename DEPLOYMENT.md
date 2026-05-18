# Amplitron - Deployment Guide

This document explains the CI/CD pipeline and how to create releases.

## GitHub Actions Workflows

### 1. CI Workflow (`.github/workflows/ci.yml`)

**Triggers**: Push to `main` or `develop`, Pull Requests to `develop`

**What it does**:
- Builds Amplitron on Windows, macOS, Linux, Android, iOS, and Web (Emscripten)
- Runs the full test suite (105+ tests) on all native desktop platforms
- Generates semantic version (`0.1.<commit_count>`)
- Caches dependencies (apt packages, Emscripten SDK, ccache)
- Uploads build artifacts (1-day retention)

**Platforms**:
- **Windows**: MSYS2/MinGW64 with Ninja, ccache
- **macOS**: Homebrew dependencies, dylib bundling
- **Linux**: Ubuntu with apt packages
- **Android**: NDK r27, Gradle, produces release APK
- **iOS**: Xcode (iOS Simulator), produces `.ipa` via Payload zip
- **Web**: Emscripten 3.1.51, WebAssembly + AudioWorklet

### 2. Release Workflow (`.github/workflows/release.yml`)

**Triggers**: Automatically on successful CI workflow on `main` branch

**What it does**:
1. Creates a GitHub Release with version `v0.1.<commit_count>`
2. Downloads build artifacts from the CI run
3. Packages platform-specific installers:
   - **Windows**: `Amplitron-Windows-Setup.exe` (NSIS installer with DLLs, shortcuts)
   - **macOS**: `Amplitron-macOS.dmg` (app bundle with dylibs, ad-hoc codesigned)
   - **Linux**: `Amplitron-Linux-x64.tar.gz` (binary + launcher script)
   - **Android**: `Amplitron-Android.apk` (renamed from Gradle release output)
   - **iOS**: `Amplitron-iOS.ipa` (Payload zip of the simulator `.app` bundle)
4. Uploads installers to the release
5. Deploys web demo and download page to GitHub Pages

### 3. PR Preview Workflow (`.github/workflows/deploy-preview.yml`)

**Triggers**: Completed CI runs for Pull Requests to `main` or `develop`, and closed Pull Requests

**What they do**:
1. Reuse the `web-build` artifact from the existing CI workflow
2. Stage only the static preview files (`index.html`, JavaScript, WebAssembly, data, worker, and service worker files)
3. Deploy the preview to GitHub Pages under `pr-previews/pr-<number>/`
4. Post or update a PR comment with the live preview URL
5. Remove the preview directory automatically when the PR is closed

Preview URLs follow this format:

```text
https://amplitron.sudipmondal.co.in/pr-previews/pr-<number>/
```

The preview deployment intentionally reuses the existing CI web build instead of compiling the Emscripten target a second time. Pull Request code is built with read-only CI permissions, while the deploy workflow only publishes the trusted CI artifact to GitHub Pages.

Because GitHub only runs `workflow_run` workflows that already exist on the default branch, this preview workflow starts creating URLs after it is merged to `main`. The pull request that introduces the workflow cannot publish its own preview from the new workflow file.

## Creating a Release

### Step 1: Prepare the Release

1. Ensure all tests pass locally:
   ```bash
   cd build
   ./amplitron-tests
   ```

2. Update version numbers if needed (in CMakeLists.txt, docs, etc.)

3. Commit all changes:
   ```bash
   git add -A
   git commit -m "Prepare release v1.0.0"
   git push origin main
   ```

### Step 2: Push to `main`

Releases are triggered automatically when CI passes on `main`. No manual tagging is required — the version is generated as `v0.1.<commit_count>`.

```bash
git push origin main
```

### Step 3: Wait for Automation

GitHub Actions will automatically:
- ✅ Build for all platforms (Windows, macOS, Linux, Android, iOS, Web)
- ✅ Run tests (105+ tests on desktop platforms)
- ✅ Create the release with semantic version
- ✅ Package platform-specific installers
- ✅ Upload binaries to the release
- ✅ Deploy website and web demo to https://amplitron.sudipmondal.co.in

Monitor progress at: https://github.com/sudip-mondal-2002/Amplitron/actions

### Step 4: Verify the Release

1. Check the release page: https://github.com/sudip-mondal-2002/Amplitron/releases
2. Download and test each platform binary
3. Verify the website is live

## Manual Release

To trigger a release manually, push to `main` and let CI complete. The release workflow is automatically triggered by a successful CI run on `main`. There is no manual workflow dispatch — releases are fully automated.

## GitHub Pages Setup

### Enable GitHub Pages

1. Go to repository Settings → Pages
2. Source: Deploy from a branch
3. Branch: `gh-pages` / `root`
4. Click Save

The website will be available at: https://amplitron.sudipmondal.co.in/

### Update the Download Page

Edit `docs/index.html` to update:
- Version numbers
- Feature descriptions
- Download links
- Screenshots

Commit and push changes - GitHub Pages will auto-deploy.

## Platform-Specific Packaging

### Windows (NSIS Installer via `scripts/installer.nsi`)

Creates `Amplitron-Windows-Setup.exe` containing:
- `Amplitron.exe`
- MinGW runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`)
- PortAudio and SDL2 DLLs
- Assets
- Start Menu and Desktop shortcuts
- Uninstaller

### macOS (DMG via release workflow)

Creates `Amplitron-macOS.dmg` with:
- `Amplitron.app` bundle (`Contents/MacOS/Amplitron`)
- Bundled dylibs in `Contents/Frameworks/` (rpath-rewritten)
- `Info.plist` with metadata and microphone usage description
- App icon (icns)
- Ad-hoc code signing
- Applications symlink for drag-to-install

### Linux (`Amplitron-Linux-x64.tar.gz`)

Creates a tarball with:
- `amplitron` binary
- `amplitron.sh` launcher script
- Assets and README

### Android (`Amplitron-Android.apk`)

Built with Gradle + Android NDK r27. The CI produces an unsigned release APK which is renamed to `Amplitron-Android.apk`. Users install it by enabling **Settings → Install unknown apps** for their browser or file manager.

### iOS (`Amplitron-iOS.ipa`)

Built with CMake → Xcode for the iOS Simulator target (arm64, no code signing). The CI packages the `.app` bundle into an IPA using the standard Payload zip format:
```text
Amplitron.ipa
└── Payload/
    └── Amplitron.app/
```

**Installation via AltStore** (free, no paid developer account needed):
1. Install [AltStore](https://altstore.io) on your iPhone (requires a Mac or PC once)
2. Open the [latest release](https://github.com/sudip-mondal-2002/Amplitron/releases/latest) in Safari on your iPhone and tap `Amplitron-iOS.ipa`
3. Tap **Share → AltStore** to install
4. AltStore silently refreshes the 7-day certificate while on the same Wi-Fi as your computer — no action needed from the user

## Troubleshooting

### CI Build Fails

1. Check the Actions logs
2. Common issues:
   - Missing dependencies
   - Test failures
   - CMake configuration errors

### Release Upload Fails

- Ensure `GITHUB_TOKEN` has proper permissions
- Check if the tag already exists
- Verify file paths in the workflow

### GitHub Pages Not Updating

1. Check Actions → pages-build-deployment
2. Ensure `gh-pages` branch exists
3. Verify Settings → Pages is configured correctly

## Version Numbering

Versions are auto-generated by CI as `v0.1.<commit_count>`, where `<commit_count>` is the total number of commits in the repository history. This ensures every push to `main` produces a unique, monotonically increasing version number.

The version is passed to CMake via `-DAMPLITRON_VERSION` and compiled into the binary, where it is displayed in the GUI and used for the release update checker.

## Contact

For CI/CD issues or questions:
- Email: sudmondal2002@gmail.com
- GitHub Issues: https://github.com/sudip-mondal-2002/Amplitron/issues
