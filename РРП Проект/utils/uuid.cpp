#include "utils/uuid.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace checkin::utils {

std::string generateUUID() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;

    uint64_t a = dist(rng);
    uint64_t b = dist(rng);

    // Set version 4 and variant bits
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (a >> 32);
    oss << '-' << std::setw(4) << ((a >> 16) & 0xFFFF);
    oss << '-' << std::setw(4) << (a & 0xFFFF);
    oss << '-' << std::setw(4) << (b >> 48);
    oss << '-' << std::setw(12) << (b & 0x0000FFFFFFFFFFFFULL);
    return oss.str();
}

} // namespace checkin::utils
