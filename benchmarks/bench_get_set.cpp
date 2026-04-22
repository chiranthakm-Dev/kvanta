#include <benchmark/benchmark.h>
#include "rcu_store.h"

static void BM_Get(benchmark::State& state) {
    RcuStore<std::string, std::string> store;
    for (int i = 0; i < 1000; ++i) {
        store.set("key" + std::to_string(i), "value" + std::to_string(i));
    }
    for (auto _ : state) {
        auto val = store.get("key0");
        benchmark::DoNotOptimize(val);
    }
}

static void BM_Set(benchmark::State& state) {
    RcuStore<std::string, std::string> store;
    for (auto _ : state) {
        store.set("key", "value");
    }
}

BENCHMARK(BM_Get);
BENCHMARK(BM_Set);
