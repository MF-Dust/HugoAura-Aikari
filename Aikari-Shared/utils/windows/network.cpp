#include <Aikari-Shared/infrastructure/loggerMacro.h>
#include <Aikari-Shared/utils/registry.h>
#include <Aikari-Shared/utils/string.h>
#include <Aikari-Shared/utils/windows.h>  // self
#include <Aikari-Shared/utils/windows/winString.h>
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <set>
#include <sstream>
#include <windows.h>

namespace registryUtils = AikariShared::Utils::WindowsRegistry;
namespace winStringUtils = AikariShared::Utils::Windows::WinString;

namespace AikariShared::Utils::Windows::Network
{
    // ↓ some constants
    const wchar_t* networkDBRegKeyPath =
        L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters";

    const wchar_t* networkDBRegKeyEntryName = L"DataBasePath";

    const wchar_t* defaultHostPath = L"%SystemRoot%\\System32\\drivers\\etc";
    const wchar_t* defaultHostPathExpanded =
        L"C:\\Windows\\System32\\drivers\\etc";
    // ↑ end of constants

    static std::filesystem::path _getHostsFilePath()
    {
        std::wstring hostDir;
        try
        {
            hostDir = registryUtils::getRegSzValue(
                HKEY_LOCAL_MACHINE,
                std::wstring(networkDBRegKeyPath),
                std::wstring(networkDBRegKeyEntryName)
            );
        }
        catch (const std::exception& err)
        {
            LOG_ERROR(
                "Unexpected error occurred querying hostPath: {}", err.what()
            );
            LOG_ERROR("Assuming to use the default path.");
            hostDir = std::wstring(defaultHostPath);
        }

        try
        {
            hostDir = winStringUtils::expandEnvStr(hostDir);
            LOG_DEBUG(
                "Successfully parsed host path, result: {}",
                winStringUtils::WstringToString(hostDir)
            );
        }
        catch (const std::exception& err)
        {
            LOG_ERROR(
                "Unexpected error occurred expanding env vars in hostPath: {}",
                err.what()
            );
            LOG_ERROR("Assuming to use the default expanded path.");
            hostDir = std::wstring(defaultHostPathExpanded);
        }

        return std::filesystem::path(winStringUtils::WstringToString(hostDir)) /
               "hosts";
    }

    static std::vector<std::string> _extractHostnames(
        const std::string& hostsLine
    )
    {
        auto hashPos = hostsLine.find('#');
        std::string activePart = hashPos == std::string::npos
                                     ? hostsLine
                                     : hostsLine.substr(0, hashPos);
        std::istringstream iss(activePart);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token)
        {
            tokens.emplace_back(std::move(token));
        }

        if (tokens.size() <= 1)
        {
            return {};
        }

        return { tokens.begin() + 1, tokens.end() };
    }

    isSeewoCoreNeedToBeKill ensureHostKeyExists(const std::string& hostLine)
    {
        constexpr int SW_CORE_DONT_NEED_KILL = false;
        constexpr int SW_CORE_NEED_KILL = true;

        std::filesystem::path hostPathIns = _getHostsFilePath();

        if (!std::filesystem::exists(hostPathIns))
        {
            LOG_ERROR("Failed to write host: hosts file not found.");
            return SW_CORE_DONT_NEED_KILL;
        }

        isSeewoCoreNeedToBeKill result = SW_CORE_NEED_KILL;

        {
            std::ifstream hostsFile(hostPathIns);

            if (!hostsFile)
            {
                LOG_ERROR("Failed to write host: hosts file failed to open.");
                return SW_CORE_DONT_NEED_KILL;
            }

            std::string perLine;

            while (std::getline(hostsFile, perLine))
            {
                if (perLine.find(hostLine) != std::string::npos)
                {
                    LOG_INFO(
                        "Found target line in hosts file, no need to kill "
                        "swCore."
                    );
                    result = SW_CORE_DONT_NEED_KILL;
                    break;
                }
            };

            if (result == SW_CORE_DONT_NEED_KILL)
            {
                hostsFile.close();
                return result;
            }

            // not found
            hostsFile.close();

            std::ofstream hostsFileWrite(hostPathIns, std::ios::app);
            if (!hostsFileWrite)
            {
                LOG_ERROR(
                    "Failed to get write access to hosts file, r u running "
                    "Aikari "
                    "with Administrator privilege?"
                );
                result = SW_CORE_DONT_NEED_KILL;
                return result;
            }

            hostsFileWrite << "\n\n" + hostLine;

            hostsFileWrite.close();

            LOG_INFO("Successfully appended `{}` into hosts file;", hostLine);
        }

        return result;
    }

    isSeewoCoreNeedToBeKill ensureHostKeysExist(
        const std::string& targetAddress,
        const std::vector<std::string>& hostnames,
        const std::string& marker
    )
    {
        constexpr int SW_CORE_DONT_NEED_KILL = false;
        constexpr int SW_CORE_NEED_KILL = true;

        if (targetAddress.empty() || hostnames.empty())
        {
            LOG_WARN("No hosts entries requested, skipping hosts update.");
            return SW_CORE_DONT_NEED_KILL;
        }

        std::vector<std::string> normalizedHosts;
        for (const auto& hostname : hostnames)
        {
            const auto trimmed = AikariShared::Utils::String::trim(hostname);
            if (!trimmed.empty() &&
                std::ranges::find(normalizedHosts, trimmed) ==
                    normalizedHosts.end())
            {
                normalizedHosts.emplace_back(trimmed);
            }
        }

        if (normalizedHosts.empty())
        {
            LOG_WARN("Hosts entries were empty after normalization.");
            return SW_CORE_DONT_NEED_KILL;
        }

        const std::filesystem::path hostPathIns = _getHostsFilePath();
        if (!std::filesystem::exists(hostPathIns))
        {
            LOG_ERROR("Failed to write host: hosts file not found.");
            return SW_CORE_DONT_NEED_KILL;
        }

        std::ifstream hostsFile(hostPathIns);
        if (!hostsFile)
        {
            LOG_ERROR("Failed to write host: hosts file failed to open.");
            return SW_CORE_DONT_NEED_KILL;
        }

        std::set<std::string> requestedHostSet(
            normalizedHosts.begin(), normalizedHosts.end()
        );
        std::set<std::string> seenHosts;
        std::vector<std::string> preservedLines;
        std::string perLine;
        bool changed = false;

        while (std::getline(hostsFile, perLine))
        {
            bool shouldDrop = false;
            if (perLine.find(marker) != std::string::npos)
            {
                for (const auto& host : _extractHostnames(perLine))
                {
                    if (requestedHostSet.contains(host))
                    {
                        const std::string expectedLine =
                            std::format("{} {} {}", targetAddress, host, marker);
                        if (AikariShared::Utils::String::trim(perLine) ==
                            expectedLine)
                        {
                            seenHosts.emplace(host);
                        }
                        else
                        {
                            shouldDrop = true;
                            changed = true;
                        }
                        break;
                    }
                }
            }

            if (!shouldDrop)
            {
                preservedLines.emplace_back(std::move(perLine));
            }
        }
        hostsFile.close();

        for (const auto& host : normalizedHosts)
        {
            if (seenHosts.contains(host))
            {
                continue;
            }

            const std::string expectedLine =
                std::format("{} {} {}", targetAddress, host, marker);
            if (std::ranges::find(preservedLines, expectedLine) ==
                preservedLines.end())
            {
                preservedLines.emplace_back(expectedLine);
                changed = true;
            }
        }

        if (!changed)
        {
            LOG_INFO("Hosts file already contains requested Aikari entries.");
            return SW_CORE_DONT_NEED_KILL;
        }

        std::ofstream hostsFileWrite(
            hostPathIns, std::ios::out | std::ios::trunc
        );
        if (!hostsFileWrite)
        {
            LOG_ERROR(
                "Failed to get write access to hosts file, r u running Aikari "
                "with Administrator privilege?"
            );
            return SW_CORE_DONT_NEED_KILL;
        }

        for (size_t idx = 0; idx < preservedLines.size(); ++idx)
        {
            hostsFileWrite << preservedLines[idx];
            if (idx + 1 < preservedLines.size())
            {
                hostsFileWrite << "\n";
            }
        }
        hostsFileWrite.close();

        LOG_INFO(
            "Successfully upserted {} Aikari hosts entries.",
            normalizedHosts.size()
        );
        return SW_CORE_NEED_KILL;
    }
}  // namespace AikariShared::Utils::Windows::Network
