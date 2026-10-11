#pragma once

#include "wms/workday.h"

#include <optional>

namespace wms {

// Where sessions live between runs, the open one and the closed archive.
// A session changes all day (prices, trucks, open receipts), so it is kept
// as its latest state rather than as a log like the journal.
class SessionStore {
public:
    virtual ~SessionStore() = default;

    // Writes the whole session over what was stored for it before. Called
    // after every change, so a restart loses nothing, an unclosed weighing included.
    virtual void save(const WorkDay& day) = 0;

    // The session left open, to go on with after a restart; nullopt when none is.
    [[nodiscard]] virtual std::optional<WorkDay> current() const = 0;
    // The session closed last: its trucks are the ones to repeat or carry over.
    // Nullopt before the first session is closed.
    [[nodiscard]] virtual std::optional<WorkDay> lastClosed() const = 0;

protected:
    SessionStore() = default;
    SessionStore(const SessionStore&) = default;
    SessionStore& operator=(const SessionStore&) = default;
};

} // namespace wms
