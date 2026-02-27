#include "db/migrator.h"
#include <spdlog/spdlog.h>

namespace checkin {

// ─── Embedded schema (mirrors sql/schema.sql) ─────────────────────────────────
// Using a raw string literal for readability.
const char* Migrator::SCHEMA_SQL = R"SQL(
CREATE TABLE IF NOT EXISTS checkins (
    checkin_id    TEXT        PRIMARY KEY,
    flight_id     TEXT        NOT NULL,
    passenger_id  TEXT        NOT NULL,
    ticket_id     TEXT        NOT NULL,
    status        TEXT        NOT NULL CHECK (status IN ('started','success','failed')),
    fail_reason   TEXT,
    created_at    TIMESTAMP   NOT NULL DEFAULT now(),
    updated_at    TIMESTAMP   NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS checkins_flight_idx    ON checkins(flight_id);
CREATE INDEX IF NOT EXISTS checkins_passenger_idx ON checkins(passenger_id);
CREATE INDEX IF NOT EXISTS checkins_status_idx    ON checkins(status);

CREATE UNIQUE INDEX IF NOT EXISTS checkins_ticket_success_uidx
    ON checkins(ticket_id) WHERE status = 'success';

CREATE TABLE IF NOT EXISTS checkin_history (
    id           BIGSERIAL   PRIMARY KEY,
    checkin_id   TEXT        NOT NULL REFERENCES checkins(checkin_id) ON DELETE CASCADE,
    old_status   TEXT,
    new_status   TEXT        NOT NULL,
    fail_reason  TEXT,
    changed_at   TIMESTAMP   NOT NULL DEFAULT now(),
    changed_by   TEXT        NOT NULL DEFAULT 'system'
);

CREATE INDEX IF NOT EXISTS checkin_history_checkin_idx ON checkin_history(checkin_id);
CREATE INDEX IF NOT EXISTS checkin_history_changed_idx ON checkin_history(changed_at);

CREATE TABLE IF NOT EXISTS processed_events (
    event_id     UUID        PRIMARY KEY,
    processed_at TIMESTAMP   NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS checkin_outbox (
    id          BIGSERIAL   PRIMARY KEY,
    topic       TEXT        NOT NULL,
    key         TEXT,
    payload     TEXT        NOT NULL,
    created_at  TIMESTAMP   NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS checkin_outbox_id_idx ON checkin_outbox(id);

CREATE OR REPLACE FUNCTION update_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = now();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS checkins_updated_at ON checkins;
CREATE TRIGGER checkins_updated_at
    BEFORE UPDATE ON checkins
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();

CREATE OR REPLACE FUNCTION checkins_write_history()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO checkin_history (checkin_id, old_status, new_status, fail_reason, changed_by)
        VALUES (NEW.checkin_id, NULL, NEW.status, NEW.fail_reason, 'system');
    ELSIF TG_OP = 'UPDATE' AND OLD.status <> NEW.status THEN
        INSERT INTO checkin_history (checkin_id, old_status, new_status, fail_reason, changed_by)
        VALUES (NEW.checkin_id, OLD.status, NEW.status, NEW.fail_reason, 'system');
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS checkins_history_trigger ON checkins;
CREATE TRIGGER checkins_history_trigger
    AFTER INSERT OR UPDATE ON checkins
    FOR EACH ROW EXECUTE FUNCTION checkins_write_history();
)SQL";

// ─── Migrator ─────────────────────────────────────────────────────────────────

Migrator::Migrator(Database& db) : db_(db) {}

void Migrator::migrate() {
    spdlog::info("Running DB migrations...");
    try {
        db_.transaction([](pqxx::work& txn) {
            txn.exec(SCHEMA_SQL);
            return true;
        });
        spdlog::info("DB migrations applied successfully");
    } catch (const std::exception& e) {
        spdlog::error("DB migration failed: {}", e.what());
        throw;
    }
}

} // namespace checkin
