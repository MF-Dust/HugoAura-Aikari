#include <Aikari-Shared/infrastructure/loggerMacro.h>
#include <Aikari-Shared/utils/windows.h>
#include <Aikari-Shared/utils/windows/winString.h>
#include <format>
#include <memory>
#include <type_traits>
#include <vector>
#include <windows.h>

namespace winStringUtils = AikariShared::Utils::Windows::WinString;

namespace AikariShared::Utils::Windows::Service
{
    struct ServiceHandleDeleter
    {
        void operator()(SC_HANDLE handle) const
        {
            if (handle != nullptr)
            {
                CloseServiceHandle(handle);
            }
        }
    };

    using ServiceHandle =
        std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, ServiceHandleDeleter>;

    static std::string _serviceStateToString(DWORD state)
    {
        switch (state)
        {
            case SERVICE_STOPPED:
                return "stopped";
            case SERVICE_START_PENDING:
                return "start_pending";
            case SERVICE_STOP_PENDING:
                return "stop_pending";
            case SERVICE_RUNNING:
                return "running";
            case SERVICE_CONTINUE_PENDING:
                return "continue_pending";
            case SERVICE_PAUSE_PENDING:
                return "pause_pending";
            case SERVICE_PAUSED:
                return "paused";
            default:
                return std::format("unknown({})", state);
        }
    }

    static std::string _startTypeToString(DWORD startType)
    {
        switch (startType)
        {
            case SERVICE_AUTO_START:
                return "auto";
            case SERVICE_BOOT_START:
                return "boot";
            case SERVICE_DEMAND_START:
                return "manual";
            case SERVICE_DISABLED:
                return "disabled";
            case SERVICE_SYSTEM_START:
                return "system";
            default:
                return std::format("unknown({})", startType);
        }
    }

    ServiceQueryResult queryService(const std::string& serviceName)
    {
        ServiceQueryResult result = { .serviceName = serviceName };

        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (scm == nullptr)
        {
            LOG_WARN(
                "Failed to open SCM while querying {}: {}",
                serviceName,
                winStringUtils::parseDWORDResult(GetLastError())
            );
            return result;
        }

        ServiceHandle closeScm(scm);

        const auto serviceNameW = winStringUtils::StringToWstring(serviceName);
        SC_HANDLE service = OpenServiceW(
            scm, serviceNameW.c_str(), SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS
        );
        if (service == nullptr)
        {
            const DWORD err = GetLastError();
            if (err != ERROR_SERVICE_DOES_NOT_EXIST)
            {
                LOG_WARN(
                    "Failed to open service {}: {}",
                    serviceName,
                    winStringUtils::parseDWORDResult(err)
                );
            }
            return result;
        }

        ServiceHandle closeService(service);

        result.exists = true;

        SERVICE_STATUS_PROCESS status = {};
        DWORD bytesNeeded = 0;
        if (QueryServiceStatusEx(
                service,
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&status),
                sizeof(status),
                &bytesNeeded
            ))
        {
            result.status = _serviceStateToString(status.dwCurrentState);
        }
        else
        {
            LOG_WARN(
                "Failed to query service status for {}: {}",
                serviceName,
                winStringUtils::parseDWORDResult(GetLastError())
            );
        }

        DWORD configBytesNeeded = 0;
        QueryServiceConfigW(service, nullptr, 0, &configBytesNeeded);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            std::vector<unsigned char> buffer(configBytesNeeded);
            auto* config =
                reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());
            if (QueryServiceConfigW(
                    service, config, configBytesNeeded, &configBytesNeeded
                ))
            {
                if (config->lpBinaryPathName != nullptr)
                {
                    result.binaryPath =
                        winStringUtils::WstringToString(config->lpBinaryPathName);
                }
                result.startType = _startTypeToString(config->dwStartType);
            }
            else
            {
                LOG_WARN(
                    "Failed to query service config for {}: {}",
                    serviceName,
                    winStringUtils::parseDWORDResult(GetLastError())
                );
            }
        }

        return result;
    }
}  // namespace AikariShared::Utils::Windows::Service
