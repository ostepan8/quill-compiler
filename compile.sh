#!/bin/bash

# Quill Compiler Script
# Usage: ./compile.sh program.quill [output_name] [optimization_level]

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Check if file argument provided
if [ $# -eq 0 ]; then
    echo -e "${RED}Usage: $0 <program.quill> [output_name] [optimization_level]${NC}"
    echo -e "${BLUE}Example: $0 examples/hello.quill${NC}"
    echo -e "${BLUE}Example: $0 examples/hello.quill my_program -O2${NC}"
    echo -e "${BLUE}Example: $0 examples/hello.quill my_program -O3${NC}"
    exit 1
fi

QUILL_FILE="$1"
OUTPUT_NAME="${2:-$(basename "$QUILL_FILE" .quill)}"
OPT_LEVEL="${3:--O1}"  # Default to -O1 optimization

# Check if quill file exists
if [ ! -f "$QUILL_FILE" ]; then
    echo -e "${RED}Error: File '$QUILL_FILE' not found${NC}"
    exit 1
fi

echo -e "${PURPLE}Compiling Quill program: $QUILL_FILE with $OPT_LEVEL${NC}"

# Build compiler if needed
if [ ! -f "build/quill" ]; then
    echo -e "${YELLOW}Building Quill compiler...${NC}"
    mkdir -p build
    cd build
    cmake .. && make
    cd ..
fi

# Build runtime if needed
if [ ! -f "runtime.o" ]; then
    echo -e "${YELLOW}Building runtime library...${NC}"
    gcc -c runtime.c -o runtime.o
fi

echo -e "${BLUE}Step 1: Compiling with optimization ($OPT_LEVEL)...${NC}"
./build/quill "$OPT_LEVEL" --timing "$QUILL_FILE"

echo -e "${BLUE}Step 2: Converting to assembly...${NC}"
/opt/homebrew/opt/llvm/bin/llc "${QUILL_FILE}.o" -o "${QUILL_FILE%.quill}.s"

echo -e "${BLUE}Step 3: Linking executable...${NC}"
gcc "${QUILL_FILE%.quill}.s" runtime.o -o "$OUTPUT_NAME"

echo -e "${GREEN}Successfully compiled '$QUILL_FILE' to '$OUTPUT_NAME' with $OPT_LEVEL${NC}"
echo -e "${BLUE}Run with: ./$OUTPUT_NAME${NC}"

# Show performance hint
case "$OPT_LEVEL" in
    "-O0")
        echo -e "${YELLOW}Try -O2 or -O3 for better performance${NC}"
        ;;
    "-O1")
        echo -e "${YELLOW}Try -O2 or -O3 for better performance${NC}"
        ;;
    "-O3")
        echo -e "${GREEN}Maximum optimization applied${NC}"
        ;;
esac