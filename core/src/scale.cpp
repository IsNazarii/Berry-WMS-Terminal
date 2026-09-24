#include "wms/scale.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace wms {

SimulatedScaleDriver::SimulatedScaleDriver(std::vector<Weight> script) : script_{std::move(script)} {}

void SimulatedScaleDriver::setScript(std::vector<Weight> script) {
    script_ = std::move(script);
    position_ = 0;
}

bool SimulatedScaleDriver::open() {
    open_ = true;
    return true;
}

void SimulatedScaleDriver::close() { open_ = false; }

std::optional<Weight> SimulatedScaleDriver::read() {
    if (!open_ || script_.empty()) return std::nullopt;

    const Weight reading = script_[std::min(position_, script_.size() - 1)];
    if (position_ < script_.size()) ++position_;
    return reading;
}

StabilityDetector::StabilityDetector(std::size_t windowSize, Weight tolerance)
    : windowSize_{windowSize}, tolerance_{tolerance} {
    if (windowSize_ == 0) throw std::invalid_argument("Stability window must hold at least one reading");
    if (tolerance_.isNegative()) throw std::invalid_argument("Stability tolerance must not be negative");
}

void StabilityDetector::push(Weight reading) {
    window_.push_back(reading);
    if (window_.size() > windowSize_) window_.pop_front();
}

bool StabilityDetector::isStable() const noexcept {
    if (window_.size() < windowSize_) return false;
    const auto [min, max] = std::minmax_element(window_.begin(), window_.end());
    return *max - *min <= tolerance_;
}

std::optional<Weight> StabilityDetector::stableWeight() const noexcept {
    if (!isStable()) return std::nullopt;

    Weight sum;
    for (const Weight w : window_) sum += w;
    return grams(divideRounded(sum.raw(), static_cast<std::int64_t>(window_.size())));
}

std::optional<Weight> StabilityDetector::lastReading() const noexcept {
    if (window_.empty()) return std::nullopt;
    return window_.back();
}

} // namespace wms
