#pragma once
#include <chrono>
#include <vector>
#include <string>
#include <functional>

class BenchmarkTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;
    std::vector<double> measurements;
    std::string benchmark_name;
    
public:
    BenchmarkTimer(const std::string& name);
    
    void start();
    void stop();
    void reset();
    
    double get_last_measurement_ms() const;
    double get_average_ms() const;
    double get_min_ms() const;
    double get_max_ms() const;
    double get_stddev_ms() const;
    
    void run_benchmark(int iterations, std::function<void()> benchmark_func);
    void print_results() const;
    void save_results_csv(const std::string& filename) const;
};