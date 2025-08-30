#include "src/server/include/astrocomm/server/services/communication_service.h"

int main() {
    // This should fail to compile and tell us which methods are missing
    auto service = std::make_unique<astrocomm::server::services::CommunicationServiceImpl>();
    return 0;
}
