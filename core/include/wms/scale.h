#pragma once

#include "wms/quantity.h"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace wms {

// Source of weight readings. Real hardware (serial port) lives in the infra
// layer; the core only knows this interface.
class IScaleDriver {
public:
    virtual ~IScaleDriver() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    virtual bool open() = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
    // Next reading, or std::nullopt when closed or nothing is available.
    [[nodiscard]] virtual std::optional<Weight> read() = 0;
};

// Plays back a scripted sequence of readings, then keeps repeating the last one.
// Used for demos and tests without physical scales.
class SimulatedScaleDriver final : public IScaleDriver {
public:
    explicit SimulatedScaleDriver(std::vector<Weight> script = {});

    void setScript(std::vector<Weight> script);

    [[nodiscard]] std::string name() const override { return "Симулятор ваг"; }
    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const override { return open_; }
    [[nodiscard]] std::optional<Weight> read() override;

private:
    std::vector<Weight> script_;
    std::size_t position_ = 0;
    bool open_ = false;
};

// Decides when the weight has settled: the last `windowSize` readings must all
// lie within `tolerance` of each other. Only a stable weight may be accepted
// into a receipt, otherwise the operator could "catch" a favourable swing.
class StabilityDetector {
public:
    StabilityDetector(std::size_t windowSize, Weight tolerance);

    void push(Weight reading);
    void reset() noexcept { window_.clear(); }

    [[nodiscard]] bool isStable() const noexcept;
    // Rounded average of the window when stable.
    [[nodiscard]] std::optional<Weight> stableWeight() const noexcept;
    [[nodiscard]] std::optional<Weight> lastReading() const noexcept;

private:
    std::size_t windowSize_;
    Weight tolerance_;
    std::deque<Weight> window_;
};

} // namespace wms
