#pragma once

#include <cstdint>

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

// Debits `from`, credits `to`.
// Throws std::invalid_argument if `amount` is negative or non-finite — a
// negative transfer is a caller bug, not a reversed direction.
// Throws std::out_of_range if either account index does not exist.
void transfer(World& w, Account from, Account to, double amount);

}  // namespace ledger
