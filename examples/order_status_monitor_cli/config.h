#pragma once

#include <new>
#include <rmqt_endpoint.h>
#include <rmqt_securityparameters.h>
#include <rmqt_exchangetype.h>
#include <rmqt_credentials.h>

#include <bsl_string.h>
#include <bsl_vector.h>
#include <optional>
#include <cstdint>

namespace osmcli {

struct ConnectionConfig {
    bsl::string host{"localhost"};
    std::uint16_t port{5672};
    bsl::string vhost{"/"};
    bsl::string username{"guest"};
    bsl::string password{"guest"};

    bsl::string exchange;         // topic/publish exchange
    bsl::string directExchange;   // direct exchange
    bsl::string queueName{"order_status_monitor"};
    bsl::vector<bsl::string> topicBindings;
    bsl::vector<bsl::string> directBindings;

    // Consumer tuning
    std::uint16_t prefetch{50};
    std::uint32_t heartbeatMs{60000};
    std::uint32_t connectionTimeoutMs{10000};

    // Declare flags
    bool durable{true};
    bool autoDelete{false};

    // Runtime control
    std::uint32_t runSeconds{0};  // 0 = run until signal

    // TLS (optional)
    bool useTls{false};
    bsl::string caCertPath;
    bsl::string clientCertPath;
    bsl::string clientKeyPath;
    BloombergLP::rmqt::SecurityParameters::Verification verifyMode{
        BloombergLP::rmqt::SecurityParameters::VERIFY_SERVER};

    // Build endpoint/credentials/security params
    bsl::shared_ptr<BloombergLP::rmqt::Endpoint> endpoint() const;
    bsl::shared_ptr<BloombergLP::rmqt::Credentials> credentials() const;
    std::optional<BloombergLP::rmqt::SecurityParameters> security() const;
};

// Load config from env, cli, and optional JSON file (Glaze)
ConnectionConfig loadConfig(int argc, char** argv);

}  // namespace osmcli
