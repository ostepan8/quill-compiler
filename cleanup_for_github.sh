#!/bin/bash

# Cleanup script for GitHub release
# Run this to clean up the project before pushing to GitHub

set -e
cd "$(dirname "$0")"

echo "Cleaning up Quill compiler project for GitHub..."

# Remove build artifacts
echo "Removing build artifacts..."
rm -rf build/
rm -f *.o *.a *.so *.dylib
rm -f *.ll *.bc *.s *.asm
rm -f hello math calculator
rm -f *_test fibonacci_* matrix_* prime_* monte_* bubble_* black_*

# Remove temporary files
echo "Removing temporary files..."
rm -f output_*.txt
rm -f *.tmp *.temp *.log *.out *.err
rm -f *~ *.bak *.orig *.swp *.swo *#
rm -rf .DS_Store .vscode/ .idea/
rm -rf .clangd/ .ccls-cache/ .cache/ .history/ .vs/ .settings/
rm -f *.code-workspace .project .cproject
rm -rf __pycache__/ *.py[cod] *$py.class

# Remove CMake generated files
echo "Removing CMake generated files..."
rm -f CMakeCache.txt cmake_install.cmake Makefile
rm -rf CMakeFiles/

# Remove debug and test scripts
echo "Removing debug scripts..."
rm -f debug*.sh test*.sh temp*.sh *_debug.sh *_test.sh

# Remove enhanced benchmark file if it exists
rm -f benchmark_results/benchmark_enhanced_20250907.md
rm -f run_updated_benchmarks.sh

# Clean up any test output files
rm -f examples/*.o examples/*.ll examples/*.s

echo "✅ Cleanup complete! Project is ready for GitHub."
echo ""
echo "Next steps:"
echo "1. git init (if not already done)"
echo "2. git add ."
echo "3. git commit -m 'Initial commit: Quill compiler with type-directed optimizations'"
echo "4. git remote add origin <your-github-repo-url>"
echo "5. git push -u origin main"