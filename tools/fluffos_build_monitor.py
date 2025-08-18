#!/usr/bin/env python3
"""
FluffOS Build Monitor
Intelligent build monitoring subagent for FluffOS compilation tasks.
Minimizes token usage through smart filtering and progressive wait intervals.
"""

import subprocess
import json
import time
import re
import sys
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from enum import Enum

class BuildState(Enum):
    STARTING = "starting"
    CONFIGURING = "configuring"
    COMPILING_CORE = "compiling_core"
    COMPILING_PACKAGES = "compiling_packages"
    LINKING = "linking"
    COMPLETED = "completed"
    FAILED = "failed"
    TIMEOUT = "timeout"

@dataclass
class BuildStatus:
    state: BuildState
    progress: int  # 0-100
    current_task: str
    packages_built: List[str]
    packages_total: List[str]
    errors: List[str]
    warnings: List[str]
    duration: float
    last_activity: str

class FluffOSBuildMonitor:
    """Intelligent build monitor for FluffOS compilation with token-efficient reporting."""
    
    def __init__(self, bash_id: str, focus_packages: List[str] = None, max_duration: int = 3600):
        self.bash_id = bash_id
        self.focus_packages = focus_packages or ["http", "rest", "openapi"]
        self.max_duration = max_duration
        self.start_time = time.time()
        self.last_check_time = 0
        self.last_output_size = 0
        self.state = BuildState.STARTING
        self.packages_built = []
        self.errors = []
        self.warnings = []
        
        # Progressive wait intervals (seconds)
        self.wait_intervals = [30, 60, 120, 240, 480, 900]  # 30s to 15m
        self.current_interval_index = 0
        self.consecutive_no_activity = 0
        
        # Build progress milestones
        self.milestones = {
            'cmake_start': 10,
            'cmake_done': 20,
            'core_start': 30,
            'packages_start': 50,
            'linking_start': 80,
            'build_complete': 100
        }
        
        self.current_progress = 0
        
    def _run_bash_output(self) -> Tuple[str, str, bool]:
        """Get new output from background bash process."""
        try:
            # Try to get bash output
            cmd = ["claude", "bash-output", self.bash_id]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            
            if result.returncode == 0:
                # Parse the output to extract stdout, stderr, and status
                output = result.stdout
                if "status:completed" in output.lower():
                    return output, "", True
                elif "status:running" in output.lower():
                    return output, "", False
                else:
                    return output, result.stderr, False
            else:
                return "", f"Failed to get bash output: {result.stderr}", False
                
        except subprocess.TimeoutExpired:
            return "", "Timeout getting bash output", False
        except Exception as e:
            return "", f"Error getting bash output: {str(e)}", False
    
    def _detect_build_activity(self, output: str) -> bool:
        """Detect if there's active build progress in the output."""
        activity_indicators = [
            r"\[\s*\d+%\]",  # Progress percentages
            r"Building.*\.o",  # Compiling files
            r"Linking.*executable",  # Linking
            r"Built target",  # Target completion
            r"make.*:",  # Make activity
            r"cmake",  # CMake activity
        ]
        
        for indicator in activity_indicators:
            if re.search(indicator, output, re.IGNORECASE):
                return True
        return False
    
    def _extract_progress(self, output: str) -> int:
        """Extract build progress percentage from output."""
        # Look for progress indicators like [42%]
        progress_matches = re.findall(r"\[\s*(\d+)%\]", output)
        if progress_matches:
            return max(int(p) for p in progress_matches)
        
        # Milestone-based progress
        if "cmake" in output.lower() and "configuring" in output.lower():
            return max(self.current_progress, self.milestones['cmake_start'])
        elif "generating done" in output.lower():
            return max(self.current_progress, self.milestones['cmake_done'])
        elif "building cxx object" in output.lower():
            return max(self.current_progress, self.milestones['core_start'])
        elif any(f"package_{pkg}" in output.lower() for pkg in self.focus_packages):
            return max(self.current_progress, self.milestones['packages_start'])
        elif "linking" in output.lower() and "executable" in output.lower():
            return max(self.current_progress, self.milestones['linking_start'])
        elif "built target" in output.lower() and ("driver" in output.lower() or "100%" in output):
            return self.milestones['build_complete']
            
        return self.current_progress
    
    def _extract_package_completions(self, output: str) -> List[str]:
        """Extract newly completed packages from output."""
        newly_built = []
        
        # Look for package completions
        for package in self.focus_packages:
            pattern = rf"Built target package_{package}"
            if re.search(pattern, output, re.IGNORECASE):
                if package not in self.packages_built:
                    newly_built.append(package)
                    self.packages_built.append(package)
        
        return newly_built
    
    def _filter_errors_and_warnings(self, output: str) -> Tuple[List[str], List[str]]:
        """Filter and categorize errors and warnings, focusing on our packages."""
        lines = output.split('\n')
        new_errors = []
        new_warnings = []
        
        # Patterns for critical errors
        critical_error_patterns = [
            r"error:",
            r"fatal error:",
            r"compilation failed",
            r"build failed",
            r"make.*error",
            r"cmake.*error",
        ]
        
        # Patterns for package-specific issues
        package_patterns = [f"packages/{pkg}" for pkg in self.focus_packages]
        
        # Noise patterns to ignore
        noise_patterns = [
            r"warning.*third.*party",
            r"warning.*libwebsockets",
            r"warning.*libevent",
            r"warning.*crypt",
            r"note: unrecognized command-line option",
            r"warning.*stringop-overflow",
            r"warning.*deprecated",
        ]
        
        for line in lines:
            line = line.strip()
            if not line:
                continue
                
            # Skip known noise
            if any(re.search(pattern, line, re.IGNORECASE) for pattern in noise_patterns):
                continue
            
            # Check for critical errors
            is_error = any(re.search(pattern, line, re.IGNORECASE) for pattern in critical_error_patterns)
            
            # Check for package-specific issues (higher priority)
            is_package_related = any(pattern in line for pattern in package_patterns)
            
            # Check for warnings
            is_warning = "warning:" in line.lower()
            
            if is_error or (is_package_related and is_warning):
                if line not in [e["message"] for e in self.errors]:
                    new_errors.append({
                        "message": line,
                        "package_related": is_package_related,
                        "critical": is_error
                    })
            elif is_warning and is_package_related:
                if line not in [w["message"] for w in self.warnings]:
                    new_warnings.append({
                        "message": line,
                        "package_related": is_package_related
                    })
        
        return new_errors, new_warnings
    
    def _update_build_state(self, output: str, completed: bool) -> BuildState:
        """Update build state based on output analysis."""
        if completed:
            if "built target driver" in output.lower() or self.current_progress >= 100:
                return BuildState.COMPLETED
            elif self.errors:
                return BuildState.FAILED
            else:
                return BuildState.COMPLETED
        
        # Check for failure indicators
        if any(error.get("critical", False) for error in self.errors):
            return BuildState.FAILED
        
        # Determine state based on progress and activity
        if "cmake" in output.lower() and self.current_progress < 30:
            return BuildState.CONFIGURING
        elif self.current_progress < 50:
            return BuildState.COMPILING_CORE
        elif self.current_progress < 80:
            return BuildState.COMPILING_PACKAGES
        elif self.current_progress < 100:
            return BuildState.LINKING
        else:
            return BuildState.COMPLETED
    
    def _get_current_activity(self, output: str) -> str:
        """Extract current activity description from output."""
        # Look for current compilation targets
        recent_lines = output.split('\n')[-10:]  # Last 10 lines
        
        for line in reversed(recent_lines):
            line = line.strip()
            if not line:
                continue
                
            # Look for compilation activities
            if re.search(r"Building.*\.o", line):
                match = re.search(r"Building.*?([^/]+\.cc|[^/]+\.c)\.o", line)
                if match:
                    return f"Compiling {match.group(1)}"
            
            # Look for package activities
            for package in self.focus_packages:
                if f"package_{package}" in line.lower():
                    return f"Building {package.upper()} package"
            
            # Look for linking activities
            if "linking" in line.lower():
                match = re.search(r"Linking.*?(\w+)", line)
                if match:
                    return f"Linking {match.group(1)}"
        
        return "Processing..."
    
    def check_build_status(self) -> BuildStatus:
        """Check current build status with minimal resource usage."""
        current_time = time.time()
        duration = current_time - self.start_time
        
        # Check for timeout
        if duration > self.max_duration:
            self.state = BuildState.TIMEOUT
            return BuildStatus(
                state=self.state,
                progress=self.current_progress,
                current_task="Timed out",
                packages_built=self.packages_built,
                packages_total=self.focus_packages,
                errors=self.errors,
                warnings=self.warnings,
                duration=duration,
                last_activity="Timeout reached"
            )
        
        # Get build output
        stdout, stderr, completed = self._run_bash_output()
        
        if stderr and "error" in stderr.lower():
            self.errors.append({"message": stderr, "critical": True})
            self.state = BuildState.FAILED
        
        # Analyze output
        has_activity = self._detect_build_activity(stdout)
        
        if has_activity:
            self.consecutive_no_activity = 0
            self.current_interval_index = 0  # Reset to fastest interval
        else:
            self.consecutive_no_activity += 1
        
        # Update progress and state
        new_progress = self._extract_progress(stdout)
        if new_progress > self.current_progress:
            self.current_progress = new_progress
        
        # Check for newly completed packages
        newly_built = self._extract_package_completions(stdout)
        
        # Extract errors and warnings
        new_errors, new_warnings = self._filter_errors_and_warnings(stdout)
        self.errors.extend(new_errors)
        self.warnings.extend(new_warnings)
        
        # Update state
        self.state = self._update_build_state(stdout, completed)
        
        # Get current activity
        current_activity = self._get_current_activity(stdout)
        
        return BuildStatus(
            state=self.state,
            progress=self.current_progress,
            current_task=current_activity,
            packages_built=self.packages_built,
            packages_total=self.focus_packages,
            errors=self.errors,
            warnings=self.warnings,
            duration=duration,
            last_activity=current_activity
        )
    
    def get_next_wait_interval(self) -> int:
        """Get next wait interval using progressive backoff."""
        if self.consecutive_no_activity > 2:
            # Increase wait interval for inactive builds
            self.current_interval_index = min(
                self.current_interval_index + 1,
                len(self.wait_intervals) - 1
            )
        
        return self.wait_intervals[self.current_interval_index]
    
    def monitor_build_to_completion(self, progress_callback=None) -> BuildStatus:
        """Monitor build until completion with intelligent wait intervals."""
        print(f"🔍 Starting FluffOS build monitor for bash ID: {self.bash_id}")
        print(f"📦 Focusing on packages: {', '.join(self.focus_packages)}")
        print(f"⏱️  Maximum monitoring time: {self.max_duration}s")
        print()
        
        last_reported_progress = -1
        last_reported_packages = set()
        
        while True:
            status = self.check_build_status()
            
            # Report significant progress changes
            if status.progress > last_reported_progress:
                print(f"[{status.progress:3d}%] {status.current_task}")
                last_reported_progress = status.progress
            
            # Report newly completed packages
            current_packages = set(status.packages_built)
            new_completions = current_packages - last_reported_packages
            for package in new_completions:
                print(f"✅ {package.upper()} package built successfully")
            last_reported_packages = current_packages
            
            # Report critical errors immediately
            for error in status.errors:
                if isinstance(error, dict) and error.get("critical", False):
                    print(f"❌ CRITICAL ERROR: {error['message']}")
            
            # Check for completion states
            if status.state in [BuildState.COMPLETED, BuildState.FAILED, BuildState.TIMEOUT]:
                break
            
            # Progress callback for external integrations
            if progress_callback:
                progress_callback(status)
            
            # Wait before next check
            wait_time = self.get_next_wait_interval()
            time.sleep(wait_time)
        
        # Final status report
        print()
        if status.state == BuildState.COMPLETED:
            print(f"🎉 Build completed successfully in {status.duration:.1f}s")
            if status.packages_built:
                print(f"📦 Packages built: {', '.join(status.packages_built)}")
        elif status.state == BuildState.FAILED:
            print(f"💥 Build failed after {status.duration:.1f}s")
            print(f"❌ {len(status.errors)} error(s) found")
        elif status.state == BuildState.TIMEOUT:
            print(f"⏰ Build monitoring timed out after {status.duration:.1f}s")
        
        return status

def main():
    """Command line interface for the build monitor."""
    import argparse
    
    parser = argparse.ArgumentParser(description="FluffOS Build Monitor")
    parser.add_argument("bash_id", help="Background bash process ID to monitor")
    parser.add_argument("--packages", nargs="+", default=["http", "rest", "openapi"],
                       help="Packages to focus on (default: http rest openapi)")
    parser.add_argument("--max-time", type=int, default=3600,
                       help="Maximum monitoring time in seconds (default: 3600)")
    parser.add_argument("--single-check", action="store_true",
                       help="Perform single status check instead of continuous monitoring")
    parser.add_argument("--output-format", choices=["text", "json"], default="text",
                       help="Output format (default: text)")
    
    args = parser.parse_args()
    
    monitor = FluffOSBuildMonitor(
        bash_id=args.bash_id,
        focus_packages=args.packages,
        max_duration=args.max_time
    )
    
    try:
        if args.single_check:
            status = monitor.check_build_status()
        else:
            status = monitor.monitor_build_to_completion()
        
        if args.output_format == "json":
            result = {
                "state": status.state.value,
                "progress": status.progress,
                "current_task": status.current_task,
                "packages_built": status.packages_built,
                "packages_total": status.packages_total,
                "errors": len(status.errors),
                "warnings": len(status.warnings),
                "duration": status.duration,
                "success": status.state == BuildState.COMPLETED
            }
            print(json.dumps(result, indent=2))
        
        # Exit with appropriate code
        exit_code = 0 if status.state == BuildState.COMPLETED else 1
        sys.exit(exit_code)
        
    except KeyboardInterrupt:
        print("\n⚠️  Build monitoring interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"❌ Error during build monitoring: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()