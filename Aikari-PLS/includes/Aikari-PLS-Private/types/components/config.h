#pragma once

#include <Aikari-Shared/virtual/IConfigPayload.h>
#include <nlohmann/json.hpp>

namespace AikariPLS::Types::Config
{
    struct PLSCompatConfig
    {
        std::string brokerHostOverride;
        int brokerPortOverride = 8883;
        bool manageLegacyBrokerHosts = true;
        bool forceRegenMqttCert = false;
    };

    struct PLSConfig : public AikariShared::VirtualIns::IConfigPayload
    {
        nlohmann::json rules;
        std::string module;
        PLSCompatConfig compat;
    };
}  // namespace AikariPLS::Types::Config
