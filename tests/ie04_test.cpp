#include "wms/ie04.h"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>

using namespace wms;

namespace wms {
// Readable failure messages instead of a byte dump.
void PrintTo(const ScaleReading& r, std::ostream* os) {
    *os << r.weight.raw() << " g" << (r.stable ? " stable" : "") << (r.zero ? " zero" : "")
        << (r.tare ? " tare" : "");
}
} // namespace wms

namespace {

// Frames copied byte for byte from tests/data/ie04_protocol1_capture.bin.
// Layout: '<' address, six display characters (no decimal point), status, '>'.
const std::string kEmpty{"<\x01    00\x0d>", 10};        // 0.0, stable, ZERO
const std::string kLoaded{"<\x01   166\x05>", 10};       // 16.6, stable
const std::string kSwinging{"<\x01   722\x01>", 10};     // 72.2, not settled
const std::string kTareMinus{"<\x01  -656\x3d>", 10};    // -65.6 net, stable, ZERO (gross), TARE, minus bit
const std::string kMinusNoBit{"<\x01   -02\x11>", 10};   // '-' on the display, minus bit not yet set
const std::string kBitNoMinus{"<\x01   -02\x31>", 10};   // both agree again a frame later

std::string withByte(std::string frame, std::size_t i, char c) {
    frame[i] = c;
    return frame;
}

} // namespace

TEST(Ie04Frame, EmptyPlatform) {
    EXPECT_EQ(parseIe04Frame(kEmpty), (ScaleReading{grams(0), true, true, false}));
}

TEST(Ie04Frame, DecimalPointComesFromStatusBits) {
    EXPECT_EQ(parseIe04Frame(kLoaded), (ScaleReading{grams(16'600), true, false, false}));
    // S1S0 = 0: the same digits are whole kilograms; 3: grams.
    EXPECT_EQ(parseIe04Frame(withByte(kLoaded, 8, '\x04'))->weight, grams(166'000));
    EXPECT_EQ(parseIe04Frame(withByte(kLoaded, 8, '\x07'))->weight, grams(166));
}

TEST(Ie04Frame, NotSettled) {
    EXPECT_EQ(parseIe04Frame(kSwinging), (ScaleReading{grams(72'200), false, false, false}));
}

TEST(Ie04Frame, NegativeNetWithTareAndGrossZero) {
    EXPECT_EQ(parseIe04Frame(kTareMinus), (ScaleReading{grams(-65'600), true, true, true}));
}

TEST(Ie04Frame, MinusSignOrMinusBitIsEnough) {
    EXPECT_EQ(parseIe04Frame(kMinusNoBit)->weight, grams(-200));
    EXPECT_EQ(parseIe04Frame(kBitNoMinus)->weight, grams(-200));
    EXPECT_EQ(parseIe04Frame(std::string{"<\x01    02\x31>", 10})->weight, grams(-200));
}

TEST(Ie04Frame, RejectsWrongSizeOrDelimiters) {
    EXPECT_FALSE(parseIe04Frame(kLoaded.substr(0, 9)));
    EXPECT_FALSE(parseIe04Frame(kLoaded + ">"));
    EXPECT_FALSE(parseIe04Frame(withByte(kLoaded, 0, 'x')));
    EXPECT_FALSE(parseIe04Frame(withByte(kLoaded, 9, 'x')));
}

TEST(Ie04Frame, NonNumericDisplayIsNoReading) {
    EXPECT_FALSE(parseIe04Frame(std::string{"<\x01   ???\x3f>", 10})); // reply to an unknown command
    EXPECT_FALSE(parseIe04Frame(std::string{"<\x01  Err1\x00>", 10}));
    EXPECT_FALSE(parseIe04Frame(std::string{"<\x01      \x05>", 10}));  // blank display
    EXPECT_FALSE(parseIe04Frame(std::string{"<\x01  1 66\x05>", 10}));  // space inside the number
    EXPECT_FALSE(parseIe04Frame(std::string{"<\x01  16-6\x05>", 10}));  // minus inside the number
    EXPECT_FALSE(parseIe04Frame(std::string{"<\x01   --6\x05>", 10}));
}

TEST(Ie04FrameReader, NothingUntilAFrameIsComplete) {
    Ie04FrameReader reader;
    EXPECT_FALSE(reader.push(""));
    EXPECT_FALSE(reader.push(kLoaded.substr(0, 4)));
    EXPECT_EQ(reader.push(kLoaded.substr(4)), parseIe04Frame(kLoaded));
}

TEST(Ie04FrameReader, SkipsGarbageAndTakesTheLatestFrame) {
    Ie04FrameReader reader;
    // The port opened mid-frame: the capture starts with "00 f8".
    const std::string chunk = std::string{"\x00\xf8", 2} + kSwinging + kLoaded + kEmpty.substr(0, 3);
    EXPECT_EQ(reader.push(chunk), parseIe04Frame(kLoaded));
    // The cut-off frame is finished by the next chunk.
    EXPECT_EQ(reader.push(kEmpty.substr(3)), parseIe04Frame(kEmpty));
}

TEST(Ie04FrameReader, NoReadingWhenTheLatestFrameIsNotANumber) {
    Ie04FrameReader reader;
    EXPECT_FALSE(reader.push(kLoaded + std::string{"<\x01  Err1\x00>", 10}));
}

TEST(Ie04FrameReader, DoesNotGrowOnEndlessGarbage) {
    Ie04FrameReader reader;
    for (int i = 0; i < 1'000; ++i) EXPECT_FALSE(reader.push(std::string(64, '\xf8')));
    EXPECT_LE(reader.buffered(), kIe04FrameSize - 1);
    EXPECT_EQ(reader.push(kLoaded), parseIe04Frame(kLoaded));
}

TEST(Ie04FrameReader, ReadsTheWholeCapture) {
    std::ifstream file{WMS_TEST_DATA_DIR "/ie04_protocol1_capture.bin", std::ios::binary};
    ASSERT_TRUE(file);
    const std::string capture{std::istreambuf_iterator<char>{file}, {}};

    // 120 bytes ≈ one 250 ms tick at 4800 baud; chunk edges fall inside frames.
    Ie04FrameReader reader;
    int readings = 0;
    std::optional<ScaleReading> last;
    for (std::size_t at = 0; at < capture.size(); at += 120) {
        if (auto r = reader.push(std::string_view{capture}.substr(at, 120))) {
            ++readings;
            last = r;
        }
    }
    EXPECT_EQ(readings, static_cast<int>((capture.size() + 119) / 120));
    EXPECT_EQ(last, (ScaleReading{grams(-65'600), true, true, true}));
}
