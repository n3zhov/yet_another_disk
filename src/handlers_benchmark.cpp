//
// Created by n3zhov on 17.12.22.
//

#include "handlers.hpp"

#include <cstdint>   // for std::uint64_t
#include <iterator>  // for std::size
#include <string_view>

#include <benchmark/benchmark.h>
#include <userver/engine/run_standalone.hpp>

void HandlersBenchmark(benchmark::State& state) {
}

BENCHMARK(HandlersBenchmark);