#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace AikariPLS::Components::Infrastructure::SeewoService
{
    struct SeewoServiceProfile
    {
        std::optional<std::filesystem::path> installRoot;
        std::optional<std::filesystem::path> componentRoot;
        std::optional<std::string> version;
        std::optional<std::filesystem::path> seewoCorePath;
        std::optional<std::filesystem::path> proxyLayerServicePath;
        std::string brokerHost = "iot-broker.seewo.com";
        int brokerPort = 8883;
        std::vector<std::string> legacyBrokerHosts = {
            "iot-broker-mis.seewo.com"
        };
        std::string seewoCoreServiceStatus = "not_found";
        std::string proxyLayerServiceStatus = "not_found";
    };

    struct SeewoServiceDiscoveryResult
    {
        SeewoServiceProfile profile;
        std::vector<std::string> warnings;
    };

    SeewoServiceDiscoveryResult discoverSeewoServiceProfile();
}  // namespace AikariPLS::Components::Infrastructure::SeewoService
