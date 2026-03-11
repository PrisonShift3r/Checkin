#pragma once
#include <string>

namespace checkin::utils {

std::string generateUUID();

// Prefix helpers
inline std::string newCheckinId() { return "CHK-" + generateUUID(); }

} // namespace checkin::utils
