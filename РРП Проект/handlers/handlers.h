#pragma once
#include "services/checkin_service.h"
#include <crow.h>

namespace checkin::handlers {

void registerCheckinRoutes(crow::SimpleApp& app, CheckinService& service);
void registerHealthRoute(crow::SimpleApp& app);

} // namespace checkin::handlers
