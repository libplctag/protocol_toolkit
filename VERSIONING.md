# Protocol Toolkit Versioning System

This document describes the automatic versioning system implemented for the Protocol Toolkit project.

## Overview

The versioning system uses semantic versioning (major.minor.patch) with automated workflows to:
- Create release candidates from the prerelease branch
- Validate releases before they are created
- Automatically create GitHub releases with proper tagging
- Increment patch versions after each release
- Maintain version consistency across all project files

## Workflow

```
dev branch → prerelease branch → release branch → GitHub Release
     ↓              ↓                    ↓              ↓
  Development   Testing &           Release         Tagged
   Changes     Validation         Creation        Release
```

## Branch Structure

### `dev` Branch
- Main development branch
- All feature development happens here
- Continuous integration runs on every push/PR
- Comprehensive testing across all platforms and architectures

### `prerelease` Branch
- Staging branch for release candidates
- When tests pass, automatically creates PR to `release` branch
- Runs full test suite before PR creation
- Manual review required for release approval

### `release` Branch
- Production-ready releases only
- Manual merge required (no auto-merge)
- Triggers automatic release creation on merge
- Version is auto-incremented after release

## Version Management

### Version File Structure

```
VERSION                    # Simple version file: "0.1.0"
scripts/version_manager.sh # Version management script
src/include/ptk_version.h  # Generated C header with version macros
CMakeLists.txt            # Updated with project version
```

### Branch Propagation Chain

The system automatically propagates version changes through branches:

```
release (0.1.1) → dev (0.1.1) → prerelease (0.1.1)
     ↓                ↓              ↓
  Auto-merge      Auto-merge    Ready for next
  (or PR)         (or PR)       release cycle
```

**Propagation Strategy:**
- **Fast-forward merge**: When possible (clean linear history)
- **Pull Request**: When branches have diverged (requires manual merge)
- **Automatic detection**: System chooses the appropriate method

### Version Manager Script

The `scripts/version_manager.sh` script provides:

```bash
# Get current version
./scripts/version_manager.sh get

# Increment patch version (1.0.0 → 1.0.1)
./scripts/version_manager.sh increment-patch

# Set specific version
./scripts/version_manager.sh set 2.1.0

# Set major.minor with patch reset to 0
./scripts/version_manager.sh set-major-minor 2 1

# Validate version format
./scripts/version_manager.sh validate 1.2.3

# Update all project files with current version
./scripts/version_manager.sh update-files
```

## GitHub Workflows

### 1. Prerelease Testing (`.github/workflows/prerelease.yml`)

**Trigger**: Push to `prerelease` branch

**Actions**:
- Runs comprehensive test suite
- Tests multiple architectures and sanitizers
- Cross-platform validation
- Creates PR to `release` branch if all tests pass
- Updates existing PR with latest test results

### 2. Release Management (`.github/workflows/release.yml`)

**Trigger**: Push/PR to `release` branch

**PR Actions** (Validation):
- Validates version format
- Checks release doesn't already exist
- Verifies version consistency across files
- Builds and tests release candidate
- Adds validation comment to PR

**Push Actions** (Release Creation):
- Creates GitHub release with version tag
- Generates release notes from commit history
- Builds and attaches release artifacts
- Increments patch version for next development
- Creates sync PR back to dev branch

### 3. Development CI (`.github/workflows/ci.yml`)

**Trigger**: Push/PR to `dev` branch

**Actions**:
- Comprehensive testing across platforms
- Architecture support: x86_64, x86, ARM64, ARMv6, ARMv7
- Sanitizer testing: ASAN, MSAN, UBSAN, TSAN
- Valgrind memory analysis
- Static analysis with Clang and Cppcheck

## Release Process

### Automated Steps

1. **Development**: Work on `dev` branch
2. **Staging**: Merge `dev` → `prerelease` when ready
3. **Validation**: Prerelease workflow runs comprehensive tests
4. **PR Creation**: If tests pass, auto-creates PR to `release` branch
5. **Manual Review**: Review and approve the PR
6. **Manual Merge**: Manually merge PR to `release` branch
7. **Release Creation**: Release workflow automatically:
   - Creates GitHub release with version tag
   - Generates release notes and artifacts
   - Increments patch version
   - **Propagates changes**: `release` → `dev` → `prerelease`

### Branch Propagation Details

After each release, version changes are automatically propagated:

#### Step 1: Release → Dev
- **Fast-forward merge**: If `dev` history is clean
- **Pull Request**: If branches have diverged
- **Auto-detection**: System chooses best approach

#### Step 2: Dev → Prerelease  
- **Fast-forward merge**: If `prerelease` history is clean
- **Pull Request**: If branches have diverged
- **Conditional**: Only runs if Step 1 succeeded

#### Fallback Strategy
If auto-merge fails, the system creates PRs with clear explanations:
- **Why manual merge is needed**
- **What changes are being synced**
- **How to resolve conflicts**

### Manual Steps Required

- **PR Review**: All PRs to `release` branch require manual review
- **PR Merge**: Manual merge required (auto-merge disabled)
- **Version Updates**: Use version manager script for major/minor changes

## Version Increment Rules

### Automatic (Patch Version)
- Incremented after each release
- `1.0.0` → `1.0.1` → `1.0.2` → etc.
- Happens automatically in release workflow

### Manual (Major/Minor Version)
- Use version manager script
- Resets patch to 0
- Example: `1.5.3` → `2.0.0` (major) or `1.6.0` (minor)

```bash
# Increment major version
./scripts/version_manager.sh set-major-minor 2 0

# Increment minor version
./scripts/version_manager.sh set-major-minor 1 6
```

## File Updates

When version changes, the following files are automatically updated:

### `VERSION`
```
1.0.0
```

### `src/include/ptk_version.h`
```c
#define PTK_VERSION_MAJOR 1
#define PTK_VERSION_MINOR 0
#define PTK_VERSION_PATCH 0
#define PTK_VERSION_STRING "1.0.0"
```

### `CMakeLists.txt`
```cmake
project(protocol_toolkit VERSION 1.0.0)
```

## Release Artifacts

Each release includes:

1. **Source Code**: Tarball and ZIP of complete source
2. **Build Artifacts**: Pre-built libraries and test harness
3. **Release Notes**: Auto-generated from commit history
4. **Test Results**: Links to CI/CD pipeline results

## Security Considerations

- All workflows use GitHub's built-in `GITHUB_TOKEN`
- No external secrets or credentials required
- Manual approval required for all releases
- Version validation prevents duplicate releases

## Troubleshooting

### Common Issues

1. **Version Already Exists**
   - Check existing releases: `gh release list`
   - Increment version: `./scripts/version_manager.sh increment-patch`

2. **Version Inconsistency**
   - Fix with: `./scripts/version_manager.sh update-files`
   - Commit the changes

3. **Failed Release Creation**
   - Check GitHub Actions logs
   - Verify version format and uniqueness
   - Ensure all tests pass in prerelease

4. **Branch Propagation Failures**
   - **Auto-merge failed**: Check for divergent branch history
   - **Fast-forward blocked**: Manual merge required via PR
   - **Missing branches**: Ensure `dev` and `prerelease` branches exist
   - **Permission issues**: Verify GitHub Actions has push permissions

### Branch Propagation Troubleshooting

#### Manual Branch Sync
If automatic propagation fails, manually sync branches:

```bash
# Sync release → dev
git checkout dev
git pull origin dev
git merge origin/release
git push origin dev

# Sync dev → prerelease  
git checkout prerelease
git pull origin prerelease
git merge origin/dev
git push origin prerelease
```

#### Check Branch Relationships
```bash
# Check if dev can fast-forward to release
git merge-base --is-ancestor origin/dev origin/release

# Check if prerelease can fast-forward to dev
git merge-base --is-ancestor origin/prerelease origin/dev

# View branch divergence
git log --oneline --graph origin/dev...origin/release
```

#### Reset Branch Strategy
For severely divergent branches, reset to match:

```bash
# Reset prerelease to match dev (DESTRUCTIVE)
git checkout prerelease
git reset --hard origin/dev
git push --force-with-lease origin prerelease
```
**⚠️ Warning**: Only use `--force-with-lease` when you understand the consequences.

### Debug Commands

```bash
# Check current version
./scripts/version_manager.sh get

# Validate version format
./scripts/version_manager.sh validate $(./scripts/version_manager.sh get)

# Check if release exists
gh release view "v$(./scripts/version_manager.sh get)"

# List recent releases
gh release list --limit 10
```

## Configuration

### Environment Variables

None required - all configuration is in files.

### Settings

- Release retention: GitHub default (indefinite)
- Artifact retention: 30 days (static analysis), 7 days (test artifacts)
- PR auto-merge: Disabled for `release` branch

## Best Practices

1. **Always test in prerelease** before creating releases
2. **Use semantic versioning** appropriately
3. **Write meaningful commit messages** (used in release notes)
4. **Review PRs carefully** before merging to release
5. **Monitor CI/CD pipelines** for any failures
6. **Keep version files in sync** using the version manager script

## Migration Guide

### From Manual Versioning

1. Ensure current version is in `VERSION` file
2. Run `./scripts/version_manager.sh update-files`
3. Commit version files
4. Follow normal development workflow

### Updating Major/Minor Versions

```bash
# For major version change (2.0.0)
./scripts/version_manager.sh set-major-minor 2 0
./scripts/version_manager.sh update-files
git add VERSION src/include/ptk_version.h CMakeLists.txt
git commit -m "Bump version to 2.0.0"

# Continue with normal release process
```

This versioning system ensures consistent, automated, and reliable releases while maintaining full control over the release process through manual approvals.