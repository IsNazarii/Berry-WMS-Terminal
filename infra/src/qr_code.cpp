#include "wms/qr_code.h"

#include <qrcodegen.hpp>

#include <cstddef>

namespace wms {

QrMatrix::QrMatrix(const std::string& text) : size_{0} {
    const auto code = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    size_ = code.getSize();
    modules_.reserve(static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_));
    for (int y = 0; y < size_; ++y) {
        for (int x = 0; x < size_; ++x) modules_.push_back(code.getModule(x, y) ? 1 : 0);
    }
}

bool QrMatrix::dark(int x, int y) const {
    return modules_.at(static_cast<std::size_t>(y) * static_cast<std::size_t>(size_) + static_cast<std::size_t>(x)) != 0;
}

} // namespace wms
