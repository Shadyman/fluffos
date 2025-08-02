# FluffOS Development Tools

This directory contains development tools for Shadyman's FluffOS fork, designed to work in any project that uses this fork as a submodule or directly.

## Quick Start

```bash
# From within the FluffOS directory
./tools/fluffos_dev.sh status

# Or from a parent project using FluffOS as submodule
./fluffos/tools/fluffos_dev.sh status
```

## Tools Overview

### `fluffos_dev.sh` - Development Workflow Script

A comprehensive script for managing FluffOS development tasks including branching, committing, syncing, and building.

## Usage

```bash
./tools/fluffos_dev.sh <command> [arguments]
```

### Available Commands

#### Core Development Commands

- **`status`** - Show current git status and repository information
- **`branch <name>`** - Create and switch to a new feature branch
- **`commit '<message>'`** - Stage, commit, and push changes
- **`sync`** - Sync current branch with your fork
- **`info`** - Show detailed branch and repository information

#### Advanced Commands

- **`upstream`** - Fetch and show upstream merge options
- **`build`** - Build FluffOS using cmake
- **`main [branch]`** - Set or show the main development branch

### Configuration

The script uses environment variables for configuration:

- **`FLUFFOS_MAIN_BRANCH`** - Set the default main branch (default: `crypto-modern-hash-algorithms`)

```bash
# Set main branch for session
export FLUFFOS_MAIN_BRANCH=master

# Or set for a single command
FLUFFOS_MAIN_BRANCH=master ./tools/fluffos_dev.sh status
```

## Repository Structure

This FluffOS fork includes enhancements for:

### Current Modifications (crypto-modern-hash-algorithms branch)

1. **Enhanced JSON Package**
   - Location: `src/packages/json/`
   - Features: Improved JSON handling for database integration
   - Files: `json.cc`, `json.spec`, `CMakeLists.txt`

2. **Database Integration Improvements**
   - Modified: `src/packages/contrib/contrib.cc`
   - Features: Connection pooling, async query support
   - Purpose: Better PostgreSQL integration for MUD applications

3. **Build System Updates**
   - Modified: `src/CMakeLists.txt`, `src/local_options`
   - Purpose: Optimized builds for MUD environments
   - Features: Enhanced compiler options and package selection

4. **Parser Enhancements**
   - Modified: `src/compiler/internal/grammar.autogen.*`
   - Purpose: Support for extended LPC syntax patterns

## Workflow Examples

### Starting New Development

```bash
# Check current status
./tools/fluffos_dev.sh status

# Create feature branch
./tools/fluffos_dev.sh branch my-new-feature

# Work on changes...
# Edit source files in src/

# Commit changes
./tools/fluffos_dev.sh commit 'Add new feature for MUD integration'
```

### Building FluffOS

```bash
# Build with development script
./tools/fluffos_dev.sh build

# Or manually
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Syncing with Updates

```bash
# Sync with your fork
./tools/fluffos_dev.sh sync

# Check for upstream updates
./tools/fluffos_dev.sh upstream

# If you want to merge upstream changes
git merge upstream/master
git push origin crypto-modern-hash-algorithms
```

## Integration with Parent Projects

When using this FluffOS fork as a submodule in other projects:

### Adding as Submodule

```bash
# In your parent project
git submodule add https://github.com/shadyman/fluffos.git fluffos
git submodule update --init --recursive
```

### Using the Development Tools

```bash
# From parent project root
./fluffos/tools/fluffos_dev.sh status

# Create alias for convenience
alias fluffos-dev='./fluffos/tools/fluffos_dev.sh'
fluffos-dev status
```

### Environment Setup in Parent Project

Add to your parent project's development scripts:

```bash
#!/bin/bash
# setup-dev.sh in parent project

# Set FluffOS main branch for your project
export FLUFFOS_MAIN_BRANCH=crypto-modern-hash-algorithms

# Add FluffOS development tools to PATH
export PATH="$PWD/fluffos/tools:$PATH"

echo "FluffOS development environment ready"
echo "Use 'fluffos_dev.sh <command>' from anywhere in project"
```

## Branch Strategy

### Main Development Branch
- **crypto-modern-hash-algorithms** - Primary development branch with MUD enhancements
- Contains all stable modifications for database integration and AI features

### Feature Branch Naming
- `feat-<description>` - New features
- `fix-<description>` - Bug fixes  
- `enhance-<description>` - Improvements to existing features
- `upstream-<description>` - Integration of upstream changes

### Example Workflow

```bash
# Start new feature
./tools/fluffos_dev.sh branch feat-async-queries

# Work on feature...
vim src/packages/contrib/contrib.cc

# Commit progress
./tools/fluffos_dev.sh commit 'WIP: Add async query support'

# More work...
./tools/fluffos_dev.sh commit 'Complete async query implementation'

# Merge back to main branch
git checkout crypto-modern-hash-algorithms
git merge feat-async-queries
git push origin crypto-modern-hash-algorithms
```

## Contributing to Upstream

To contribute features back to the main FluffOS project:

```bash
# Create clean feature branch from upstream
git checkout -b contrib-feature upstream/master

# Make changes without project-specific modifications
# Test thoroughly

# Push to your fork
git push origin contrib-feature

# Create pull request from your fork to fluffos/fluffos
```

**Note**: The `tools/` directory is marked with `export-ignore` in `.gitattributes`, so it won't be included when creating archives or patches for upstream contributions. This ensures your fork-specific development tools don't accidentally get submitted to the main FluffOS project.

## Troubleshooting

### Common Issues

1. **Permission Denied**
   ```bash
   chmod +x tools/fluffos_dev.sh
   ```

2. **Wrong Main Branch**
   ```bash
   ./tools/fluffos_dev.sh main crypto-modern-hash-algorithms
   ```

3. **Build Failures**
   ```bash
   # Clean build
   rm -rf build
   ./tools/fluffos_dev.sh build
   ```

4. **Submodule Issues**
   ```bash
   # From parent project
   git submodule update --init --recursive
   git submodule foreach git checkout crypto-modern-hash-algorithms
   ```

### Getting Help

1. Check the script help: `./tools/fluffos_dev.sh`
2. Review git status: `./tools/fluffos_dev.sh status`
3. Check FluffOS documentation: `docs/`
4. Review upstream issues: https://github.com/fluffos/fluffos/issues

## File Structure

```
tools/
├── fluffos_dev.sh      # Main development script
└── README.md          # This documentation

src/                   # FluffOS source code
├── packages/          # Package implementations
│   ├── json/         # Enhanced JSON package
│   └── contrib/      # Database integration
├── compiler/         # LPC compiler
└── CMakeLists.txt    # Build configuration

build/                # Build directory (created by cmake)
└── driver           # Compiled FluffOS binary
```

## Version History

- **v1.0** - Initial development tools
  - Basic workflow commands
  - Submodule detection
  - Build integration
  - Upstream sync capabilities

---

*Part of Shadyman's FluffOS fork*  
*Enhanced for MUD development with database integration and AI features*
