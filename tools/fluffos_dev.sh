#!/bin/bash

# FluffOS development workflow helper
# This script can be used in any project that includes FluffOS as a submodule
# Version: 1.0 for Shadyman's FluffOS fork
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FLUFFOS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Detect if we're in a submodule or main repository
if [ -f "$FLUFFOS_DIR/.git" ] && grep -q "gitdir:" "$FLUFFOS_DIR/.git" 2>/dev/null; then
    # We're in a submodule
    PARENT_PROJECT=$(cd "$FLUFFOS_DIR/.." && basename "$(pwd)")
    echo "FluffOS Development Helper (in $PARENT_PROJECT project)"
else
    # We're in the main FluffOS repository
    echo "FluffOS Development Helper (main repository)"
fi

# Default main branch - can be overridden by environment variable
MAIN_BRANCH="${FLUFFOS_MAIN_BRANCH:-crypto-modern-hash-algorithms}"

# Function to create a feature branch
create_feature_branch() {
    local branch_name="$1"
    if [ -z "$branch_name" ]; then
        echo "Usage: $0 branch <branch-name>"
        exit 1
    fi
    
    cd "$FLUFFOS_DIR"
    git checkout "$MAIN_BRANCH"
    git pull origin "$MAIN_BRANCH"
    git checkout -b "$branch_name"
    echo "✓ Created and switched to branch '$branch_name'"
}

# Function to commit and push changes
commit_changes() {
    local message="$1"
    if [ -z "$message" ]; then
        echo "Usage: $0 commit '<commit-message>'"
        exit 1
    fi
    
    cd "$FLUFFOS_DIR"
    git add .
    git commit -m "$message"
    
    local current_branch=$(git branch --show-current)
    git push origin "$current_branch"
    echo "✓ Committed and pushed changes to branch '$current_branch'"
}

# Function to show current status
status() {
    cd "$FLUFFOS_DIR"
    echo "=== FluffOS Development Status ==="
    echo "Current branch: $(git branch --show-current)"
    echo "Main branch: $MAIN_BRANCH"
    echo "Working directory status:"
    git status --short
    echo ""
    echo "Recent commits:"
    git log --oneline -5
    echo ""
    echo "Remotes:"
    git remote -v
}

# Function to sync with your fork's main branch
sync_fork() {
    cd "$FLUFFOS_DIR"
    
    echo "Fetching from your fork..."
    git fetch origin
    
    local current_branch=$(git branch --show-current)
    
    if [ "$current_branch" = "$MAIN_BRANCH" ]; then
        echo "Merging latest changes from origin..."
        git merge "origin/$MAIN_BRANCH"
        echo "✓ Synced with origin/$MAIN_BRANCH"
    else
        echo "Currently on branch '$current_branch'"
        echo "To sync with origin, switch to $MAIN_BRANCH first:"
        echo "  git checkout $MAIN_BRANCH"
        echo "  git pull origin $MAIN_BRANCH"
    fi
}

# Function to sync with upstream FluffOS repository
sync_upstream() {
    cd "$FLUFFOS_DIR"
    
    # Check if upstream remote exists
    if ! git remote | grep -q "upstream"; then
        echo "No upstream remote found. Adding upstream..."
        git remote add upstream https://github.com/fluffos/fluffos.git
    fi
    
    echo "Fetching from upstream FluffOS..."
    git fetch upstream
    
    local current_branch=$(git branch --show-current)
    
    echo "Available upstream branches:"
    git branch -r | grep upstream/ | head -5
    echo ""
    echo "To merge upstream changes:"
    echo "  git merge upstream/master  (for latest stable)"
    echo "  git merge upstream/v2019   (for v2019 branch)"
    echo ""
    echo "After merging, push to your fork:"
    echo "  git push origin $current_branch"
}

# Function to show branch information
branch_info() {
    cd "$FLUFFOS_DIR"
    local current_branch=$(git branch --show-current)
    
    echo "=== Branch Information ==="
    echo "Current branch: $current_branch"
    echo "Main branch: $MAIN_BRANCH"
    echo "Your fork URL: $(git remote get-url origin 2>/dev/null || echo 'No origin remote')"
    if git remote get-url origin >/dev/null 2>&1; then
        local origin_url=$(git remote get-url origin)
        echo "Branch URL: ${origin_url%.git}/tree/$current_branch"
    fi
    echo ""
    echo "Local branches:"
    git branch
    echo ""
    echo "Recent commits on this branch:"
    git log --oneline -10
}

# Function to build FluffOS with intelligent monitoring
build_fluffos() {
    cd "$FLUFFOS_DIR"
    
    echo "Building FluffOS from scratch..."
    mkdir -p build
    cd build
    
    # Clean build directory
    echo "Cleaning build directory..."
    rm -rf ./*
    
    # Use cmake if available, otherwise try configure
    if command -v cmake >/dev/null 2>&1; then
        echo "Running cmake..."
        cmake ..
        
        echo "Running make with intelligent monitoring..."
        if [ "$1" = "fast" ]; then
            echo "Using parallel compilation (-j4) with build monitor..."
            nohup make -j4 > build.log 2>&1 &
            BUILD_PID=$!
        elif [ "$1" = "monitor" ]; then
            echo "Using parallel compilation with detailed monitoring..."
            nohup make -j4 > build.log 2>&1 &
            BUILD_PID=$!
        else
            echo "Using sequential compilation (easier error parsing)..."
            nohup make > build.log 2>&1 &
            BUILD_PID=$!
        fi
        
        # Use intelligent build monitor if available
        if [ -f "$FLUFFOS_DIR/tools/fluffos_build_monitor.py" ] && [ "$1" != "silent" ]; then
            echo "Starting intelligent build monitor..."
            python3 "$FLUFFOS_DIR/tools/fluffos_build_monitor.py" "$BUILD_PID" --packages http rest openapi
            BUILD_RESULT=$?
        else
            echo "Monitoring build progress (PID: $BUILD_PID)..."
            wait $BUILD_PID
            BUILD_RESULT=$?
        fi
        
        # Check build result
        if [ $BUILD_RESULT -eq 0 ]; then
            echo "✓ FluffOS built successfully"
            echo "Binary location: $FLUFFOS_DIR/build/src/driver"
        else
            echo "✗ FluffOS build failed"
            echo "Build log available at: $FLUFFOS_DIR/build/build.log"
            echo "Last 20 lines of build log:"
            tail -20 build.log
            exit 1
        fi
    else
        echo "CMake not found. Please install cmake to build FluffOS."
        exit 1
    fi
}

# Function to test compile individual packages
test_package() {
    local package_name="$1"
    if [ -z "$package_name" ]; then
        echo "Usage: $0 test-package <package-name>"
        echo "Example: $0 test-package http"
        echo "Available packages can be found in /src/packages/"
        exit 1
    fi
    
    cd "$FLUFFOS_DIR/build"
    
    echo "Testing compilation of package: $package_name"
    echo "Running: make package_${package_name}/fast"
    
    if make "package_${package_name}/fast"; then
        echo "✓ Package '$package_name' compiled successfully"
    else
        echo "✗ Package '$package_name' compilation failed"
        exit 1
    fi
}

# Function to update documentation after successful compilation
update_docs() {
    cd "$FLUFFOS_DIR/build"
    
    echo "Updating FluffOS documentation..."
    echo ""
    echo "🤖 REMINDER: For complex documentation tasks like creating guidelines,"
    echo "    formatting standards, or comprehensive documentation reviews,"
    echo "    consider using Claude Code subagents with the Task tool."
    echo "    This ensures consistent quality and leverages specialized expertise."
    echo ""
    
    # Check if generate_keywords exists
    if [ ! -f "src/generate_keywords" ]; then
        echo "✗ generate_keywords not found. Please ensure FluffOS was compiled successfully."
        exit 1
    fi
    
    # Generate keywords.json
    echo "Step 1: Generating keywords.json..."
    src/generate_keywords
    
    if [ ! -f "src/keywords.json" ]; then
        echo "✗ Failed to generate keywords.json"
        exit 1
    fi
    
    # Copy keywords.json to docs directory
    echo "Step 2: Copying keywords.json to docs directory..."
    cp src/keywords.json "$FLUFFOS_DIR/docs/"
    
    # Change to docs directory for remaining operations
    cd "$FLUFFOS_DIR/docs"
    
    # Run add_missing_efuns.py to generate missing EFUN documentation
    echo "Step 3: Running add_missing_efuns.py to generate missing EFUN documentation..."
    if [ -f "add_missing_efuns.py" ]; then
        python3 add_missing_efuns.py
        echo "✓ Missing EFUN documentation generated at: $FLUFFOS_DIR/docs/efun/general/"
    else
        echo "✗ add_missing_efuns.py not found in docs directory"
        exit 1
    fi
    
    # Generate AI-powered descriptions for TBW placeholders
    echo "Step 4: Running generate_efun_descriptions.py to create AI-powered descriptions..."
    echo "         💡 TIP: For complex documentation tasks, use Claude Code subagents"
    if [ -f "generate_efun_descriptions.py" ]; then
        python3 generate_efun_descriptions.py
        echo "✓ AI-generated descriptions created for EFUN documentation"
    else
        echo "⚠ generate_efun_descriptions.py not found, skipping AI description generation"
    fi
    
    # Run fix_md_header.py to add titles to markdown files
    echo "Step 5: Running fix_md_header.py to add titles to markdown files..."
    if [ -f "fix_md_header.py" ]; then
        python3 fix_md_header.py
        echo "✓ Markdown headers fixed"
    else
        echo "⚠ fix_md_header.py not found, skipping header fixes"
    fi
    
    # Run update_index.sh to generate index files
    echo "Step 6: Running update_index.sh to generate index files..."
    if [ -f "update_index.sh" ]; then
        chmod +x update_index.sh
        ./update_index.sh
        echo "✓ Index files generated"
    else
        echo "⚠ update_index.sh not found, skipping index generation"
    fi
    
    echo "✓ Documentation update completed successfully!"
    echo "Documentation available at: $FLUFFOS_DIR/docs/"
}

# Function to build with monitoring (new command)
monitor_build() {
    echo "=== FluffOS Build with Detailed Monitoring ==="
    build_fluffos "monitor"
}

# Function to do a full build and update cycle
full_build() {
    echo "=== Full FluffOS Build and Update Cycle ==="
    
    # Build FluffOS
    build_fluffos "$1"
    
    # Update documentation
    echo ""
    echo "=== Updating Documentation ==="
    update_docs
    
    echo ""
    echo "✓ Full build and update completed successfully!"
}

# Function to set main branch
set_main_branch() {
    local new_main="$1"
    if [ -z "$new_main" ]; then
        echo "Current main branch: $MAIN_BRANCH"
        echo "Usage: $0 main <branch-name>"
        echo "Example: $0 main master"
        return
    fi
    
    cd "$FLUFFOS_DIR"
    
    # Check if branch exists
    if git branch -a | grep -q "$new_main"; then
        export FLUFFOS_MAIN_BRANCH="$new_main"
        echo "✓ Main branch set to: $new_main"
        echo "Note: This is temporary. Set FLUFFOS_MAIN_BRANCH environment variable to make it permanent."
    else
        echo "Branch '$new_main' not found. Available branches:"
        git branch -a
    fi
}

# Function to create clean upstream contribution branch
create_upstream_branch() {
    local branch_name="$1"
    if [ -z "$branch_name" ]; then
        echo "Usage: $0 upstream-branch <branch-name>"
        echo "Creates a clean branch from upstream/master for contributing back"
        exit 1
    fi
    
    cd "$FLUFFOS_DIR"
    
    # Ensure upstream remote exists
    if ! git remote | grep -q "upstream"; then
        echo "Adding upstream remote..."
        git remote add upstream https://github.com/fluffos/fluffos.git
    fi
    
    # Fetch latest upstream
    echo "Fetching from upstream..."
    git fetch upstream
    
    # Create branch from upstream/master
    git checkout -b "$branch_name" upstream/master
    
    echo "✓ Created clean upstream contribution branch: $branch_name"
    echo "This branch is based on upstream/master and excludes fork-specific files."
    echo ""
    echo "When ready to contribute:"
    echo "  git push origin $branch_name"
    echo "  # Then create PR from your fork to fluffos/fluffos"
}

# Main command handling
case "$1" in
    "branch")
        create_feature_branch "$2"
        ;;
    "commit")
        commit_changes "$2"
        ;;
    "status")
        status
        ;;
    "sync")
        sync_fork
        ;;
    "upstream")
        sync_upstream
        ;;
    "info")
        branch_info
        ;;
    "build")
        build_fluffos "$2"
        ;;
    "monitor-build")
        monitor_build
        ;;
    "test-package")
        test_package "$2"
        ;;
    "update-docs")
        update_docs
        ;;
    "full-build")
        full_build "$2"
        ;;
    "main")
        set_main_branch "$2"
        ;;
    "upstream-branch")
        create_upstream_branch "$2"
        ;;
    *)
        echo "FluffOS Development Helper"
        echo ""
        echo "Usage: $0 <command> [arguments]"
        echo ""
        echo "Git Commands:"
        echo "  branch <name>     Create and switch to a new feature branch"
        echo "  commit '<msg>'    Commit and push current changes to your fork"
        echo "  status           Show current git status and recent commits"
        echo "  sync             Sync current branch with your fork"
        echo "  upstream         Fetch and show upstream merge options"
        echo "  info             Show detailed branch and repository information"
        echo "  main [branch]    Set/show main branch (default: $MAIN_BRANCH)"
        echo "  upstream-branch <name>  Create clean branch from upstream for contributions"
        echo ""
        echo "Build Commands:"
        echo "  build [fast|monitor|silent]  Build FluffOS from scratch with options:"
        echo "                               fast: parallel build (-j4)"
        echo "                               monitor: parallel build with detailed monitoring"
        echo "                               silent: no monitoring, just build"
        echo "                               (default): sequential build with smart monitoring"
        echo "  monitor-build    Build with detailed monitoring (same as 'build monitor')"
        echo "  test-package <name>  Test compile individual package (e.g., http, rest, openapi)"
        echo "  update-docs      Complete documentation update (keywords → EFUN docs → AI descriptions → headers → indexes)"
        echo "  full-build [fast]    Complete build and documentation update cycle"
        echo ""
        echo "Examples:"
        echo "  $0 branch my-new-feature"
        echo "  $0 commit 'Add new feature'"
        echo "  $0 status"
        echo "  $0 sync"
        echo "  $0 upstream"
        echo "  $0 build              # Sequential build with smart monitoring"
        echo "  $0 build fast         # Parallel build (-j4) with monitoring"
        echo "  $0 build monitor      # Parallel build with detailed monitoring"
        echo "  $0 build silent       # Build without monitoring output"
        echo "  $0 monitor-build      # Same as 'build monitor'"
        echo "  $0 test-package http  # Test HTTP package compilation"
        echo "  $0 update-docs        # Complete documentation update (6-step process with AI descriptions)"
        echo "  $0 full-build         # Complete build and update cycle"
        echo "  $0 main master"
        echo "  $0 upstream-branch contrib-fix-database"
        echo ""
        echo "Environment Variables:"
        echo "  FLUFFOS_MAIN_BRANCH    Set default main branch (current: $MAIN_BRANCH)"
        echo ""
        if git remote get-url origin >/dev/null 2>&1; then
            echo "Your fork: $(git remote get-url origin)"
        fi
        echo "Upstream:  https://github.com/fluffos/fluffos"
        echo ""
        echo "💡 TIP: For complex tasks like documentation creation, code analysis, or formatting"
        echo "    standards, consider using Claude Code subagents with the Task tool. This"
        echo "    ensures consistent quality and leverages specialized expertise for different"
        echo "    types of tasks (e.g., general-purpose, statusline-setup, output-style-setup)."
        ;;
esac
