#include "wms/sqlite.h"

#include <sqlite3.h>

#include <utility>

namespace wms::sqlite {

namespace {

[[noreturn]] void fail(sqlite3* db, int code) {
    // The connection's message is more specific ("no such table: x") than the code's.
    const char* message = db != nullptr ? sqlite3_errmsg(db) : sqlite3_errstr(code);
    throw Error{code & 0xFF, message};
}

void check(sqlite3* db, int code) {
    if (code != SQLITE_OK) fail(db, code);
}

} // namespace

// --- Statement ---

Statement::Statement(sqlite3* db, std::string_view sql) : db_{db} {
    check(db_, sqlite3_prepare_v2(db_, sql.data(), static_cast<int>(sql.size()), &statement_, nullptr));
}

Statement::Statement(Statement&& other) noexcept
    : db_{other.db_}, statement_{std::exchange(other.statement_, nullptr)} {}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        sqlite3_finalize(statement_);
        db_ = other.db_;
        statement_ = std::exchange(other.statement_, nullptr);
    }
    return *this;
}

Statement::~Statement() { sqlite3_finalize(statement_); }

Statement& Statement::bind(int index, std::int64_t value) {
    check(db_, sqlite3_bind_int64(statement_, index, value));
    return *this;
}

Statement& Statement::bind(int index, std::string_view text) {
    // SQLITE_TRANSIENT: sqlite copies the text, the view may die before step().
    check(db_, sqlite3_bind_text(statement_, index, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT));
    return *this;
}

Statement& Statement::bind(int index, std::nullopt_t) {
    check(db_, sqlite3_bind_null(statement_, index));
    return *this;
}

Statement& Statement::bind(int index, std::optional<std::int64_t> value) {
    return value ? bind(index, *value) : bind(index, std::nullopt);
}

bool Statement::step() {
    const int code = sqlite3_step(statement_);
    if (code == SQLITE_ROW) return true;
    if (code == SQLITE_DONE) return false;
    fail(db_, code);
}

void Statement::reset() {
    sqlite3_reset(statement_);
    sqlite3_clear_bindings(statement_);
}

bool Statement::isNull(int column) const { return sqlite3_column_type(statement_, column) == SQLITE_NULL; }

std::int64_t Statement::columnInt(int column) const { return sqlite3_column_int64(statement_, column); }

std::string Statement::columnText(int column) const {
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement_, column));
    // Byte count is asked after the text: that order avoids a second conversion.
    return text != nullptr ? std::string(text, static_cast<std::size_t>(sqlite3_column_bytes(statement_, column)))
                           : std::string{};
}

// --- Database ---

Database::Database(const std::string& path) {
    const int code = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (code != SQLITE_OK) {
        // sqlite3 hands out a handle even on failure; take the message, then close it.
        const Error error{code & 0xFF, db_ != nullptr ? sqlite3_errmsg(db_) : sqlite3_errstr(code)};
        sqlite3_close(db_);
        throw error;
    }
    // Two terminals may share nothing, but the UI and a backup may touch the
    // file at once: wait a little instead of failing with SQLITE_BUSY.
    sqlite3_busy_timeout(db_, 2000);
    execute("PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL");
}

Database::Database(Database&& other) noexcept : db_{std::exchange(other.db_, nullptr)} {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        sqlite3_close(db_);
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

Database::~Database() {
    // Statements must be finalized first; owners keep them shorter-lived than the Database.
    sqlite3_close(db_);
}

void Database::execute(std::string_view sql) {
    const std::string text{sql}; // sqlite3_exec needs a terminated string
    check(db_, sqlite3_exec(db_, text.c_str(), nullptr, nullptr, nullptr));
}

Statement Database::prepare(std::string_view sql) { return Statement{db_, sql}; }

void Database::backupTo(const std::string& path) {
    Statement vacuum = prepare("VACUUM INTO ?1");
    vacuum.bind(1, path);
    vacuum.step();
}

std::int64_t Database::lastInsertRowId() const noexcept { return sqlite3_last_insert_rowid(db_); }

// --- ScopedTransaction ---

// Savepoints rather than BEGIN/COMMIT, since only they nest. The outermost
// one starts a deferred transaction; that is fine with a single writer (one
// terminal process), as WAL readers never block it.
ScopedTransaction::ScopedTransaction(Database& db) : db_{db} { db_.execute("SAVEPOINT scoped"); }

ScopedTransaction::~ScopedTransaction() {
    if (done_) return;
    try {
        // ROLLBACK TO undoes the changes but keeps the savepoint open; RELEASE ends it.
        db_.execute("ROLLBACK TO scoped; RELEASE scoped");
    } catch (const Error&) {
        // sqlite rolls back by itself after some errors; nothing left to undo.
    }
}

void ScopedTransaction::commit() {
    // Durable only when the outermost savepoint is released.
    db_.execute("RELEASE scoped");
    done_ = true;
}

} // namespace wms::sqlite
