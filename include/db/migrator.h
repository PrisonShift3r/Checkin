#pragma once
#include "db/database.h"
#include <string>

namespace checkin {

// Applies SQL migrations at service startup.
// Reads schema.sql from the filesystem (or uses embedded SQL) and runs it
// idempotently (all statements use IF NOT EXISTS / CREATE OR REPLACE).
class Migrator {
public:
    explicit Migrator(Database& db);

    // Run migrations. Throws on failure.
    void migrate();

private:
    Database& db_;

    // Embedded schema — same content as sql/schema.sql.
    // Avoids file-system dependency in the Docker image.
    static const char* SCHEMA_SQL;
};

} // namespace checkin
