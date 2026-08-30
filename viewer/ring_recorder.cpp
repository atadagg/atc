#include "ring_recorder.hpp"

#include <stdexcept>

RingRecorder::RingRecorder(size_t capacity) : buf_(capacity) {
    if (capacity == 0) throw std::invalid_argument("RingRecorder capacity must be > 0");
}

void RingRecorder::record(const Metrics& m) {
    buf_[next_] = m;
    next_ = (next_ + 1) % buf_.size();
    if (size_ < buf_.size()) ++size_;
}

const Metrics& RingRecorder::at(size_t i) const {
    if (i >= size_) throw std::out_of_range("RingRecorder::at index out of range");
    // Once full, the oldest live slot is the one next_ is about to overwrite.
    const size_t oldest = (size_ == buf_.size()) ? next_ : 0;
    return buf_[(oldest + i) % buf_.size()];
}

void RingRecorder::clear() {
    next_ = 0;
    size_ = 0;
}
