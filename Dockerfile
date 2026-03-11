# ─── Build stage ──────────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Только то, что нужно системе + то, что не умеет FetchContent:
#   libpq-dev   — C-клиент PostgreSQL (libpqxx собирается поверх него)
#   libssl-dev  — нужен librdkafka
#   zlib1g-dev  — нужен librdkafka
#   libzstd-dev — нужен librdkafka (сжатие)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    ca-certificates \
    libasio-dev \
    libpq-dev \
    libssl-dev \
    zlib1g-dev \
    libzstd-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

# FetchContent скачает: Crow, Asio, spdlog, nlohmann/json, libpqxx, librdkafka
RUN cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DUSE_SYSTEM_DEPS=OFF \
    && cmake --build build --parallel "$(nproc)"

# ─── Runtime stage ────────────────────────────────────────────────────────────
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Только runtime-библиотеки (без -dev заголовков)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpq5 \
    libssl3 \
    zlib1g \
    libzstd1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/build/checkin-service .
COPY --from=builder /build/build/_deps/librdkafka-build/src/librdkafka.so* /usr/local/lib/
COPY --from=builder /build/build/_deps/librdkafka-build/src-cpp/librdkafka++.so* /usr/local/lib/
ENV LD_LIBRARY_PATH=/usr/local/lib:${LD_LIBRARY_PATH}

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
