#pragma once

#include <cstddef>
#include <vector>

#include "recorder.hpp"

// Keeps the last `capacity` Metrics in a fixed-size ring. The viewer's
// time-series source; handed to the simulation exactly as CsvRecorder is,
// so the core still does I/O only through Recorder (invariant 5).
class RingRecorder : public Recorder {
public:
    // Throws std::invalid_argument if capacity == 0.
    explicit RingRecorder(size_t capacity);

    void record(const Metrics& m) override;

    size_t size() const { return size_; }
    size_t capacity() const { return buf_.size(); }

    // i == 0 is the oldest retained month. Throws std::out_of_range if
    // i >= size().
    const Metrics& at(size_t i) const;

    void clear();

private:
    std::vector<Metrics> buf_;
    size_t next_ = 0;   // where the next record lands
    size_t size_ = 0;   // how many slots are live
};
