#pragma once

class World;

namespace month_close {

void pay_wages(World& w);
void distribute_profits(World& w);
void update_reservation_wages(World& w);
void apply_pending_fires(World& w);
void roll_demand(World& w);

// The full end-of-month sequence, in order.
void run(World& w);

}  // namespace month_close
