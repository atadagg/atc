#include "scenario.hpp"

#include <charconv>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace scenario {
namespace {

[[noreturn]] void fail(const std::string& what, int line) {
    throw std::runtime_error("scenario: line " + std::to_string(line) + ": " + what);
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream in(line);
    std::string field;
    while (in >> field) fields.push_back(field);
    return fields;
}

long long parse_int(const std::string& s, int line) {
    long long value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
        fail("expected an integer, got '" + s + "'", line);
    }
    return value;
}

double parse_double(const std::string& s, int line) {
    // from_chars for double is not available in every stdlib we care about;
    // strtod with a full-consumption check is the portable equivalent of the
    // strictness used for integers.
    const char* begin = s.c_str();
    char* end = nullptr;
    double value = std::strtod(begin, &end);
    if (end != begin + s.size() || s.empty()) {
        fail("expected a number, got '" + s + "'", line);
    }
    return value;
}

const char* action_name(intervention::Action a) {
    switch (a) {
        case intervention::Action::SetParam:             return "set_param";
        case intervention::Action::Issue:                return "issue";
        case intervention::Action::IssueToAllHouseholds: return "issue_all";
    }
    return "unknown";
}

}  // namespace

void write(const Scenario& s, std::ostream& out) {
    out << "atc-scenario " << kFormatVersion << '\n';
    out << "seed " << s.seed << '\n';

    auto specs = intervention::param_specs();
    for (const auto& i : s.log.all()) {
        out << i.month << ' ' << action_name(i.action);
        switch (i.action) {
            case intervention::Action::SetParam:
                out << ' ' << specs[i.param].name << ' ' << i.value;
                break;
            case intervention::Action::Issue:
                out << ' ' << i.issuer << ' ' << i.target << ' ' << i.value;
                break;
            case intervention::Action::IssueToAllHouseholds:
                out << ' ' << i.issuer << ' ' << i.value;
                break;
        }
        out << '\n';
    }
}

Scenario read(std::istream& in) {
    Scenario s;
    std::string line;
    int number = 0;
    bool saw_header = false;
    bool saw_seed = false;

    while (std::getline(in, line)) {
        ++number;
        if (auto hash = line.find('#'); hash != std::string::npos) line.resize(hash);
        auto f = split(line);
        if (f.empty()) continue;

        if (!saw_header) {
            if (f.size() != 2 || f[0] != "atc-scenario") {
                fail("expected 'atc-scenario <version>' as the first record", number);
            }
            if (parse_int(f[1], number) != kFormatVersion) {
                fail("unsupported format version '" + f[1] + "'", number);
            }
            saw_header = true;
            continue;
        }

        if (f[0] == "seed") {
            if (f.size() != 2) fail("seed takes one value", number);
            s.seed = static_cast<uint64_t>(parse_int(f[1], number));
            saw_seed = true;
            continue;
        }

        const int month = static_cast<int>(parse_int(f[0], number));
        if (f.size() < 2) fail("a record needs a month and an action", number);
        const std::string& action = f[1];

        if (action == "set_param") {
            if (f.size() != 4) fail("set_param takes <name> <value>", number);
            if (intervention::find_param(f[2]) == intervention::kNoParam) {
                fail("no such parameter '" + f[2] + "'", number);
            }
            s.log.set_param(month, f[2], parse_double(f[3], number));
        } else if (action == "issue") {
            if (f.size() != 5) fail("issue takes <issuer> <household> <amount>", number);
            s.log.issue(month, static_cast<uint32_t>(parse_int(f[2], number)),
                        static_cast<uint32_t>(parse_int(f[3], number)),
                        parse_double(f[4], number));
        } else if (action == "issue_all") {
            if (f.size() != 4) fail("issue_all takes <issuer> <amount each>", number);
            s.log.issue_to_all_households(month,
                                          static_cast<uint32_t>(parse_int(f[2], number)),
                                          parse_double(f[3], number));
        } else {
            fail("unknown action '" + action + "'", number);
        }
    }

    if (!saw_header) throw std::runtime_error("scenario: empty or missing header");
    if (!saw_seed) throw std::runtime_error("scenario: no seed record");
    return s;
}

}  // namespace scenario
