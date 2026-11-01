#include "wms/serial_scale_driver.h"

#include "scale_reading_print.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <gtest/gtest.h>

#include <pty.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace wms;
using namespace std::chrono_literals;

namespace {

const std::string kLoaded{"<\x01   166\x05>", 10}; // 16.6 kg, stable
const std::string kEmpty{"<\x01    00\x0d>", 10};  // 0.0 kg, stable, ZERO

// Plays the scale: whatever is written to the master end comes out of the slave
// end, which the driver opens like /dev/ttyUSB0.
class FakeScale {
public:
    FakeScale() {
        if (openpty(&master_, &slave_, nullptr, nullptr, nullptr) != 0) throw std::runtime_error("openpty");
        portName_ = QString::fromLocal8Bit(ttyname(slave_));
    }
    ~FakeScale() {
        unplug();
        if (slave_ >= 0) ::close(slave_);
    }
    FakeScale(const FakeScale&) = delete;
    FakeScale& operator=(const FakeScale&) = delete;

    [[nodiscard]] const QString& portName() const { return portName_; }
    void send(const std::string& bytes) const {
        ASSERT_EQ(::write(master_, bytes.data(), bytes.size()), static_cast<ssize_t>(bytes.size()));
    }
    void unplug() {
        if (master_ >= 0) ::close(master_);
        master_ = -1;
    }

private:
    int master_ = -1;
    int slave_ = -1;
    QString portName_;
};

// What a terminal tick sees after `wait`: the event loop runs meanwhile and
// moves the bytes into the port's buffer.
std::optional<ScaleReading> readAfter(SerialScaleDriver& driver, std::chrono::milliseconds wait) {
    QElapsedTimer timer;
    timer.start();
    while (timer.durationElapsed() < wait) {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    return driver.read();
}

} // namespace

TEST(SerialScaleDriver, FailsToOpenAMissingPort) {
    SerialScaleDriver driver{"/dev/serial/by-id/no-such-scale"};
    EXPECT_FALSE(driver.open());
    EXPECT_FALSE(driver.isOpen());
    EXPECT_FALSE(driver.read());
}

TEST(SerialScaleDriver, ReadsTheLatestFrame) {
    FakeScale scale;
    SerialScaleDriver driver{scale.portName()};
    ASSERT_TRUE(driver.open());

    // Opened mid-frame: garbage first, then two frames and the start of a third.
    scale.send(std::string{"\x00\xf8", 2} + kEmpty + kLoaded + kEmpty.substr(0, 4));
    EXPECT_EQ(readAfter(driver, 50ms), (ScaleReading{grams(16'600), true, false, false}));

    scale.send(kEmpty.substr(4));
    EXPECT_EQ(readAfter(driver, 50ms), (ScaleReading{grams(0), true, true, false}));
}

TEST(SerialScaleDriver, NothingNewIsNotALostConnection) {
    FakeScale scale;
    SerialScaleDriver driver{scale.portName(), 500ms};
    ASSERT_TRUE(driver.open());
    scale.send(kLoaded);
    EXPECT_TRUE(readAfter(driver, 50ms));
    EXPECT_FALSE(readAfter(driver, 50ms));
    EXPECT_TRUE(driver.isOpen());
}

TEST(SerialScaleDriver, ClosesWhenTheScaleFallsSilent) {
    FakeScale scale;
    SerialScaleDriver driver{scale.portName(), 100ms};
    ASSERT_TRUE(driver.open());
    scale.send(kLoaded);
    EXPECT_TRUE(readAfter(driver, 50ms));

    EXPECT_FALSE(readAfter(driver, 150ms));
    EXPECT_FALSE(driver.isOpen());

    // The monitor reopens it on the next tick and readings flow again.
    ASSERT_TRUE(driver.open());
    scale.send(kEmpty);
    EXPECT_EQ(readAfter(driver, 50ms), (ScaleReading{grams(0), true, true, false}));
}

TEST(SerialScaleDriver, ClosesWhenTheDisplayShowsNoNumber) {
    FakeScale scale;
    SerialScaleDriver driver{scale.portName(), 100ms};
    ASSERT_TRUE(driver.open());
    scale.send(kLoaded);
    EXPECT_TRUE(readAfter(driver, 50ms));

    // An error code on the display: frames keep coming, but none has a weight.
    const std::string error{"<\x01 Err 1\x00>", 10};
    for (int tick = 0; tick < 4; ++tick) {
        scale.send(error);
        EXPECT_FALSE(readAfter(driver, 40ms));
    }
    EXPECT_FALSE(driver.isOpen());
}

TEST(SerialScaleDriver, ClosesWhenUnplugged) {
    FakeScale scale;
    SerialScaleDriver driver{scale.portName()};
    ASSERT_TRUE(driver.open());
    scale.send(kLoaded);
    EXPECT_TRUE(readAfter(driver, 50ms));

    scale.unplug();
    EXPECT_FALSE(readAfter(driver, 100ms));
    EXPECT_FALSE(driver.isOpen());
}

TEST(SerialScaleDriver, ReadsTheRecordedCaptureInTicks) {
    std::ifstream file{WMS_TEST_DATA_DIR "/ie04_protocol1_capture.bin", std::ios::binary};
    ASSERT_TRUE(file);
    const std::string capture{std::istreambuf_iterator<char>{file}, {}};

    FakeScale scale;
    SerialScaleDriver driver{scale.portName()};
    ASSERT_TRUE(driver.open());
    std::optional<ScaleReading> last;
    for (std::size_t at = 0; at < capture.size(); at += 120) {
        scale.send(capture.substr(at, 120));
        if (auto r = readAfter(driver, 10ms)) last = r;
    }
    EXPECT_EQ(last, (ScaleReading{grams(-65'600), true, true, true}));
}

int main(int argc, char** argv) {
    // QSerialPort fills its buffer from the event loop.
    QCoreApplication app{argc, argv};
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
