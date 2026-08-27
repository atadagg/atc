#include <charconv>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>

#include "params.hpp"
#include "recorder.hpp"
#include "rng.hpp"
#include "scheduler.hpp"
#include "world.hpp"

namespace {

long parse_long(std::string_view s, long fallback) {
    long value = fallback;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    return (ec == std::errc{}) ? value : fallback;
}

}  // namespace

int main(int argc, char** argv) {
    uint64_t seed = 1;
    int months = params::kMeasurementMonths;
    int burn_in = params::kBurnInMonths;
    std::string out_path = "output/run.csv";

    for (int i = 1; i + 1 < argc; i += 2) {
        std::string_view flag(argv[i]);
        std::string_view value(argv[i + 1]);
        if (flag == "--seed")         seed = static_cast<uint64_t>(parse_long(value, 1));
        else if (flag == "--months")  months = static_cast<int>(parse_long(value, months));
        else if (flag == "--burn-in") burn_in = static_cast<int>(parse_long(value, burn_in));
        else if (flag == "--out")     out_path = std::string(value);
    }

    std::println("seed={} burn_in={} months={} out={}", seed, burn_in, months, out_path);

    Rng rng(seed);
    World world(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler scheduler(world, rng);

    const double money_at_start = world.total_money();

    std::unique_ptr<CsvRecorder> recorder;
    try {
        recorder = std::make_unique<CsvRecorder>(out_path);
    } catch (const std::runtime_error& e) {
        std::println(stderr, "error: could not open output path '{}': {}", out_path, e.what());
        return 1;
    }

    for (int m = 0; m < burn_in; ++m) scheduler.run_month();

    for (int m = 0; m < months; ++m) {
        scheduler.run_month();
        recorder->record(compute_metrics(world, m));
    }

    const double money_at_end = world.total_money();
    std::println("money start={:.6f} end={:.6f} drift={:.9f}",
                 money_at_start, money_at_end, money_at_end - money_at_start);

    return 0;
}
