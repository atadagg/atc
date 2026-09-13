#include "balance_sheet.hpp"

#include "agents.hpp"
#include "params.hpp"
#include "world.hpp"

namespace balance_sheet {

Sheet of(const World& w, uint32_t firm) {
    const Firm& f = w.firms[firm];

    Sheet s;
    s.cash = f.liquidity;
    s.receivables = f.receivables;
    s.payables = f.payables;
    s.output_stock = f.inventory * f.output_cost_basis;

    for (uint32_t i = 0; i < params::kMaxInputs; ++i) {
        s.input_stock += f.input_stock[i] * f.input_cost_basis[i];
    }

    return s;
}

}  // namespace balance_sheet
