# ✈️ Check-in Service

Микросервис регистрации пассажиров аэропорта. Часть общего Airport Simulation проекта.

## Стек

| Компонент | Технология |
|-----------|-----------|
| HTTP      | [Crow](https://crowcpp.org) (C++17, header-only) |
| База данных | PostgreSQL + [libpqxx](https://pqxx.org/) |
| Очередь сообщений | Apache Kafka + [librdkafka](https://github.com/confluentinc/librdkafka) |
| JSON      | [nlohmann/json](https://github.com/nlohmann/json) |
| Логирование | [spdlog](https://github.com/gabime/spdlog) |
| Сборка    | CMake 3.16+ |

---

## Архитектура

```
flights.events ──┐
                 ├──► KafkaConsumer ──► handleKafkaMessage()
tickets.events ──┘         │
                            ├──► CacheService (in-memory, thread-safe)
                            │       ├── flight_cache (FlightStatus)
                            │       └── ticket_cache (TicketStatus)
                            │
                            └──► CheckinService.runAutoCheckin()  ← RegistrationOpen
                                        │
                            REST ──► CheckinService.startCheckin() ← Manual

CheckinService
    ├── DB: INSERT checkins (status=success)
    ├── DB: INSERT checkin_outbox (transactional outbox)  ← same TX
    └── OutboxPublisher (background) ──► Kafka checkin.events
```

### Ключевые решения

**Устойчивость к падениям** — сервис не вызывает Flights/Tickets по REST.  
Вместо этого поддерживаются `flight_cache` и `ticket_cache`, наполняемые из Kafka.  
Если Flights или Tickets упадут — регистрация продолжает работать по накопленным данным.

**Идемпотентность** двойная:
- `processed_events` — дедупликация Kafka-событий по `eventId`
- `UNIQUE INDEX checkins(ticket_id) WHERE status = 'success'` — один успешный checkin на билет

**Transactional Outbox** — событие `checkin.completed` записывается в `checkin_outbox`  
в той же транзакции, что и строка `checkins`. Фоновый `OutboxPublisher` доставляет  
его в Kafka. Это гарантирует что событие не потеряется при падении Kafka.

---

## API

```
GET  /health                  — healthcheck
GET  /v1/checkins             — список регистраций (?flightId= &passengerId= &status=)
GET  /v1/checkins/{checkinId} — конкретная регистрация
POST /v1/checkins/start       — ручная регистрация оператором
```

### POST /v1/checkins/start

```json
{
  "flightId":    "FL123",
  "passengerId": "PAX-uuid",
  "ticketId":    "TCK-uuid"
}
```

**Ответы:**
| Код | Описание |
|-----|----------|
| 201 | Checkin создан успешно |
| 400 | Ошибка валидации |
| 409 | `registration_closed` / `ticket_inactive` / `already_checked_in` |
| 500 | Внутренняя ошибка |

---

## Kafka

### Потребляет

| Topic | Тип события | Действие |
|-------|-------------|---------|
| `flights.events` | `flight.status.changed` (newStatus=RegistrationOpen) | Запуск авто-регистрации |
| `flights.events` | `flight.status.changed` (любой) | Обновление `flight_cache` |
| `tickets.events` | `ticket.bought` | Добавить в `ticket_cache` (active) |
| `tickets.events` | `ticket.refunded` | Обновить статус в `ticket_cache` |
| `tickets.events` | `ticket.bumped` | Обновить статус в `ticket_cache` |

### Публикует

**Topic:** `checkin.events`  
**Ключ:** `flightId`

```json
{
  "checkinId":   "CHK-uuid",
  "flightId":    "FL123",
  "passengerId": "PAX-uuid",
  "ticketId":    "TCK-uuid",
  "status":      "success",
  "simTime":     "2025-01-01T10:00:00Z"
}
```

---

## Запуск (Docker)

```bash
# Поднять весь стек
docker-compose up --build

# Проверить healthcheck
curl http://localhost:8006/health

# Ручная регистрация
curl -X POST http://localhost:8006/v1/checkins/start \
  -H 'Content-Type: application/json' \
  -d '{"flightId":"FL123","passengerId":"PAX-1","ticketId":"TCK-1"}'

# Список регистраций по рейсу
curl "http://localhost:8006/v1/checkins?flightId=FL123"
```

## Сборка без Docker

```bash
# Установить зависимости (Ubuntu/Debian)
sudo apt install cmake build-essential libpqxx-dev librdkafka-dev \
                 libboost-dev libssl-dev

# Сборка
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Запуск
DB_HOST=localhost DB_USER=checkin DB_PASSWORD=checkin \
KAFKA_BROKERS=localhost:9092 ./build/checkin-service
```

---

## Переменные окружения

| Переменная | Значение по умолчанию | Описание |
|------------|----------------------|----------|
| `PORT` | `8000` | HTTP порт |
| `DB_HOST` | `postgres` | PostgreSQL хост |
| `DB_PORT` | `5432` | PostgreSQL порт |
| `DB_NAME` | `checkin` | Имя БД |
| `DB_USER` | `checkin` | Пользователь БД |
| `DB_PASSWORD` | `checkin` | Пароль БД |
| `KAFKA_BROKERS` | `kafka:9092` | Адреса брокеров Kafka |
| `KAFKA_GROUP_ID` | `checkin-service` | Consumer group |
| `TOPIC_FLIGHTS` | `flights.events` | Топик рейсов |
| `TOPIC_TICKETS` | `tickets.events` | Топик билетов |
| `TOPIC_CHECKIN` | `checkin.events` | Топик регистраций (output) |
