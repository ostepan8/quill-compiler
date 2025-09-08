# Contributing to Quill

Thank you for your interest in contributing to the Quill programming language compiler!

## Development Setup

### Prerequisites
- macOS with Homebrew (primary supported platform)
- LLVM 21.1.0+ via Homebrew: `brew install llvm`
- CMake 3.20+
- Modern C++ compiler with C++17 support

### Building
```bash
# Clone and build
git clone <repository-url>
cd quill-compiler
mkdir build && cd build
cmake .. && make -j4
```

### Testing
```bash
# Run examples
../compile.sh ../examples/hello.quill

# Run benchmarks
../benchmark.sh

# Test optimizations
./quill -O3 --opt-report ../examples/math.quill
```

## Code Style

- Use consistent indentation (4 spaces)
- Follow existing naming conventions
- Keep functions focused and well-documented
- Remove debug comments and placeholder code
- No emoji in source code or commit messages

## Architecture

The compiler pipeline:
1. **Lexer** (`src/lexer.cpp`) - Tokenization
2. **Parser** (`src/parser.cpp`) - AST generation  
3. **Type Checker** (`types/`) - Static analysis and type inference
4. **Code Generator** (`src/codegen.cpp`) - LLVM IR generation
5. **Optimizer** (`optimization/`) - Custom optimization passes
6. **LLVM Backend** - Assembly and linking

## Areas for Contribution

### High Priority
- Language features (loops, arrays, strings)
- Error reporting improvements
- Performance optimizations
- Documentation and examples

### Medium Priority  
- IDE integration (LSP)
- Additional optimization passes
- Memory management features
- Module system

### Research Areas
- Advanced type system features
- Compile-time evaluation
- GPU/SIMD code generation

## Pull Request Guidelines

1. **Focus**: One feature or fix per PR
2. **Testing**: Include tests for new features
3. **Documentation**: Update README and docs as needed
4. **Performance**: Run benchmarks for optimization changes
5. **Code Quality**: Clean, readable code with clear intent

## Optimization Development

When adding optimization passes:

1. Extend `optimization/` directory with new pass
2. Register pass in `optimization_manager.cpp`
3. Add statistics collection if applicable
4. Update optimization reporting
5. Benchmark performance impact

## Type System Development

The type system supports:
- Static type checking with inference
- Generic types and constraints  
- Union and discriminated union types
- Flow-sensitive analysis

See `types/` directory for implementation details.

## Reporting Issues

- Use GitHub issues for bugs and feature requests
- Include minimal reproduction cases
- Specify platform and LLVM version
- Provide compiler output and error messages

## Getting Help

- Check existing issues and discussions
- Review architecture in README.md
- Examine existing code for patterns
- Test changes with benchmark suite

## License

By contributing, you agree that your contributions will be licensed under the MIT License.