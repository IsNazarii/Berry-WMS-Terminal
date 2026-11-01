#pragma once

#include "wms/ie04.h"
#include "wms/scale.h"

#include <QElapsedTimer>
#include <QSerialPort>
#include <QString>

#include <chrono>

namespace wms {

// IE-04 scale on a serial port, protocol 1: the scale streams frames on its own and
// the terminal only listens, so it can never take a tare or zero the scale by mistake.
// Non-blocking: each read() takes whatever arrived since the previous one.
// Needs a running Qt event loop, which fills the port's buffer.
class SerialScaleDriver final : public IScaleDriver {
public:
    // Prefer /dev/serial/by-id/...: /dev/ttyUSB0 may become ttyUSB1 after a replug.
    // `silenceLimit`: how long the scale may go without a weight (silent, or an error
    // code on the display) before the connection counts as lost.
    explicit SerialScaleDriver(const QString& portName,
                               std::chrono::milliseconds silenceLimit = std::chrono::seconds{2});

    [[nodiscard]] std::string name() const override { return "Ваги ІЕ-04"; }
    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const override { return port_.isOpen(); }
    // Closes the port when the connection is lost, so the caller knows to reopen it.
    [[nodiscard]] std::optional<ScaleReading> read() override;

private:
    QSerialPort port_;
    Ie04FrameReader reader_;
    QElapsedTimer sinceReading_;
    std::chrono::milliseconds silenceLimit_;
};

} // namespace wms
