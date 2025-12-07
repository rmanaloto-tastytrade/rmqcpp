#include "config.h"

#include <CLI/CLI.hpp>
#include <glaze/glaze.hpp>
#include <rmqt_plaincredentials.h>
#include <rmqt_simpleendpoint.h>
#include <rmqt_mutualsecurityparameters.h>

#include <bsl_string.h>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace osmcli {

struct JsonConfig {
    bsl::string host;
    std::uint16_t port{5672};
    bsl::string vhost;
    bsl::string username;
    bsl::string password;
    bsl::string exchange;
    bsl::string directExchange;
    bsl::string queueName;
    bsl::vector<bsl::string> topicBindings;
    bsl::vector<bsl::string> directBindings;
    std::uint16_t prefetch{50};
    std::uint32_t heartbeatMs{60000};
    std::uint32_t connectionTimeoutMs{10000};
    bool durable{true};
    bool autoDelete{false};
    std::uint32_t runSeconds{0};
    bool useTls{false};
    bsl::string caCertPath;
    bsl::string clientCertPath;
    bsl::string clientKeyPath;
    bsl::string verifyMode;  // "VERIFY_SERVER" or "MUTUAL"
};

}  // namespace osmcli

template <>
struct glz::meta<osmcli::JsonConfig> {
    using T = osmcli::JsonConfig;
    static constexpr auto value = object(
        "host", &T::host,
        "port", &T::port,
        "vhost", &T::vhost,
        "username", &T::username,
        "password", &T::password,
        "exchange", &T::exchange,
        "direct_exchange", &T::directExchange,
        "queue", &T::queueName,
        "topic_bindings", &T::topicBindings,
        "direct_bindings", &T::directBindings,
        "prefetch", &T::prefetch,
        "heartbeat_ms", &T::heartbeatMs,
        "connection_timeout_ms", &T::connectionTimeoutMs,
        "durable", &T::durable,
        "auto_delete", &T::autoDelete,
        "run_seconds", &T::runSeconds,
        "use_tls", &T::useTls,
        "ca_cert", &T::caCertPath,
        "client_cert", &T::clientCertPath,
        "client_key", &T::clientKeyPath,
        "verify_mode", &T::verifyMode);
};

namespace osmcli {

BloombergLP::rmqt::SecurityParameters::Verification parseVerifyMode(const bsl::string& mode)
{
    if (mode == "MUTUAL") return BloombergLP::rmqt::SecurityParameters::MUTUAL;
    return BloombergLP::rmqt::SecurityParameters::VERIFY_SERVER;
}

void applyEnvOverrides(ConnectionConfig& cfg)
{
    if (const char* v = std::getenv("MQ_HOST")) cfg.host = v;
    if (const char* v = std::getenv("MQ_PORT")) cfg.port = static_cast<std::uint16_t>(std::atoi(v));
    if (const char* v = std::getenv("MQ_VHOST")) cfg.vhost = v;
    if (const char* v = std::getenv("MQ_USERNAME")) cfg.username = v;
    if (const char* v = std::getenv("MQ_PASSWORD")) cfg.password = v;
    if (const char* v = std::getenv("MQ_EXCHANGE_NAME")) cfg.exchange = v;
    if (const char* v = std::getenv("MQ_DIRECT_EXCHANGE_NAME")) cfg.directExchange = v;
    if (const char* v = std::getenv("MQ_QUEUE_NAME")) cfg.queueName = v;
}

bsl::shared_ptr<BloombergLP::rmqt::Endpoint> ConnectionConfig::endpoint() const
{
    using BloombergLP::rmqt::SimpleEndpoint;
    return bsl::make_shared<SimpleEndpoint>(host, vhost, port);
}

bsl::shared_ptr<BloombergLP::rmqt::Credentials>
ConnectionConfig::credentials() const
{
    return bsl::make_shared<BloombergLP::rmqt::PlainCredentials>(username,
                                                                 password);
}

std::optional<BloombergLP::rmqt::SecurityParameters> ConnectionConfig::security() const
{
    if (!useTls) return std::nullopt;
    if (!clientCertPath.empty() && !clientKeyPath.empty()) {
        return BloombergLP::rmqt::MutualSecurityParameters(
            caCertPath, clientCertPath, clientKeyPath);
    }
    return BloombergLP::rmqt::SecurityParameters(caCertPath);
}

ConnectionConfig loadConfig(int argc, char** argv)
{
    ConnectionConfig cfg;
    bsl::string configPath;

    CLI::App app{"rmq_order_status_monitor_cli"};
    app.add_option("--config", configPath, "JSON config file");
    app.add_option("--host", cfg.host, "MQ host");
    app.add_option("--port", cfg.port, "MQ port");
    app.add_option("--vhost", cfg.vhost, "MQ vhost");
    app.add_option("--username", cfg.username, "MQ username");
    app.add_option("--password", cfg.password, "MQ password");
    app.add_option("--exchange", cfg.exchange, "Topic exchange");
    app.add_option("--direct-exchange", cfg.directExchange, "Direct exchange");
    app.add_option("--queue", cfg.queueName, "Queue name");
    app.add_option("--topic-binding", cfg.topicBindings, "Topic binding (repeatable)");
    app.add_option("--direct-binding", cfg.directBindings, "Direct binding (repeatable)");
    app.add_option("--prefetch", cfg.prefetch, "Prefetch for consumer");
    app.add_option("--heartbeat-ms", cfg.heartbeatMs, "Heartbeat interval ms");
    app.add_option("--conn-timeout-ms", cfg.connectionTimeoutMs, "Connection timeout ms");
    app.add_flag("--durable", cfg.durable, "Declare durable queue");
    app.add_flag("--auto-delete", cfg.autoDelete, "Auto-delete queue");
    app.add_flag("--tls", cfg.useTls, "Enable TLS");
    app.add_option("--ca-cert", cfg.caCertPath, "CA certificate path");
    app.add_option("--client-cert", cfg.clientCertPath, "Client certificate path");
    app.add_option("--client-key", cfg.clientKeyPath, "Client key path");
    bsl::string verifyModeStr = "VERIFY_SERVER";
    app.add_option("--verify-mode", verifyModeStr, "TLS verify mode (VERIFY_SERVER|MUTUAL)");
    app.add_option("--run-seconds", cfg.runSeconds, "Run duration seconds (0 = until signal)");

    try {
        app.parse(argc, argv);
    }
    catch (const CLI::CallForHelp&) {
        std::cout << app.help() << std::endl;
        std::exit(0);
    }

    // Load JSON if provided
    if (!configPath.empty()) {
        std::ifstream in(configPath.c_str(), std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open config file: " + configPath);
        }
        std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        JsonConfig jc;
        auto ec = ::glz::read_json(jc, json);
        if (ec) {
            throw std::runtime_error(
                "Failed to parse config JSON: " + ::glz::format_error(ec, json));
        }
        if (!jc.host.empty()) cfg.host = jc.host;
        if (jc.port) cfg.port = jc.port;
        if (!jc.vhost.empty()) cfg.vhost = jc.vhost;
        if (!jc.username.empty()) cfg.username = jc.username;
        if (!jc.password.empty()) cfg.password = jc.password;
        if (!jc.exchange.empty()) cfg.exchange = jc.exchange;
        if (!jc.directExchange.empty()) cfg.directExchange = jc.directExchange;
        if (!jc.queueName.empty()) cfg.queueName = jc.queueName;
        if (!jc.topicBindings.empty()) cfg.topicBindings = jc.topicBindings;
        if (!jc.directBindings.empty()) cfg.directBindings = jc.directBindings;
        cfg.prefetch = jc.prefetch;
        cfg.heartbeatMs = jc.heartbeatMs;
        cfg.connectionTimeoutMs = jc.connectionTimeoutMs;
        cfg.durable = jc.durable;
        cfg.autoDelete = jc.autoDelete;
        cfg.runSeconds = jc.runSeconds;
        cfg.useTls = jc.useTls;
        if (!jc.caCertPath.empty()) cfg.caCertPath = jc.caCertPath;
        if (!jc.clientCertPath.empty()) cfg.clientCertPath = jc.clientCertPath;
        if (!jc.clientKeyPath.empty()) cfg.clientKeyPath = jc.clientKeyPath;
        if (!jc.verifyMode.empty()) cfg.verifyMode = parseVerifyMode(jc.verifyMode);
    }

    // Apply env overrides
    applyEnvOverrides(cfg);

    // Apply verify mode if provided on CLI
    cfg.verifyMode = parseVerifyMode(verifyModeStr);

    // Defaults for bindings if empty
    if (cfg.topicBindings.empty()) {
        cfg.topicBindings.push_back("accounts.*.orders.*.*");
    }

    if (cfg.exchange.empty()) {
        throw std::runtime_error("exchange is required (set --exchange or MQ_EXCHANGE_NAME)");
    }
    if (cfg.directExchange.empty()) {
        throw std::runtime_error("direct exchange is required (set --direct-exchange or MQ_DIRECT_EXCHANGE_NAME)");
    }

    return cfg;
}

}  // namespace osmcli
