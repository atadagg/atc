#pragma once

#include <cstdint>
#include <vector>

#include "world.hpp"

// The money chokepoint. `transfer` is the only code permitted to write
// `liquidity`, which makes invariant 1 (money conservation) true by
// construction rather than verified after the fact: one subtraction and one
// addition of the same double, in one place.
//
// Overdraft is deliberately not policed here — it is a policy question the
// caller answers. Today no caller may overdraw (`month_close::pay_wages`
// handles an unaffordable payroll by cutting the posted wage, the
// no-bankruptcy rule); credit will introduce accounts that legitimately go
// negative, which is a change to the account type, not to `transfer`.
namespace ledger {

enum class Kind : uint8_t { Firm, Household };

// A tagged index, following invariant 3: indices between agents, never
// pointers.
struct Account {
    Kind kind;
    uint32_t index;
};

constexpr Account firm(uint32_t i) { return {Kind::Firm, i}; }
constexpr Account household(uint32_t i) { return {Kind::Household, i}; }

// Why a movement happened. Widened as new modules move money -- a bank, a
// stock market, firm-to-firm trade.
enum class Reason : uint8_t {
    Unspecified,
    Purchase,  // household -> firm, a daily shop visit
    Wage,      // firm -> employee, at month close
    Profit,    // firm -> household, the ownership proxy
};

// Wages and profits move at month close, outside any day.
inline constexpr int kMonthClose = -1;

struct Entry {
    int month = 0;
    int day = kMonthClose;
    uint32_t seq = 0;  // monotonic over the whole run: a total order
    Account from{Kind::Firm, 0};
    Account to{Kind::Firm, 0};
    double amount = 0.0;
    Reason reason = Reason::Unspecified;
};

// The optional sink for entries. Opt-in on purpose: the reference run is
// 7000 months of ~1000 households shopping daily, so recording every movement
// unconditionally would dominate runtime and memory. Off by default; the
// viewer and targeted analysis runs turn it on.
//
// It is not part of `World`, and therefore not part of `SimSnapshot` -- a
// journal is an output, and the core's outputs go through a sink it does not
// own (invariant 5).
class Journal {
public:
    virtual ~Journal() = default;

    // The scheduler stamps the clock; `transfer` does not know what day it is.
    void set_time(int month, int day) {
        month_ = month;
        day_ = day;
    }

    // Called by `transfer` only, and only after the movement has succeeded.
    void record(Account from, Account to, double amount, Reason reason) {
        append(Entry{month_, day_, seq_++, from, to, amount, reason});
    }

protected:
    virtual void append(const Entry& e) = 0;

private:
    int month_ = 0;
    int day_ = kMonthClose;
    uint32_t seq_ = 0;
};

// Keeps every entry in memory. For tests and short analysis runs; a 6000-month
// run would need gigabytes.
class VectorJournal : public Journal {
public:
    std::vector<Entry> entries;

protected:
    void append(const Entry& e) override { entries.push_back(e); }
};

// Debits `from`, credits `to`, and -- if `journal` is non-null -- records the
// movement once it has succeeded. A rejected transfer records nothing.
// Throws std::invalid_argument if `amount` is negative or non-finite — a
// negative transfer is a caller bug, not a reversed direction.
// Throws std::out_of_range if either account index does not exist.
void transfer(World& w, Account from, Account to, double amount,
              Reason reason = Reason::Unspecified, Journal* journal = nullptr);

}  // namespace ledger
