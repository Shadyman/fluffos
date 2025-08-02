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

# Function to build FluffOS
build_fluffos() {
    cd "$FLUFFOS_DIR"
    
    echo "Building FluffOS..."
    mkdir -p build
    cd build
    
    # Use cmake if available, otherwise try configure
    if command -v cmake >/dev/null 2>&1; then
        cmake ..
        make -j$(nproc 2>/dev/null || echo 4)
    else
        echo "CMake not found. Please install cmake to build FluffOS."
        exit 1
    fi
    
    echo "✓ FluffOS built successfully"
    echo "Binary location: $FLUFFOS_DIR/build/driver"
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
        build_fluffos
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
        echo "Commands:"
        echo "  branch <name>     Create and switch to a new feature branch"
        echo "  commit '<msg>'    Commit and push current changes to your fork"
        echo "  status           Show current git status and recent commits"
        echo "  sync             Sync current branch with your fork"
        echo "  upstream         Fetch and show upstream merge options"
        echo "  info             Show detailed branch and repository information"
        echo "  build            Build FluffOS using cmake"
        echo "  main [branch]    Set/show main branch (default: $MAIN_BRANCH)"
        echo "  upstream-branch <name>  Create clean branch from upstream for contributions"
        echo ""
        echo "Examples:"
        echo "  $0 branch my-new-feature"
        echo "  $0 commit 'Add new feature'"
        echo "  $0 status"
        echo "  $0 sync"
        echo "  $0 upstream"
        echo "  $0 build"
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
        ;;
esac
