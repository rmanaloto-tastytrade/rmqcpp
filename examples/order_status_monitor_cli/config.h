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
    bsl::string queueName{"order_status_monitor"};  // kept for backward compat
    bsl::vector<bsl::string> queues;                // can consume multiple queues
    bsl::vector<bsl::string> topicBindings;
    bsl::vector<bsl::string> directBindings;
    bsl::string queuesTsvPath;  // optional TSV (name\t...) file to list queues
    bsl::string definitionsJsonPath; // optional rabbitmqadmin-ng definitions export JSON
    bsl::string logDir{"logs"};
    bsl::string logPrefix{"order_status_monitor"};
    bool enableHttpAdmin{false};
    bsl::string httpAdminUser;
    bsl::string httpAdminPassword;
    std::uint16_t httpAdminPort{15672};
    bool skipAutoDeleteQueues{false};
    bsl::string overviewCachePath;  // optional cache of /api/overview for offline mode

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

struct DefinitionsQueueInfo {
    bsl::string name;
    bsl::string vhost;
};

struct DefinitionsExchangeInfo {
    bsl::string name;
    bsl::string vhost;
};

struct DefinitionsBindingInfo {
    bsl::string vhost;
    bsl::string source;
    bsl::string destination;
    bsl::string destinationType;
    bsl::string routingKey;
};

struct DefinitionsSummary {
    bsl::vector<bsl::string> vhosts;
    bsl::vector<DefinitionsQueueInfo> queues;
    bsl::vector<DefinitionsExchangeInfo> exchanges;
    bsl::vector<DefinitionsBindingInfo> bindings;
};

// Load config from env, cli, and optional JSON file (Glaze)
ConnectionConfig loadConfig(int argc, char** argv);
DefinitionsSummary loadDefinitionsSummary(const bsl::string& path);

}  // namespace osmcli
