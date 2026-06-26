#include "config.h"

namespace AikariPLS::Types::Config
{
    template <typename BasicJsonType>
    void to_json(
        BasicJsonType& target,
        const AikariPLS::Types::Config::PLSCompatConfig& origin
    )
    {
        target = nlohmann::json(
            { { "brokerHostOverride", origin.brokerHostOverride },
              { "brokerPortOverride", origin.brokerPortOverride },
              { "manageLegacyBrokerHosts", origin.manageLegacyBrokerHosts },
              { "forceRegenMqttCert", origin.forceRegenMqttCert } }
        );
    };

    template <typename BasicJsonType>
    void from_json(
        const BasicJsonType& origin,
        AikariPLS::Types::Config::PLSCompatConfig& target
    )
    {
        target.brokerHostOverride = origin.value("brokerHostOverride", "");
        target.brokerPortOverride = origin.value("brokerPortOverride", 8883);
        target.manageLegacyBrokerHosts =
            origin.value("manageLegacyBrokerHosts", true);
        target.forceRegenMqttCert =
            origin.value("forceRegenMqttCert", false);
    };

    template <typename BasicJsonType>
    void to_json(
        BasicJsonType& target, const AikariPLS::Types::Config::PLSConfig& origin
    )
    {
        target = nlohmann::json(
            { { "rules", origin.rules },
              { "module", origin.module },
              { "compat", origin.compat } }
        );
    };

    template <typename BasicJsonType>
    void from_json(
        const BasicJsonType& origin, AikariPLS::Types::Config::PLSConfig& target
    )
    {
        origin.at("rules").get_to(target.rules);
        origin.at("module").get_to(target.module);
        if (origin.contains("compat") && origin["compat"].is_object())
        {
            origin.at("compat").get_to(target.compat);
        }
        else
        {
            target.compat = {};
        }
    };
}  // namespace AikariPLS::Types::Config

namespace AikariPLS::Components::Config
{
    void PLSConfigManager::loadConfigImpl(nlohmann::json& configData)
    {
        auto configIns =
            configData.template get<AikariPLS::Types::Config::PLSConfig>();
        auto configInsPtr =
            std::make_shared<AikariPLS::Types::Config::PLSConfig>(configIns);
        // this->config = configInsPtr;
        std::atomic_store(&this->config, configInsPtr);
    }

    nlohmann::json PLSConfigManager::getStringifyConfigImpl()
    {
        nlohmann::json stringifyConfig = *std::atomic_load(&this->config);
        return stringifyConfig;
    }
}  // namespace AikariPLS::Components::Config
