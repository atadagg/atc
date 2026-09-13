#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "world.hpp"

// The money chokepoint. `transfer` is the only code permitted to write
// `liquidity`, which makes invariant 1 (money conservation) true by
// construction rather than verified after the fact: one subtraction and one
// addition of the same double, in one place.
//
// Solvency is enforced here, not left to callers: a movement that would push a
// non-issuer below zero is rejected. That converts an impossible state from
// merely unwritten into unrepresentable.
//
// The invariant this serves has no exceptions. Money is never created by a
// movement -- only by issuance, which is itself a balanced movement out of an
// issuer account. A feature that appears to need an exception is missing an
// issuer, not entitled to one.
namespace ledger {

// What sort of account this is. The kind -- not a per-account flag -- decides
// whether the account may go negative, so "which accounts can create money" is
// answerable by reading `may_issue` rather than by auditing a thousand
// booleans. An issuer is a type of thing, not a permission that can be granted
// to an ordinary household by a stray line of code.
enum class Kind : uint8_t { Firm, Household, Issuer };

// An issuer's negative balance is not a deficit or an error: it is the money
// it has issued. Nothing else in the model may go below zero.
constexpr bool may_issue(Kind k) { return k == Kind::Issuer; }

// A tagged index, following invariant 3: indices between agents, never
// pointers.
struct Account {
    Kind kind;
    uint32_t index;
};

constexpr Account firm(uint32_t i) { return {Kind::Firm, i}; }
constexpr Account household(uint32_t i) { return {Kind::Household, i}; }
constexpr Account issuer(uint32_t i) { return {Kind::Issuer, i}; }

// A movement that would push a non-issuer below zero. Distinct from
// std::invalid_argument (the amount is fine) and std::out_of_range (the
// account exists): the request is well-formed but the state forbids it.
class Overdraft : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Why a movement happened. Widened as new modules move money -- a bank, a
// stock market, firm-to-firm trade.
enum class Reason : uint8_t {
    Unspecified,
    Purchase,  // household -> firm, a daily shop visit
    Wage,      // firm -> employee, at month close
    Profit,    // firm -> household, the ownership proxy
    Issuance,  // issuer -> anyone: money entering circulation
    // firm -> firm, paying last month's input bills. Input purchases
    // themselves move no money: they create a payable, settled here.
    TradeCreditSettlement,

    // Sentinel, never a real reason: do not construct an Entry with it and do
    // not tally it. It exists only so kReasonCount cannot drift behind the
    // enum -- see the static_assert below.
    Count,
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

// How many `Reason`s there are. `reason_index` is the tally slot for one, so a
// widened `Reason` widens every total with it rather than leaving a new kind of
// movement silently uncounted.
//
// Forgetting this bump used to be silent and nasty: `by_reason[6]` runs off the
// end of the array straight into `in_by_kind[0]`, corrupting the very flow
// check meant to catch it, with no compile error and no crash. The sentinel and
// the assert turn that into a build failure.
inline constexpr std::size_t kReasonCount = 6;
static_assert(kReasonCount == static_cast<std::size_t>(Reason::Count),
              "kReasonCount is out of step with Reason: a new reason was added "
              "without widening the per-reason tallies.");

constexpr std::size_t reason_index(Reason r) { return static_cast<std::size_t>(r); }

// One month of money movement, reduced to totals. This is a *flow* account: a
// balance says where money ended up, a flow says how much moved and why, and
// nothing in the model checked the second until this existed.
struct MonthFlows {
    int month = 0;
    double by_reason[kReasonCount] = {};

    // Sector totals, indexed by `Kind`. Indexed rather than named so that the
    // hot path is three array adds and no branches at all: this runs on every
    // movement, and a `switch` here is an indirect branch ~855 million times a
    // run. The named accessors below are what the rest of the code reads.
    double in_by_kind[3] = {};
    double out_by_kind[3] = {};

    static constexpr std::size_t slot(Kind k) { return static_cast<std::size_t>(k); }

    // A movement within one sector counts on both sides and so nets to zero,
    // which is what makes `in - out` the sector's net receipts.
    double household_in() const { return in_by_kind[slot(Kind::Household)]; }
    double household_out() const { return out_by_kind[slot(Kind::Household)]; }
    double firm_in() const { return in_by_kind[slot(Kind::Firm)]; }
    double firm_out() const { return out_by_kind[slot(Kind::Firm)]; }

    // Net money created this month: out of issuer accounts, less what came
    // back. Zero in Lengnick v1, which has an issuer but never uses it.
    double issued() const {
        return out_by_kind[slot(Kind::Issuer)] - in_by_kind[slot(Kind::Issuer)];
    }

    // Inline and branchless on purpose. This runs on every movement -- ~855
    // million times in a reference run -- so it must not cost a virtual
    // dispatch or an indirect branch. That is why the tally lives here and is
    // called from the non-virtual `Journal::record`, rather than being a
    // journal subclass of its own.
    void add(Account from, Account to, double amount, Reason reason) {
        by_reason[reason_index(reason)] += amount;
        out_by_kind[slot(from.kind)] += amount;
        in_by_kind[slot(to.kind)] += amount;
    }
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
    //
    // The month's totals are accumulated here, in non-virtual inline code, and
    // the virtual `append` is reached only when something actually wants a row
    // per movement. That ordering is a performance requirement, not a style
    // choice: a journal is attached for every measurement month now, so a
    // virtual call on this path would cost ~1.7s of a 13.5s reference run.
    void record(Account from, Account to, double amount, Reason reason) {
        flows_.add(from, to, amount, reason);
        if (rows_) append(Entry{month_, day_, seq_, from, to, amount, reason});
        ++seq_;
    }

    // Opens a new month, discarding the previous month's totals.
    //
    // Months are opened by the caller rather than inferred from the entries
    // arriving. Inferring would silently fail to roll over a month in which no
    // money moved, and a missing month is precisely the kind of gap the flow
    // totals exist to make visible.
    void begin_month(int month) {
        flows_ = MonthFlows{};
        flows_.month = month;
    }

    const MonthFlows& flows() const { return flows_; }

protected:
    // `rows` says whether this journal wants an `Entry` per movement. A journal
    // that only totals says no and skips the virtual call entirely.
    explicit Journal(bool rows) : rows_(rows) {}

    void set_rows(bool rows) { rows_ = rows; }

    virtual void append(const Entry& e) = 0;

    // Hands an already-built entry to another journal. A journal that wraps
    // another needs this, and it stays out of the public interface so that
    // `record` -- which stamps the clock and the sequence -- remains the only
    // way an entry enters a journal from outside.
    static void append_to(Journal& j, const Entry& e) { j.append(e); }

private:
    MonthFlows flows_;
    int month_ = 0;
    int day_ = kMonthClose;
    uint32_t seq_ = 0;
    bool rows_ = true;
};

// Keeps every entry in memory. For tests and short analysis runs; a 6000-month
// run would need gigabytes.
class VectorJournal : public Journal {
public:
    VectorJournal() : Journal(true) {}

    std::vector<Entry> entries;

protected:
    void append(const Entry& e) override { entries.push_back(e); }
};

// A journal that only totals. It exists so that the flow account can stay
// attached for a whole 6000-month run, which `CsvJournal` cannot: totals are
// O(1) in memory where rows are O(movements).
//
// It carries no accumulation of its own -- that lives in `Journal`, above, so
// that it costs no virtual dispatch. What this class adds is the wrapping: the
// scheduler holds one journal pointer, so wrapping is how the permanent totals
// and a windowed `CsvJournal` coexist without a separate tee class. With no
// wrapped journal it declares that it wants no rows, and the per-movement
// virtual call disappears.
class SummaryJournal : public Journal {
public:
    explicit SummaryJournal(Journal* next = nullptr)
        : Journal(next != nullptr), next_(next) {}

    // Swap the wrapped sink between months -- the mirror of
    // `Scheduler::set_journal`, and how a journal window is opened and closed
    // while the totals keep running.
    void set_next(Journal* next) {
        next_ = next;
        set_rows(next != nullptr);
    }

protected:
    void append(const Entry& e) override {
        if (next_) append_to(*next_, e);
    }

private:
    Journal* next_ = nullptr;
};

// The flows the journal saw do not account for the change in the stock. Money
// moved without being journaled, or was written outside `transfer` altogether.
class FlowMismatch : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// (what came in, less what went out) minus the change in the stock. Zero to
// machine precision when every movement was journaled.
//
// Per-reason totals cannot fail this way -- `transfer` balances them by
// construction. Reconciling against the stock is the check with teeth: it is
// the one that notices a movement the journal never saw.
double flow_discrepancy(const MonthFlows& f, double household_liquidity_before,
                        double household_liquidity_after);

// Throws `FlowMismatch` when the discrepancy is too large to be rounding.
// `scale` is the size of the money stock the comparison sits against; the
// tolerance is relative to it, because a remainder of 1e-9 against 100,000 is
// a summation crumb and the same number against 1e-6 is a real gap. Two
// magnitudes, one tolerance -- the same distinction
// `month_close::check_landed_on_target` draws.
void check_flows(const MonthFlows& f, double household_liquidity_before,
                 double household_liquidity_after, double scale);

// Debits `from`, credits `to`, and -- if `journal` is non-null -- records the
// movement once it has succeeded. A rejected transfer records nothing.
// Throws std::invalid_argument if `amount` is negative or non-finite — a
// negative transfer is a caller bug, not a reversed direction.
// Throws std::out_of_range if either account index does not exist.
// Throws ledger::Overdraft if `from` is not an issuer and holds less than
// `amount`. A rejected transfer moves nothing and records nothing.
void transfer(World& w, Account from, Account to, double amount,
              Reason reason = Reason::Unspecified, Journal* journal = nullptr);

}  // namespace ledger
