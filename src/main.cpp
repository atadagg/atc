#include <charconv>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
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

// Parses the whole of `s` as a base-10 integer. Returns nullopt (rather than
// silently falling back to a default) if any of it fails to parse -- for a
// reproducibility tool, a malformed --seed must never silently become a
// different, unintended run.
std::optional<long> parse_long(std::string_view s) {
    long value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    return value;
}

}  // namespace

int main(int argc, char** argv) {
    uint64_t seed = 1;
    int months = params::kMeasurementMonths;
    int burn_in = params::kBurnInMonths;
    std::string out_path = "output/run.csv";

    for (int i = 1; i < argc; i += 2) {
        std::string_view flag(argv[i]);
        if (i + 1 >= argc) {
            std::println(stderr, "error: flag '{}' is missing a value", flag);
            return 1;
        }
        std::string_view value(argv[i + 1]);

        if (flag == "--seed") {
            auto v = parse_long(value);
            if (!v) {
                std::println(stderr, "error: --seed expects an integer, got '{}'", value);
                return 1;
            }
            seed = static_cast<uint64_t>(*v);
        } else if (flag == "--months") {
            auto v = parse_long(value);
            if (!v) {
                std::println(stderr, "error: --months expects an integer, got '{}'", value);
                return 1;
            }
            months = static_cast<int>(*v);
        } else if (flag == "--burn-in") {
            auto v = parse_long(value);
            if (!v) {
                std::println(stderr, "error: --burn-in expects an integer, got '{}'", value);
                return 1;
            }
            burn_in = static_cast<int>(*v);
        } else if (flag == "--out") {
            out_path = std::string(value);
        } else {
            std::println(stderr, "error: unrecognized flag '{}'", flag);
            return 1;
        }
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
