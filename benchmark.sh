#!/bin/bash

# Quill Compiler Performance Benchmark Suite
# Comprehensive benchmarking with comparison to Python and C++

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
ITERATIONS=10
WARMUP_RUNS=2
RESULTS_DIR="benchmark_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RESULTS_FILE="$RESULTS_DIR/benchmark_${TIMESTAMP}.md"

echo -e "${BLUE}Quill Compiler Performance Benchmark Suite${NC}"
echo -e "${BLUE}=============================================${NC}"

# Create results directory
mkdir -p "$RESULTS_DIR"

# Initialize results file
cat > "$RESULTS_FILE" << EOF
# Quill Compiler Performance Benchmark Results
**Generated:** $(date)
**Iterations per test:** $ITERATIONS
**Warmup runs:** $WARMUP_RUNS

## System Information
- **OS:** $(uname -s) $(uname -r)
- **CPU:** $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Unknown")
- **Compiler:** $(gcc --version | head -n1)
- **LLVM:** $(/opt/homebrew/opt/llvm/bin/llc --version | head -n1)

## Benchmark Results

EOF

# Build compiler if needed
if [ ! -f "build/quill" ]; then
    echo -e "${YELLOW}Building Quill compiler...${NC}"
    mkdir -p build
    cd build && cmake .. && make && cd ..
fi

# Build runtime if needed
if [ ! -f "runtime.o" ]; then
    echo -e "${YELLOW}Building runtime library...${NC}"
    gcc -c runtime.c -o runtime.o
fi

# Compile reference programs
echo -e "${YELLOW}Compiling reference programs...${NC}"
cd benchmarks/reference
g++ -O3 fibonacci_recursive.cpp -o fibonacci_recursive_cpp
cd ../..

# Function to run a benchmark and measure time
run_benchmark() {
    local program=$1
    local language=$2
    local optimization=${3:-""}
    
    echo "Running $program ($language$optimization)..." >&2
    
    local total_time=0
    local min_time=999999
    local max_time=0
    local times=()
    
    # Warmup runs
    for ((i=1; i<=WARMUP_RUNS; i++)); do
        if [ "$language" = "Quill" ]; then
            ./"$program" > /dev/null 2>&1
        elif [ "$language" = "Python" ]; then
            python3 benchmarks/reference/"$program".py > /dev/null 2>&1
        elif [ "$language" = "C++" ]; then
            ./benchmarks/reference/"$program"_cpp > /dev/null 2>&1
        fi
    done
    
    # Actual benchmark runs
    for ((i=1; i<=ITERATIONS; i++)); do
        local start_time=$(python3 -c "import time; print(time.time())")
        
        if [ "$language" = "Quill" ]; then
            ./"$program" > /dev/null 2>&1
        elif [ "$language" = "Python" ]; then
            python3 benchmarks/reference/"$program".py > /dev/null 2>&1
        elif [ "$language" = "C++" ]; then
            ./benchmarks/reference/"$program"_cpp > /dev/null 2>&1
        fi
        
        local end_time=$(python3 -c "import time; print(time.time())")
        local duration=$(python3 -c "print(($end_time - $start_time) * 1000)")
        
        times+=($duration)
        total_time=$(python3 -c "print($total_time + $duration)")
        
        # Track min/max
        if (( $(echo "$duration < $min_time" | bc -l) )); then
            min_time=$duration
        fi
        if (( $(echo "$duration > $max_time" | bc -l) )); then
            max_time=$duration
        fi
    done
    
    # Calculate average and standard deviation
    local avg_time=$(python3 -c "print($total_time / $ITERATIONS)")
    
    # Calculate standard deviation
    local variance=0
    for time in "${times[@]}"; do
        variance=$(python3 -c "print($variance + ($time - $avg_time) ** 2)")
    done
    variance=$(python3 -c "print($variance / ($ITERATIONS - 1))")
    local stddev=$(python3 -c "import math; print(math.sqrt($variance))")
    
    echo "$avg_time,$min_time,$max_time,$stddev"
}

# Function to compile Quill program
compile_quill() {
    local program=$1
    local optimization=${2:-""}
    
    echo -e "${YELLOW}Compiling $program.quill$optimization...${NC}"
    
    if [ -n "$optimization" ]; then
        # For now, use standard compilation (we'll add optimization flags later)
        ./build/quill "benchmarks/$program.quill" > /dev/null 2>&1
    else
        ./build/quill "benchmarks/$program.quill" > /dev/null 2>&1
    fi
    
    /opt/homebrew/opt/llvm/bin/llc "benchmarks/$program.quill.o" -o "benchmarks/$program.s"
    gcc "benchmarks/$program.s" runtime.o -o "$program"
}

# Benchmark programs
PROGRAMS=("fibonacci_recursive" "fibonacci_iterative" "matrix_multiply" "prime_sieve" "monte_carlo_pi" "bubble_sort" "black_scholes_simple")

echo -e "${GREEN}Starting benchmark suite...${NC}"

# Add results table header
cat >> "$RESULTS_FILE" << EOF
| Benchmark | Language | Opt Level | Avg (ms) | Min (ms) | Max (ms) | StdDev (ms) | Speedup vs Python |
|-----------|----------|-----------|----------|----------|----------|-------------|-------------------|
EOF

# Run benchmarks
for program in "${PROGRAMS[@]}"; do
    echo -e "\n${PURPLE}=== Benchmarking $program ===${NC}"
    
    # Run Python benchmark first (if reference exists) - this will be our baseline
    python_avg=0
    if [ -f "benchmarks/reference/$program.py" ]; then
        python_results=$(run_benchmark "$program" "Python")
        python_avg=$(echo "$python_results" | cut -d',' -f1)
        python_min=$(echo "$python_results" | cut -d',' -f2)
        python_max=$(echo "$python_results" | cut -d',' -f3)
        python_stddev=$(echo "$python_results" | cut -d',' -f4)
        
        # Add Python results to table
        printf "| %-20s | %-8s | %-9s | %8.2f | %8.2f | %8.2f | %11.2f | %17s |\n" \
            "$program" "Python" "-" "$python_avg" "$python_min" "$python_max" "$python_stddev" "-" >> "$RESULTS_FILE"
    fi
    
    # Run C++ benchmark (if reference exists)
    if [ -f "benchmarks/reference/${program}_cpp" ]; then
        cpp_results=$(run_benchmark "$program" "C++")
        cpp_avg=$(echo "$cpp_results" | cut -d',' -f1)
        cpp_min=$(echo "$cpp_results" | cut -d',' -f2)
        cpp_max=$(echo "$cpp_results" | cut -d',' -f3)
        cpp_stddev=$(echo "$cpp_results" | cut -d',' -f4)
        
        cpp_speedup="N/A"
        if [ "$python_avg" != "0" ]; then
            cpp_speedup=$(python3 -c "print(f'{float($python_avg) / float($cpp_avg):.2f}x')")
        fi
        
        # Add C++ results to table
        printf "| %-20s | %-8s | %-9s | %8.2f | %8.2f | %8.2f | %11.2f | %17s |\n" \
            "$program" "C++" "-O3" "$cpp_avg" "$cpp_min" "$cpp_max" "$cpp_stddev" "$cpp_speedup" >> "$RESULTS_FILE"
    fi
    
    # Test all Quill optimization levels
    for opt_level in "-O0" "-O1" "-O2" "-O3"; do
        echo -e "${YELLOW}Compiling $program.quill ($opt_level)...${NC}"
        
        # Compile with specific optimization level
        if timeout 30 ./build/quill "$opt_level" "benchmarks/$program.quill" > /dev/null 2>&1; then
            if /opt/homebrew/opt/llvm/bin/llc "benchmarks/$program.quill.o" -o "benchmarks/$program.s" > /dev/null 2>&1; then
                if gcc "benchmarks/$program.s" runtime.o -o "${program}_${opt_level}" > /dev/null 2>&1; then
                    
                    # Run Quill benchmark
                    quill_results=$(run_benchmark "${program}_${opt_level}" "Quill")
                    quill_avg=$(echo "$quill_results" | cut -d',' -f1)
                    quill_min=$(echo "$quill_results" | cut -d',' -f2)
                    quill_max=$(echo "$quill_results" | cut -d',' -f3)
                    quill_stddev=$(echo "$quill_results" | cut -d',' -f4)
                    
                    # Calculate speedup vs Python
                    python_speedup="N/A"
                    if [ "$python_avg" != "0" ]; then
                        python_speedup=$(python3 -c "print(f'{float($python_avg) / float($quill_avg):.2f}x')")
                    fi
                    
                    # Add Quill results to table
                    printf "| %-20s | %-8s | %-9s | %8.2f | %8.2f | %8.2f | %11.2f | %17s |\n" \
                        "$program" "Quill" "$opt_level" "$quill_avg" "$quill_min" "$quill_max" "$quill_stddev" "$python_speedup" >> "$RESULTS_FILE"
                    
                    # Clean up
                    rm -f "${program}_${opt_level}"
                else
                    echo -e "${RED}Failed to link $program with $opt_level${NC}"
                fi
            else
                echo -e "${RED}Failed to generate assembly for $program with $opt_level${NC}"
            fi
        else
            echo -e "${RED}Failed to compile $program with $opt_level${NC}"
        fi
        
        # Clean up intermediate files
        rm -f "benchmarks/$program.quill.o" "benchmarks/$program.s"
    done
    
    echo "" >> "$RESULTS_FILE"
done

# Add performance analysis
cat >> "$RESULTS_FILE" << EOF

## Performance Analysis

### Optimization Level Comparison
The benchmark tests Quill at all optimization levels:
- **-O0:** No optimization (baseline, fastest compilation)
- **-O1:** Basic optimizations (InstCombine, SimplifyCFG) 
- **-O2:** Standard optimizations (adds Reassociate, GVN)
- **-O3:** Maximum optimizations (all passes + aggressive opts)

### Language Comparison
- **Python:** Reference implementation baseline
- **C++:** Optimized with -O3 for performance comparison 
- **Quill:** All optimization levels for comprehensive analysis

### Key Metrics
- **Speedup vs Python:** Shows Quill's performance advantage
- **Optimization Impact:** Compare -O0 vs -O3 for optimization effectiveness
- **Standard Deviation:** Measurement consistency and reliability

### Notes
- Measurements include program startup and execution time
- Results may vary based on system load and CPU throttling
- Optimization passes currently disabled to prevent hangs (infrastructure ready)
- Performance differences will be visible once optimization passes are re-enabled

### Next Steps
1. Debug and re-enable LLVM optimization passes gradually
2. Implement custom Quill-specific optimization passes
3. Profile memory usage and cache behavior
4. Add vectorization hints for numerical computations

---
*Generated by Quill Compiler Benchmark Suite*
EOF

echo -e "\n${GREEN}Benchmark suite completed!${NC}"
echo -e "${BLUE}Results saved to: $RESULTS_FILE${NC}"
echo -e "${YELLOW}View results:${NC} cat $RESULTS_FILE"

# Clean up intermediate files
echo -e "\n${YELLOW}Cleaning up...${NC}"
rm -f benchmarks/*.s benchmarks/*.o
./clean.sh > /dev/null 2>&1