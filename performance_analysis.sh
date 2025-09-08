#!/bin/bash

# Quill Compiler Performance Analysis Tool
# Comprehensive optimization level comparison

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

BENCHMARK_FILE="${1:-benchmarks/fibonacci_recursive.quill}"
RESULTS_DIR="performance_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
REPORT_FILE="$RESULTS_DIR/performance_analysis_${TIMESTAMP}.md"

echo -e "${PURPLE}🔬 Quill Compiler Optimization Analysis${NC}"
echo -e "${PURPLE}=====================================${NC}"

# Create results directory
mkdir -p "$RESULTS_DIR"

# Build compiler if needed
if [ ! -f "build/quill" ]; then
    echo -e "${YELLOW}Building Quill compiler...${NC}"
    cd build && make && cd ..
fi

# Build runtime if needed
if [ ! -f "runtime.o" ]; then
    gcc -c runtime.c -o runtime.o
fi

# Initialize report
cat > "$REPORT_FILE" << EOF
# Quill Compiler Optimization Analysis Report
**Generated:** $(date)
**Test Program:** $BENCHMARK_FILE
**System:** $(uname -s) $(uname -r)
**CPU:** $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Unknown")

## Compilation Performance Comparison

EOF

echo -e "${CYAN}Testing compilation performance across optimization levels...${NC}"

# Test each optimization level
for OPT_LEVEL in "-O0" "-O1" "-O2" "-O3"; do
    echo -e "\n${BLUE}🧪 Testing $OPT_LEVEL optimization...${NC}"
    
    # Compile with timing
    echo -e "### Optimization Level: $OPT_LEVEL" >> "$REPORT_FILE"
    echo -e "\`\`\`" >> "$REPORT_FILE"
    
    # Run compilation and capture timing
    ./build/quill $OPT_LEVEL --timing --opt-report "$BENCHMARK_FILE" 2>&1 >> "$REPORT_FILE" || true
    
    echo -e "\`\`\`" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    
    # Generate assembly and check size
    if [ -f "${BENCHMARK_FILE}.o" ]; then
        /opt/homebrew/opt/llvm/bin/llc "${BENCHMARK_FILE}.o" -o "${BENCHMARK_FILE%.quill}_${OPT_LEVEL}.s" 2>/dev/null || true
        
        if [ -f "${BENCHMARK_FILE%.quill}_${OPT_LEVEL}.s" ]; then
            ASM_LINES=$(wc -l < "${BENCHMARK_FILE%.quill}_${OPT_LEVEL}.s")
            ASM_SIZE=$(wc -c < "${BENCHMARK_FILE%.quill}_${OPT_LEVEL}.s")
            echo "- **Assembly Lines:** $ASM_LINES" >> "$REPORT_FILE"
            echo "- **Assembly Size:** $ASM_SIZE bytes" >> "$REPORT_FILE"
            echo "" >> "$REPORT_FILE"
        fi
        
        # Create executable for runtime testing
        if [ -f "${BENCHMARK_FILE%.quill}_${OPT_LEVEL}.s" ]; then
            gcc "${BENCHMARK_FILE%.quill}_${OPT_LEVEL}.s" runtime.o -o "test_${OPT_LEVEL#-}" 2>/dev/null || true
        fi
    fi
done

# Runtime Performance Testing
echo -e "\n${CYAN}Testing runtime performance...${NC}"

cat >> "$REPORT_FILE" << EOF
## Runtime Performance Comparison

| Optimization | Execution Time (avg) | Speedup vs O0 | Executable Size |
|--------------|----------------------|---------------|-----------------|
EOF

declare -A execution_times
baseline_time=""

# Test execution performance
for OPT_LEVEL in "O0" "O1" "O2" "O3"; do
    if [ -f "test_${OPT_LEVEL}" ]; then
        echo -e "${BLUE}⏱️  Measuring runtime for -$OPT_LEVEL...${NC}"
        
        # Run multiple times and average
        total_time=0
        runs=5
        
        for ((i=1; i<=runs; i++)); do
            start_time=$(python3 -c "import time; print(time.time())")
            timeout 30s "./test_${OPT_LEVEL}" > /dev/null 2>&1 || true
            end_time=$(python3 -c "import time; print(time.time())")
            
            duration=$(python3 -c "print(($end_time - $start_time) * 1000)")
            total_time=$(python3 -c "print($total_time + $duration)")
        done
        
        avg_time=$(python3 -c "print(round($total_time / $runs, 2))")
        execution_times[$OPT_LEVEL]=$avg_time
        
        # Set baseline for O0
        if [ "$OPT_LEVEL" = "O0" ]; then
            baseline_time=$avg_time
        fi
        
        # Calculate speedup
        if [ -n "$baseline_time" ] && [ "$OPT_LEVEL" != "O0" ]; then
            speedup=$(python3 -c "print(f'{$baseline_time / $avg_time:.2f}x')")
        else
            speedup="1.00x (baseline)"
        fi
        
        # Get executable size
        exe_size=$(ls -la "test_${OPT_LEVEL}" | awk '{print $5}')
        
        # Add to report
        printf "| %-12s | %20s ms | %13s | %15s |\n" "-$OPT_LEVEL" "$avg_time" "$speedup" "$exe_size bytes" >> "$REPORT_FILE"
    fi
done

# Add LLVM IR comparison
echo -e "\n${CYAN}Generating LLVM IR comparison...${NC}"

cat >> "$REPORT_FILE" << EOF

## LLVM IR Analysis

### Unoptimized (-O0)
\`\`\`llvm
EOF

./build/quill -O0 --emit-llvm "$BENCHMARK_FILE" 2>/dev/null >> "$REPORT_FILE" || true

cat >> "$REPORT_FILE" << EOF
\`\`\`

### Fully Optimized (-O3)
\`\`\`llvm
EOF

./build/quill -O3 --emit-llvm "$BENCHMARK_FILE" 2>/dev/null >> "$REPORT_FILE" || true

cat >> "$REPORT_FILE" << EOF
\`\`\`

## Analysis Summary

### Key Findings
- **Best Performance:** $(python3 -c "
import sys
times = {$( for opt in "${!execution_times[@]}"; do echo "'$opt': ${execution_times[$opt]}, "; done )}
if times:
    best = min(times.keys(), key=lambda k: times[k])
    print(f'-{best} ({times[best]} ms)')
else:
    print('N/A')
")
- **Compilation Speed:** Optimization levels increase compilation time but improve runtime performance
- **Code Size:** Higher optimization levels may increase or decrease code size depending on inlining decisions

### Optimization Effectiveness
1. **-O1:** Basic optimizations with minimal compilation overhead
2. **-O2:** Balanced optimization for most use cases
3. **-O3:** Maximum optimization for performance-critical applications

### Recommendations
- Use **-O2** for development builds (good balance of speed and performance)
- Use **-O3** for production releases (maximum runtime performance)
- Use **-O0** only for debugging (fastest compilation, no optimization)

---
*Generated by Quill Compiler Performance Analysis Tool*
EOF

# Cleanup
rm -f test_O* *.s benchmarks/*.s benchmarks/*.o 2>/dev/null || true

echo -e "\n${GREEN}📊 Performance analysis completed!${NC}"
echo -e "${BLUE}Report saved to: $REPORT_FILE${NC}"
echo -e "${YELLOW}View report: cat $REPORT_FILE${NC}"