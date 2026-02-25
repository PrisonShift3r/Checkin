-- ============================================================
-- Check-in Service — Database Schema
-- ============================================================

-- Main checkins table (SoT for registration facts)
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
-- Ensure one successful checkin per ticket (idempotency)
CREATE UNIQUE INDEX IF NOT EXISTS checkins_ticket_success_uidx
    ON checkins(ticket_id) WHERE status = 'success';

-- Kafka idempotency: track processed event UUIDs
CREATE TABLE IF NOT EXISTS processed_events (
    event_id     UUID        PRIMARY KEY,
    processed_at TIMESTAMP   NOT NULL DEFAULT now()
);

-- Cleanup old processed events (run periodically, e.g. via pg_cron)
-- DELETE FROM processed_events WHERE processed_at < now() - INTERVAL '7 days';

-- Transactional outbox for reliable Kafka publishing
-- Events written here in the same DB transaction as the checkin row.
-- Background worker reads and publishes them, then deletes on success.
CREATE TABLE IF NOT EXISTS checkin_outbox (
    id          BIGSERIAL   PRIMARY KEY,
    topic       TEXT        NOT NULL,
    key         TEXT,
    payload     TEXT        NOT NULL,
    created_at  TIMESTAMP   NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS checkin_outbox_id_idx ON checkin_outbox(id);

-- Auto-update updated_at on checkins
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
