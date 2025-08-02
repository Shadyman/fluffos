# FluffOS Fork Upstream Safety

This document explains the safety measures in place to prevent fork-specific files from being accidentally submitted to upstream FluffOS.

## Protection Mechanisms

### 1. Git Attributes (`export-ignore`)

The `.gitattributes` file contains:
```
tools/ export-ignore
.github/copilot-instructions.md export-ignore
```

This ensures that:
- The `tools/` directory is excluded from `git archive` commands
- Fork-specific GitHub Copilot instructions are excluded
- GitHub release archives won't include fork-specific files
- Patch files won't include the protected directories/files
- CI workflows and other legitimate `.github/` content is preserved

### 2. Clean Upstream Branch Creation

The development script includes an `upstream-branch` command:

```bash
./tools/fluffos_dev.sh upstream-branch contrib-my-feature
```

This command:
- Creates a branch from `upstream/master` (not your fork's main branch)
- Ensures a clean starting point for contributions
- Automatically sets up the upstream remote if needed

### 3. Documentation and Guidelines

Clear documentation in `tools/README.md` explains:
- How to contribute back to upstream
- Which files are fork-specific
- Best practices for clean contributions

## Workflow for Upstream Contributions

### Safe Contribution Process

1. **Create Clean Branch**:
   ```bash
   ./tools/fluffos_dev.sh upstream-branch contrib-database-fix
   ```

2. **Make Changes**: Only modify core FluffOS files, avoid fork-specific modifications

3. **Test Thoroughly**: Ensure changes work in vanilla FluffOS

4. **Push and Create PR**:
   ```bash
   git push origin contrib-database-fix
   # Create PR from shadyman/fluffos:contrib-database-fix to fluffos/fluffos:master
   ```

### What Gets Excluded

- `tools/` directory (development scripts and documentation)
- `.github/copilot-instructions.md` (fork-specific GitHub Copilot configuration)
- Any future fork-specific directories added to `.gitattributes`
- Project-specific configuration files

### What Can Be Contributed

- Core FluffOS improvements
- Bug fixes
- New LPC features
- Documentation improvements (to main docs)
- Package enhancements

## Verification

You can verify what would be included in an archive:

```bash
# Test archive contents (tools/ and copilot-instructions.md should be excluded)
git archive --format=tar HEAD | tar -tf - | grep -E "(^tools/|copilot-instructions)"
# Should show no results

# Or create a test archive
git archive --format=tar.gz --prefix=fluffos/ HEAD -o test-archive.tar.gz
tar -tzf test-archive.tar.gz | grep -E "(^fluffos/tools/|copilot-instructions)"
```

## Branch Strategy for Contributions

### Fork-Specific Branches
- `crypto-modern-hash-algorithms` - Main development branch with all enhancements
- `feat-*` - Feature branches based on main development branch
- `fix-*` - Bug fix branches for fork-specific issues

### Upstream Contribution Branches
- `contrib-*` - Clean branches from upstream/master
- `upstream-*` - Branches for integrating upstream changes
- Based on `upstream/master`, not your fork's main branch

## Example Scenarios

### Scenario 1: Contributing a Bug Fix

```bash
# Create clean upstream branch
./tools/fluffos_dev.sh upstream-branch contrib-fix-memory-leak

# Make the fix (only core FluffOS files)
vim src/base/interpret.cc

# Commit and push
git add src/base/interpret.cc
git commit -m "Fix memory leak in interpret.cc"
git push origin contrib-fix-memory-leak

# Create PR to fluffos/fluffos
```

### Scenario 2: Adding Fork-Specific Feature

```bash
# Work on main development branch
git checkout crypto-modern-hash-algorithms

# Add feature with MUD-specific enhancements
vim src/packages/contrib/contrib.cc
./tools/fluffos_dev.sh commit 'Add MUD-specific database pooling'

# This stays in your fork, not contributed upstream
```

## Maintenance

### Adding New Protected Directories

If you add more fork-specific directories:

```bash
# Edit .gitattributes
echo "my-fork-stuff/ export-ignore" >> .gitattributes
git commit -m "Exclude my-fork-stuff from upstream archives"
```

### Updating Safety Measures

The safety measures are part of the tools themselves, so they get updated when you update the tools:

```bash
./tools/fluffos_dev.sh sync  # Updates tools along with other changes
```

## Benefits

1. **Prevents Accidents**: Fork-specific files can't accidentally be included in upstream contributions
2. **Clean Contributions**: Upstream branches start from clean upstream state
3. **Automatic Protection**: Works without manual intervention
4. **Documented Process**: Clear guidelines for contributors
5. **Verifiable**: Can test what would be included in contributions

This system ensures your fork can evolve independently while still being able to contribute cleanly back to the upstream FluffOS project.

---

*Safety measures implemented: August 1, 2025*  
*Part of Shadyman's FluffOS fork development tools*
