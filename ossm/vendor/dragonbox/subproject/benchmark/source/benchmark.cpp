// Copyright 2020-2021 Junekey Jeon
//
// The contents of this file may be used under the terms of
// the Apache License v2.0 with LLVM Exceptions.
//
//    (See accompanying file LICENSE-Apache or copy at
//     https://llvm.org/foundation/relicensing/LICENSE.txt)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

#include "benchmark.h"
#include "random_float.h"
#include "dragonbox/dragonbox_to_chars.h"
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>

template <class Float>
class benchmark_holder {
public:
    static constexpr auto max_digits = std::size_t(std::numeric_limits<Float>::max_digits10);

    static benchmark_holder& get_instance() {
        static benchmark_holder<Float> inst;
        return inst;
    }

    // Generate random samples
    void prepare_samples(std::size_t number_of_general_samples,
                         std::size_t number_of_digits_samples_per_digits) {
        samples_[0].resize(number_of_general_samples);
        for (auto& sample : samples_[0])
            sample = uniformly_randomly_generate_general_float<Float>(rg_);

        for (unsigned int digits = 1; digits <= max_digits; ++digits) {
            samples_[digits].resize(number_of_digits_samples_per_digits);
            for (auto& sample : samples_[digits])
                sample = randomly_generate_float_with_given_digits<Float>(digits, rg_);
        }
    }

    // { "name" : [(digits, [(sample, measured_time)])] }
    // Results for general samples is stored at the position digits=0
    using output_type =
        std::unordered_map<std::string,
                           std::array<std::vector<std::pair<Float, double>>, max_digits + 1>>;
    void run(std::size_t number_of_iterations, std::string_view float_name, output_type& out) {
        assert(number_of_iterations >= 1);
        char buffer[40];

        for (auto const& name_func_pair : name_func_pairs_) {
            auto [result_array_itr, is_inserted] = out.insert_or_assign(
                name_func_pair.first,
                std::array<std::vector<std::pair<Float, double>>, max_digits + 1>{});

            for (unsigned int digits = 0; digits <= max_digits; ++digits) {
                (*result_array_itr).second[digits].resize(samples_[digits].size());
                auto out_itr = (*result_array_itr).second[digits].begin();

                if (digits == 0) {
                    std::cout << "Benchmarking " << name_func_pair.first << " with uniformly random "
                              << float_name << "'s...\n";
                }
                else {
                    std::cout << "Benchmarking " << name_func_pair.first
                              << " with (approximately) uniformly random " << float_name << "'s of "
                              << digits << " digits...\n";
                }

                for (Float sample : samples_[digits]) {
                    auto from = std::chrono::high_resolution_clock::now();
                    for (std::size_t i = 0; i < number_of_iterations; ++i) {
                        name_func_pair.second(sample, buffer);
                    }
                    auto dur = std::chrono::high_resolution_clock::now() - from;

                    *out_itr = {
                        sample,
                        double(std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count()) /
                            double(number_of_iterations)};
                    ++out_itr;
                }
            }
        }
    }

    output_type run(std::size_t number_of_iterations, std::string_view float_name) {
        output_type out;
        run(number_of_iterations, float_name, out);
        return out;
    }

    void register_function(std::string_view name, void (*func)(Float, char*)) {
        name_func_pairs_.emplace(name, func);
    }

private:
    benchmark_holder() : rg_(generate_correctly_seeded_mt19937_64()) {}

    // Digits samples for [1] ~ [max_digits], general samples for [0]
    std::array<std::vector<Float>, max_digits + 1> samples_;
    std::mt19937_64 rg_;
    std::unordered_map<std::string, void (*)(Float, char*)> name_func_pairs_;
};

register_function_for_benchmark::register_function_for_benchmark(std::string_view name,
                                                                 void (*func_float)(float, char*)) {
    benchmark_holder<float>::get_instance().register_function(name, func_float);
};

register_function_for_benchmark::register_function_for_benchmark(std::string_view name,
                                                                 void (*func_double)(double, char*)) {
    benchmark_holder<double>::get_instance().register_function(name, func_double);
};

register_function_for_benchmark::register_function_for_benchmark(std::string_view name,
                                                                 void (*func_float)(float, char*),
                                                                 void (*func_double)(double, char*)) {
    benchmark_holder<float>::get_instance().register_function(name, func_float);
    benchmark_holder<double>::get_instance().register_function(name, func_double);
};


#define RUN_MATLAB
#ifdef RUN_MATLAB
    #include <cstdlib>

void run_matlab() {
    struct launcher {
        ~launcher() { std::system("matlab -nosplash -r \"cd('matlab'); plot_benchmarks\""); }
    };
    static launcher l;
}
#endif

template <class Float>
static void benchmark_test(std::string_view float_name, std::size_t number_of_uniform_samples,
                           std::size_t number_of_digits_samples_per_digits,
                           std::size_t number_of_iterations) {
    auto& inst = benchmark_holder<Float>::get_instance();
    std::cout << "Generating random samples...\n";
    inst.prepare_samples(number_of_uniform_samples, number_of_digits_samples_per_digits);
    auto out = inst.run(number_of_iterations, float_name);

    std::cout << "Benchmarking done.\n"
              << "Now writing to files...\n";

    // Write uniform benchmark results
    auto filename = std::string("results/uniform_benchmark_");
    filename += float_name;
    filename += ".csv";
    std::ofstream out_file{filename};
    out_file << "number_of_samples," << number_of_uniform_samples << std::endl;
    ;
    out_file << "name,sample,bit_representation,time\n";

    typename jkj::dragonbox::default_float_bit_carrier_conversion_traits<Float>::carrier_uint br;
    for (auto& name_result_pair : out) {
        for (auto const& data_time_pair : name_result_pair.second[0]) {
            std::memcpy(&br, &data_time_pair.first, sizeof(Float));
            out_file << "\"" << name_result_pair.first << "\","
                     << "0x" << std::hex << std::setfill('0');
            if constexpr (sizeof(Float) == 4)
                out_file << std::setw(8);
            else
                out_file << std::setw(16);
            out_file << br << std::dec << "," << data_time_pair.second << "\n";
        }
    }
    out_file.close();

    // Write digits benchmark results
    filename = std::string("results/digits_benchmark_");
    filename += float_name;
    filename += ".csv";
    out_file.open(filename);
    out_file << "number_of_samples_per_digits," << number_of_digits_samples_per_digits << std::endl;
    out_file << "name,digits,sample,time\n";

    for (auto& name_result_pair : out) {
        for (unsigned int digits = 1; digits <= benchmark_holder<Float>::max_digits; ++digits) {
            for (auto const& data_time_pair : name_result_pair.second[digits]) {
                std::memcpy(&br, &data_time_pair.first, sizeof(Float));
                out_file << "\"" << name_result_pair.first << "\"," << digits << ","
                         << "0x" << std::hex << std::setfill('0');
                if constexpr (sizeof(Float) == 4)
                    out_file << std::setw(8);
                else
                    out_file << std::setw(16);
                out_file << br << std::dec << "," << data_time_pair.second << "\n";
            }
        }
    }
    out_file.close();
}

int main() {
    constexpr bool benchmark_float = true;
    constexpr std::size_t number_of_uniform_benchmark_samples_float = 1000000;
    constexpr std::size_t number_of_digits_benchmark_samples_per_digits_float = 100000;
    constexpr std::size_t number_of_benchmark_iterations_float = 1000;

    constexpr bool benchmark_double = true;
    constexpr std::size_t number_of_uniform_benchmark_samples_double = 1000000;
    constexpr std::size_t number_of_digits_benchmark_samples_per_digits_double = 100000;
    constexpr std::size_t number_of_benchmark_iterations_double = 1000;

    if constexpr (benchmark_float) {
        std::cout << "[Running benchmark for binary32...]\n";
        benchmark_test<float>("binary32", number_of_uniform_benchmark_samples_float,
                              number_of_digits_benchmark_samples_per_digits_float,
                              number_of_benchmark_iterations_float);
        std::cout << "Done.\n\n\n";
    }
    if constexpr (benchmark_double) {
        std::cout << "[Running benchmark for binary64...]\n";
        benchmark_test<double>("binary64", number_of_uniform_benchmark_samples_double,
                               number_of_digits_benchmark_samples_per_digits_double,
                               number_of_benchmark_iterations_double);
        std::cout << "Done.\n\n\n";
    }

#ifdef RUN_MATLAB
    run_matlab();
#endif
}