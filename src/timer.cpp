#include "timer.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <functional>

BenchmarkTimer::BenchmarkTimer(const std::string& name) : benchmark_name(name) {
    measurements.reserve(100); // Pre-allocate for efficiency
}

void BenchmarkTimer::start() {
    start_time = std::chrono::high_resolution_clock::now();
}

void BenchmarkTimer::stop() {
    end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    double ms = duration.count() / 1000000.0;
    measurements.push_back(ms);
}

void BenchmarkTimer::reset() {
    measurements.clear();
}

double BenchmarkTimer::get_last_measurement_ms() const {
    if (measurements.empty()) return 0.0;
    return measurements.back();
}

double BenchmarkTimer::get_average_ms() const {
    if (measurements.empty()) return 0.0;
    
    double sum = 0.0;
    for (double measurement : measurements) {
        sum += measurement;
    }
    return sum / measurements.size();
}

double BenchmarkTimer::get_min_ms() const {
    if (measurements.empty()) return 0.0;
    return *std::min_element(measurements.begin(), measurements.end());
}

double BenchmarkTimer::get_max_ms() const {
    if (measurements.empty()) return 0.0;
    return *std::max_element(measurements.begin(), measurements.end());
}

double BenchmarkTimer::get_stddev_ms() const {
    if (measurements.size() < 2) return 0.0;
    
    double avg = get_average_ms();
    double variance = 0.0;
    
    for (double measurement : measurements) {
        double diff = measurement - avg;
        variance += diff * diff;
    }
    
    variance /= (measurements.size() - 1);
    return std::sqrt(variance);
}

void BenchmarkTimer::run_benchmark(int iterations, std::function<void()> benchmark_func) {
    reset();
    
    // Warm-up run
    benchmark_func();
    
    // Actual measurements
    for (int i = 0; i < iterations; ++i) {
        start();
        benchmark_func();
        stop();
    }
}

void BenchmarkTimer::print_results() const {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Benchmark: " << benchmark_name << std::endl;
    std::cout << "Runs: " << measurements.size() << std::endl;
    std::cout << "Average: " << get_average_ms() << " ms" << std::endl;
    std::cout << "Min: " << get_min_ms() << " ms" << std::endl;
    std::cout << "Max: " << get_max_ms() << " ms" << std::endl;
    std::cout << "StdDev: " << get_stddev_ms() << " ms" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

void BenchmarkTimer::save_results_csv(const std::string& filename) const {
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) return;
    
    file << benchmark_name << "," 
         << measurements.size() << ","
         << get_average_ms() << ","
         << get_min_ms() << ","
         << get_max_ms() << ","
         << get_stddev_ms() << std::endl;
}