#pragma once

#include <string>
#include <vector>

#include "./windows/rc.hpp"

namespace AikariShared::Utils::Windows
{
    namespace Network
    {
        typedef bool isSeewoCoreNeedToBeKill;
        isSeewoCoreNeedToBeKill ensureHostKeyExists(
            const std::string& hostLine
        );
        isSeewoCoreNeedToBeKill ensureHostKeysExist(
            const std::string& targetAddress,
            const std::vector<std::string>& hostnames,
            const std::string& marker
        );
    }  // namespace Network

    namespace Service
    {
        struct ServiceQueryResult
        {
            bool exists = false;
            std::string serviceName;
            std::string binaryPath;
            std::string status = "unknown";
            std::string startType = "unknown";
        };

        ServiceQueryResult queryService(const std::string& serviceName);
    }  // namespace Service

    namespace Process
    {
        void killProcessByName(const std::string& procNameASCII);
    }

    namespace RC
    {
        // Implements in windows/rc.hpp
    }  // namespace RC
}  // namespace AikariShared::Utils::Windows
