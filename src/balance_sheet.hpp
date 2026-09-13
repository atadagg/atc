#pragma once

#include <cstdint>

class World;

namespace balance_sheet {

// A firm's position, COMPUTED from firm state. Equity is never stored --
// the same discipline as World::books_balance(), so it cannot silently drift
// away from the things it is the sum of.
struct Sheet {
    double cash = 0.0;
    double input_stock = 0.0;   // at cost
    double output_stock = 0.0;  // at cost
    double receivables = 0.0;
    double payables = 0.0;

    double assets() const { return cash + input_stock + output_stock + receivables; }
    double liabilities() const { return payables; }
    double equity() const { return assets() - liabilities(); }

    // With no debt a firm's equity is a sum of non-negative terms, so a firm
    // that has never bought on credit cannot reach this state. That is what
    // keeps the single-good world's behaviour unchanged by the lifecycle.
    bool insolvent() const { return equity() < 0.0; }
};

Sheet of(const World& w, uint32_t firm);

}  // namespace balance_sheet
