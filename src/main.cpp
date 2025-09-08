#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "optimization_passes.h"
#include "timer.h"
#include "type_checker.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct CompilerOptions {
    std::string input_file;
    std::string output_file;
    quill::QuillOptimizationManager::OptimizationLevel opt_level = quill::QuillOptimizationManager::O0;
    bool emit_llvm_ir = false;
    bool emit_assembly = false;
    bool show_optimization_report = false;
    bool show_timing = false;
    bool enable_type_checking = true;
    bool show_type_errors = true;
    bool help = false;
};

void print_usage(const char* program_name) {
    std::cout << "Quill Compiler - Python-inspired Language\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS] <source_file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -O0              No optimization (default)\n";
    std::cout << "  -O1              Basic optimizations\n";
    std::cout << "  -O2              More aggressive optimizations\n";
    std::cout << "  -O3              Maximum optimization\n";
    std::cout << "  -o <file>        Output file name\n";
    std::cout << "  --emit-llvm      Emit LLVM IR instead of object file\n";
    std::cout << "  --emit-asm       Emit assembly code\n";
    std::cout << "  --opt-report     Show optimization report\n";
    std::cout << "  --timing         Show compilation timing\n";
    std::cout << "  --no-typecheck   Disable type checking\n";
    std::cout << "  --type-errors    Show detailed type error information\n";
    std::cout << "  -h, --help       Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -O2 program.quill\n";
    std::cout << "  " << program_name << " -O3 --opt-report program.quill\n";
    std::cout << "  " << program_name << " --emit-llvm program.quill\n";
    std::cout << "  " << program_name << " --type-errors --timing program.quill\n";
}

CompilerOptions parse_arguments(int argc, char* argv[]) {
    CompilerOptions options;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            options.help = true;
        } else if (arg == "-O0") {
            options.opt_level = quill::QuillOptimizationManager::O0;
        } else if (arg == "-O1") {
            options.opt_level = quill::QuillOptimizationManager::O1;
        } else if (arg == "-O2") {
            options.opt_level = quill::QuillOptimizationManager::O2;
        } else if (arg == "-O3") {
            options.opt_level = quill::QuillOptimizationManager::O3;
        } else if (arg == "--emit-llvm") {
            options.emit_llvm_ir = true;
        } else if (arg == "--emit-asm") {
            options.emit_assembly = true;
        } else if (arg == "--opt-report") {
            options.show_optimization_report = true;
        } else if (arg == "--timing") {
            options.show_timing = true;
        } else if (arg == "--no-typecheck") {
            options.enable_type_checking = false;
        } else if (arg == "--type-errors") {
            options.show_type_errors = true;
        } else if (arg == "-o" && i + 1 < argc) {
            options.output_file = argv[++i];
        } else if (arg.front() != '-') {
            options.input_file = arg;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            options.help = true;
        }
    }
    
    return options;
}

int main(int argc, char* argv[]) {
    CompilerOptions options = parse_arguments(argc, argv);
    
    if (options.help || options.input_file.empty()) {
        print_usage(argv[0]);
        return options.help ? 0 : 1;
    }
    
    // Set default output file if not specified
    if (options.output_file.empty()) {
        options.output_file = options.input_file + ".o";
    }
    
    BenchmarkTimer total_timer("Total Compilation");
    total_timer.start();
    
    try {
        // Read source file
        std::ifstream file(options.input_file);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << options.input_file << std::endl;
            return 1;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();
        file.close();
        
        if (options.show_timing) {
            std::cout << "=== Quill Compiler Performance Analysis ===" << std::endl;
        }
        
        // Lexical analysis
        BenchmarkTimer lex_timer("Lexical Analysis");
        if (options.show_timing) lex_timer.start();
        
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        
        if (options.show_timing) {
            lex_timer.stop();
            std::cout << "Lexical Analysis: " << lex_timer.get_last_measurement_ms() 
                      << " ms (" << tokens.size() << " tokens)" << std::endl;
        }
        
        // Syntax analysis
        BenchmarkTimer parse_timer("Parsing");
        if (options.show_timing) parse_timer.start();
        
        Parser parser(std::move(tokens));
        auto program = parser.parse();
        
        if (options.show_timing) {
            parse_timer.stop();
            std::cout << "Parsing: " << parse_timer.get_last_measurement_ms() << " ms" << std::endl;
        }
        
        // Type checking (if enabled)
        if (options.enable_type_checking) {
            BenchmarkTimer typecheck_timer("Type Checking");
            if (options.show_timing) typecheck_timer.start();
            
            quill::TypeChecker type_checker;
            auto type_result = type_checker.checkProgram(program.get());
            
            if (options.show_timing) {
                typecheck_timer.stop();
                std::cout << "Type Checking: " << typecheck_timer.get_last_measurement_ms() << " ms" << std::endl;
            }
            
            // Report type checking results
            if (type_result.hasErrors() || !type_checker.getErrors().empty()) {
                if (options.show_type_errors) {
                    std::cout << "\nType Checking Results:" << std::endl;
                    const auto& errors = type_checker.getErrors();
                    for (const auto& error : errors) {
                        std::cout << "Error: " << error << std::endl;
                    }
                    
                    const auto& warnings = type_checker.getWarnings();
                    for (const auto& warning : warnings) {
                        std::cout << "Warning: " << warning << std::endl;
                    }
                }
            } else if (options.show_type_errors) {
                std::cout << "Type checking passed successfully" << std::endl;
            }
        }
        
        // Code generation
        BenchmarkTimer codegen_timer("Code Generation");
        if (options.show_timing) codegen_timer.start();
        
        CodeGen codegen;
        codegen.generate(*program);
        
        if (options.show_timing) {
            codegen_timer.stop();
            std::cout << "Code Generation: " << codegen_timer.get_last_measurement_ms() << " ms" << std::endl;
        }
        
        // Optimization
        BenchmarkTimer opt_timer("Optimization");
        if (options.show_timing) opt_timer.start();
        
        quill::QuillOptimizationManager optimizer(options.opt_level);
        if (options.opt_level != quill::QuillOptimizationManager::O0) {
            optimizer.runOptimizations(*codegen.module);
        }
        
        if (options.show_timing) {
            opt_timer.stop();
            std::cout << "Optimization: " << opt_timer.get_last_measurement_ms() << " ms" << std::endl;
        }
        
        // Show optimization report
        if (options.show_optimization_report) {
            optimizer.printOptimizationReport();
        }
        
        // Output generation
        if (options.emit_llvm_ir) {
            std::cout << "\n=== Generated LLVM IR ===" << std::endl;
            codegen.print_ir();
        } else {
            // Write object/assembly file
            codegen.write_object_file(options.output_file);
            
            if (!options.show_timing) {
                std::cout << "Successfully compiled '" << options.input_file 
                          << "' with -O" << (int)options.opt_level << std::endl;
                std::cout << "Output written to: " << options.output_file << std::endl;
            }
        }
        
        total_timer.stop();
        
        if (options.show_timing) {
            std::cout << "Total Compilation: " << total_timer.get_last_measurement_ms() << " ms" << std::endl;
            std::cout << "===========================================" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}