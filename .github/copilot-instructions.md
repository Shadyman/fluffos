# GitHub Copilot Instructions for Shadyman's FluffOS Fork

## Project Overview

This is an enhanced FluffOS fork specifically designed for MUD (Multi-User Dungeon) development with advanced database integration, AI features, and comprehensive development tools. The fork includes MUD-specific enhancements while maintaining compatibility with upstream FluffOS.

**Repository**: https://github.com/shadyman/fluffos  
**Upstream**: https://github.com/fluffos/fluffos  
**Main Development Branch**: `crypto-modern-hash-algorithms`

## Key Enhancements

### Database Integration
- **Enhanced PostgreSQL support** with connection pooling and async queries
- **MUD-specific database features** for persistent game state
- **Optimized for Dead Souls MUD architecture** and similar codebases
- **AI-powered content generation** database backends

### JSON Package Improvements
- **Location**: `src/packages/json/`
- **Enhanced JSON handling** for database operations and AI integration
- **Custom CMake integration** for MUD environments

### Build System Optimizations
- **MUD-focused build configuration** in `src/local_options`
- **Optimized compiler settings** for game server performance
- **PostgreSQL integration flags** and dependencies

### Parser Enhancements
- **Extended LPC syntax support** for advanced MUD features
- **Grammar improvements** for database query integration
- **AI content generation syntax** extensions

## Development Tools (`tools/` Directory)

### `fluffos_dev.sh` - Main Development Script
**Purpose**: Comprehensive workflow management for FluffOS development across multiple projects.

**Key Features**:
- **Submodule detection** - Works whether in main repo or as submodule
- **Configurable main branch** via `FLUFFOS_MAIN_BRANCH` environment variable
- **Clean upstream contributions** - Creates branches from upstream/master
- **Build integration** - Handles cmake builds automatically
- **Multi-project support** - Same script works in any project using this fork

**Commands**:
```bash
./tools/fluffos_dev.sh status              # Show git status and repo info
./tools/fluffos_dev.sh branch <name>       # Create feature branch
./tools/fluffos_dev.sh commit '<msg>'      # Commit and push changes
./tools/fluffos_dev.sh sync               # Sync with your fork
./tools/fluffos_dev.sh upstream           # Fetch upstream updates
./tools/fluffos_dev.sh build              # Build FluffOS with cmake
./tools/fluffos_dev.sh upstream-branch <name>  # Clean upstream contribution branch
./tools/fluffos_dev.sh main [branch]      # Set/show main development branch
```

### `README.md` - Comprehensive Documentation
**Purpose**: Complete usage guide for the development tools and fork integration.

**Covers**:
- **Tool usage examples** and command reference
- **Repository structure** and modification details
- **Integration workflows** for parent projects
- **Branch strategy** and development patterns
- **Contribution guidelines** for upstream FluffOS
- **Troubleshooting** common issues

### `UPSTREAM_SAFETY.md` - Contribution Protection
**Purpose**: Ensures fork-specific tools never accidentally get submitted to upstream FluffOS.

**Protection Mechanisms**:
- **`.gitattributes` with `export-ignore`** - Excludes tools/ from archives
- **Clean branch creation** from upstream/master
- **Verification methods** for testing what gets included
- **Documented workflows** for safe contributions

## Git Configuration

### `.gitattributes` Protection
```
tools/ export-ignore
```
This ensures the `tools/` directory is excluded from git archives, preventing fork-specific development tools from being included in upstream contributions or releases.

### Submodule Configuration
When used as a submodule in other projects:
```ini
[submodule "fluffos"]
    path = fluffos
    url = https://github.com/shadyman/fluffos.git
    branch = crypto-modern-hash-algorithms
```

## Usage Patterns

### As Main Repository
```bash
# Direct development on FluffOS
git clone https://github.com/shadyman/fluffos.git
cd fluffos
./tools/fluffos_dev.sh status
```

### As Submodule in MUD Projects
```bash
# In parent MUD project
git submodule add -b crypto-modern-hash-algorithms https://github.com/shadyman/fluffos.git fluffos
./fluffos/tools/fluffos_dev.sh status

# Or use project wrapper script
./fluffos-dev status  # If parent project has wrapper
```

### Environment Configuration
```bash
# Set main development branch (default: crypto-modern-hash-algorithms)
export FLUFFOS_MAIN_BRANCH=crypto-modern-hash-algorithms

# For other branches if needed
export FLUFFOS_MAIN_BRANCH=master
```

## Development Workflows

### Feature Development
```bash
# Create feature branch
./tools/fluffos_dev.sh branch feat-database-pooling

# Work on FluffOS source code
vim src/packages/contrib/contrib.cc

# Commit and push
./tools/fluffos_dev.sh commit 'Add advanced database connection pooling'

# Build and test
./tools/fluffos_dev.sh build
```

### Upstream Contributions
```bash
# Create clean upstream branch (excludes fork-specific tools)
./tools/fluffos_dev.sh upstream-branch contrib-memory-fix

# Make changes (only to core FluffOS, no MUD-specific modifications)
vim src/base/interpret.cc

# Standard git workflow for upstream PR
git commit -m "Fix memory leak in interpret.cc"
git push origin contrib-memory-fix
# Create PR from shadyman/fluffos:contrib-memory-fix to fluffos/fluffos:master
```

### Syncing with Upstream
```bash
# Check for upstream updates
./tools/fluffos_dev.sh upstream

# If you want to merge upstream changes
git merge upstream/master
git push origin crypto-modern-hash-algorithms
```

## Integration with Parent Projects

### MUD Project Template Files
Parent projects need these files to use this FluffOS fork:

1. **`.gitmodules`** - Defines FluffOS submodule
2. **`.gitignore`** - MUD-appropriate ignore patterns
3. **`fluffos-dev`** - Wrapper script for easy tool access

### Example Parent Project Wrapper
```bash
#!/bin/bash
# fluffos-dev wrapper in parent project
export FLUFFOS_MAIN_BRANCH=crypto-modern-hash-algorithms
exec ./fluffos/tools/fluffos_dev.sh "$@"
```

## Code Modifications

### Current Enhancements (crypto-modern-hash-algorithms branch)

#### Database Integration (`src/packages/contrib/contrib.cc`)
- **Connection pooling** for MUD daemon efficiency
- **Async query support** for non-blocking database operations
- **Error handling improvements** for game server stability
- **PostgreSQL optimization** for MUD-specific use cases

#### JSON Package (`src/packages/json/`)
- **Enhanced JSON parsing** for database integration
- **AI content generation support** for dynamic MUD content
- **Performance optimizations** for real-time game data
- **Custom CMake integration** for MUD build systems

#### Build System (`src/CMakeLists.txt`, `src/local_options`)
- **MUD-optimized compiler flags** for game server performance
- **PostgreSQL integration** compile-time configuration
- **Debug/release profiles** for development and production
- **Package selection** optimized for MUD environments

#### Grammar Extensions (`src/compiler/internal/grammar.autogen.*`)
- **Extended LPC syntax** for database operations
- **AI content generation** language features
- **MUD-specific function support** and syntax sugar

## Best Practices for Contributors

### FluffOS Development
- **Use feature branches** for all development: `feat-`, `fix-`, `enhance-`
- **Test thoroughly** with actual MUD codebases before committing
- **Maintain backward compatibility** with existing MUD libraries
- **Document MUD-specific features** clearly in commit messages

### Upstream Contributions
- **Use `upstream-branch` command** to create clean contribution branches
- **Test with vanilla FluffOS** to ensure no MUD-specific dependencies
- **Follow upstream coding standards** and contribution guidelines
- **Exclude all fork-specific modifications** from upstream submissions

### Multi-Project Development
- **Set `FLUFFOS_MAIN_BRANCH`** appropriately for each project
- **Use submodules** for clean separation between FluffOS and MUD code
- **Keep FluffOS changes separate** from MUD library development
- **Version control FluffOS updates** in parent project commits

## Architecture Notes

### Why This Architecture?
1. **Separation of Concerns** - FluffOS enhancements separate from MUD libraries
2. **Reusability** - Same FluffOS fork usable across multiple MUD projects
3. **Maintainability** - Tools versioned with FluffOS code they support
4. **Contribution Safety** - Fork-specific tools can't accidentally reach upstream
5. **Professional Workflow** - Follows standard practices for dependency management

### Performance Considerations
- **Database connection pooling** reduces connection overhead in MUD environments
- **Async operations** prevent blocking during database queries
- **Optimized build flags** improve runtime performance for game servers
- **Memory management improvements** for long-running MUD processes

This FluffOS fork provides a complete development ecosystem for advanced MUD development while maintaining clean separation between fork-specific enhancements and upstream-compatible code.

---

*Enhanced FluffOS fork for MUD development*  
*Maintained by Shadyman*  
*Tools and documentation version: August 2025*
