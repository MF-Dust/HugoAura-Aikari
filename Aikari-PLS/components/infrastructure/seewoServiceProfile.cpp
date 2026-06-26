#include <Aikari-PLS-Private/types/components/seewoServiceProfile.h>

#include <Aikari-Shared/infrastructure/loggerMacro.h>
#include <Aikari-Shared/utils/string.h>
#include <Aikari-Shared/utils/windows.h>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>

namespace AikariPLS::Components::Infrastructure::SeewoService
{
    static constexpr const char* DEFAULT_BROKER_HOST = "iot-broker.seewo.com";
    static constexpr const char* LEGACY_BROKER_HOST =
        "iot-broker-mis.seewo.com";

    static std::filesystem::path _stripServiceBinaryArgs(
        const std::string& binaryPath
    )
    {
        std::string trimmed = AikariShared::Utils::String::trim(binaryPath);
        if (trimmed.empty())
        {
            return {};
        }

        if (trimmed.front() == '"')
        {
            const auto closingQuote = trimmed.find('"', 1);
            if (closingQuote != std::string::npos)
            {
                return std::filesystem::path(
                    trimmed.substr(1, closingQuote - 1)
                );
            }
        }

        const auto exePos = trimmed.find(".exe");
        if (exePos != std::string::npos)
        {
            return std::filesystem::path(trimmed.substr(0, exePos + 4));
        }

        return std::filesystem::path(trimmed);
    }

    static std::optional<std::string> _versionFromComponentRoot(
        const std::filesystem::path& componentRoot
    )
    {
        const std::string filename = componentRoot.filename().string();
        const std::regex versionRegex(R"(SeewoService_([0-9]+(?:\.[0-9]+)*))");
        std::smatch matches;
        if (std::regex_search(filename, matches, versionRegex) &&
            matches.size() > 1)
        {
            return matches[1].str();
        }
        return std::nullopt;
    }

    static std::optional<std::string> _versionFromPackageJson(
        const std::filesystem::path& componentRoot
    )
    {
        const auto packagePath = componentRoot / "app-package.json";
        if (!std::filesystem::exists(packagePath))
        {
            return std::nullopt;
        }

        try
        {
            std::ifstream input(packagePath);
            auto parsed = nlohmann::json::parse(input);
            for (const auto& key : { "version", "Version", "appVersion" })
            {
                if (parsed.contains(key) && parsed[key].is_string())
                {
                    return parsed[key].get<std::string>();
                }
            }
        }
        catch (const std::exception& err)
        {
            LOG_WARN(
                "Failed to parse Seewo app-package.json at {}: {}",
                packagePath.string(),
                err.what()
            );
        }

        return std::nullopt;
    }

    static std::string _readPrintableStrings(
        const std::filesystem::path& filePath
    )
    {
        std::ifstream input(filePath, std::ios::binary);
        if (!input)
        {
            return {};
        }

        std::string content(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>()
        );
        std::string printable;
        printable.reserve(content.size());

        for (size_t idx = 0; idx < content.size(); ++idx)
        {
            const unsigned char ch = static_cast<unsigned char>(content[idx]);
            if (idx + 1 < content.size() && content[idx + 1] == '\0' &&
                ch >= 32 && ch <= 126)
            {
                printable.push_back(static_cast<char>(ch));
                ++idx;
            }
            else if (ch >= 32 && ch <= 126)
            {
                printable.push_back(static_cast<char>(ch));
            }
            else
            {
                printable.push_back('\n');
            }
        }

        return printable;
    }

    static std::optional<std::string> _discoverBrokerFromDll(
        const std::filesystem::path& iotMqttDll
    )
    {
        if (!std::filesystem::exists(iotMqttDll))
        {
            return std::nullopt;
        }

        const auto strings = _readPrintableStrings(iotMqttDll);
        const std::regex brokerRegex(R"(iot-broker[-a-zA-Z0-9.]*\.seewo\.com)");
        std::sregex_iterator cur(strings.begin(), strings.end(), brokerRegex);
        std::sregex_iterator end;
        std::vector<std::string> matches;

        for (; cur != end; ++cur)
        {
            const std::string host = cur->str();
            if (std::ranges::find(matches, host) == matches.end())
            {
                matches.emplace_back(host);
            }
        }

        if (matches.empty())
        {
            return std::nullopt;
        }

        auto preferred = std::ranges::find(matches, DEFAULT_BROKER_HOST);
        if (preferred != matches.end())
        {
            return *preferred;
        }

        return matches.front();
    }

    SeewoServiceDiscoveryResult discoverSeewoServiceProfile()
    {
        SeewoServiceDiscoveryResult result;
        auto& profile = result.profile;

        auto coreService =
            AikariShared::Utils::Windows::Service::queryService(
                "SeewoCoreService"
            );
        auto proxyService =
            AikariShared::Utils::Windows::Service::queryService(
                "SeewoProxyLayerService"
            );

        if (coreService.exists)
        {
            profile.seewoCoreServiceStatus = coreService.status;
            const auto binaryPath =
                _stripServiceBinaryArgs(coreService.binaryPath);
            if (!binaryPath.empty())
            {
                profile.seewoCorePath = binaryPath;
                profile.componentRoot = binaryPath.parent_path().parent_path();
                profile.installRoot = profile.componentRoot->parent_path();
            }
        }
        else
        {
            result.warnings.emplace_back("SeewoCoreService not found");
        }

        if (proxyService.exists)
        {
            profile.proxyLayerServiceStatus = proxyService.status;
            const auto binaryPath =
                _stripServiceBinaryArgs(proxyService.binaryPath);
            if (!binaryPath.empty())
            {
                profile.proxyLayerServicePath = binaryPath;
                if (!profile.componentRoot.has_value())
                {
                    profile.componentRoot =
                        binaryPath.parent_path().parent_path();
                    profile.installRoot = profile.componentRoot->parent_path();
                }
            }
        }
        else
        {
            result.warnings.emplace_back("SeewoProxyLayerService not found");
        }

        if (profile.componentRoot.has_value())
        {
            auto version = _versionFromComponentRoot(*profile.componentRoot);
            if (!version.has_value())
            {
                version = _versionFromPackageJson(*profile.componentRoot);
            }
            profile.version = version;

            if (!profile.proxyLayerServicePath.has_value())
            {
                const auto proxyPath =
                    *profile.componentRoot / "ProxyLayerService" /
                    "proxyLayerService.exe";
                if (std::filesystem::exists(proxyPath))
                {
                    profile.proxyLayerServicePath = proxyPath;
                }
            }

            const auto mqttDll =
                *profile.componentRoot / "SeewoCore" / "iot_mqtt.dll";
            auto discoveredBroker = _discoverBrokerFromDll(mqttDll);
            if (discoveredBroker.has_value())
            {
                profile.brokerHost = *discoveredBroker;
            }
            else
            {
                result.warnings.emplace_back(
                    "Broker host not found in iot_mqtt.dll; using default"
                );
            }
        }
        else
        {
            result.warnings.emplace_back(
                "Unable to locate SeewoService component root; using fallback "
                "broker"
            );
        }

        if (profile.brokerHost != LEGACY_BROKER_HOST &&
            std::ranges::find(
                profile.legacyBrokerHosts, std::string(LEGACY_BROKER_HOST)
            ) == profile.legacyBrokerHosts.end())
        {
            profile.legacyBrokerHosts.emplace_back(LEGACY_BROKER_HOST);
        }

        return result;
    }
}  // namespace AikariPLS::Components::Infrastructure::SeewoService
