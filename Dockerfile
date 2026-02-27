# ─── Build stage ──────────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git pkg-config \
    libpqxx-dev \
    librdkafka-dev \
    libboost-dev \
    libssl-dev \
    zlib1g-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel $(nproc)

# ─── Runtime stage ────────────────────────────────────────────────────────────
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libpqxx-8.0 \
    librdkafka++1 \
    libssl3 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/build/checkin-service .
COPY sql/schema.sql .

# Default env (override in docker-compose)
ENV PORT=8000 \
    DB_HOST=postgres \
    DB_PORT=5432 \
    DB_NAME=checkin \
    DB_USER=checkin \
    DB_PASSWORD=checkin \
    KAFKA_BROKERS=kafka:9092 \
    KAFKA_GROUP_ID=checkin-service \
    TOPIC_FLIGHTS=flights.events \
    TOPIC_TICKETS=tickets.events \
    TOPIC_CHECKIN=checkin.events

EXPOSE 8000

ENTRYPOINT ["./checkin-service"]
