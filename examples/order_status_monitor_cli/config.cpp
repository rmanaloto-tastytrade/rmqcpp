#include "config.h"

#include <CLI/CLI.hpp>
#include <glaze/glaze.hpp>
#include <rmqt_plaincredentials.h>
#include <rmqt_simpleendpoint.h>
#include <rmqt_mutualsecurityparameters.h>

#include <bsl_unordered_set.h>
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
    bsl::vector<bsl::string> queues;
    bsl::vector<bsl::string> queueWhitelist;
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
    bsl::string queuesTsvPath;
    bsl::string definitionsJsonPath;
    bsl::string logDir{"logs"};
    bsl::string logPrefix{"order_status_monitor"};
    bsl::string ballMinSeverity{"trace"};
    std::uint32_t threadPoolQueueDepth{200000};
    std::uint32_t connectionErrorThresholdMs{0};
    bool shuffleConnectionEndpoints{false};
    bool infiniteImmediateRetry{false};
    bsl::vector<bsl::string> ballNoisyPrefixes;
    int ballNoisyMinSeverity{300};
    bool enableOtel{false};
    bool enableOtelTraces{true};
    bool enableOtelMetrics{true};
    bool enableOtelExport{true};
    bsl::string otelProtocol{"grpc"};
    bsl::string otelEndpoint{"localhost:4317"};
    bsl::string otelExportFile;
    bsl::string otelServiceName{"order-status-monitor-cli"};
    bsl::string otelEnvironment;
    bool enableHttpAdmin{false};
    bsl::string httpAdminUser;
    bsl::string httpAdminPassword;
    std::uint16_t httpAdminPort{15672};
    int httpPageSize{0};
    bool httpDisableStats{true};
    bool httpEnableQueueTotals{true};
    bool skipAutoDeleteQueues{false};
    bool skipExclusiveQueues{true};
    bsl::string overviewCachePath;
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
        "queues", &T::queues,
        "queue_whitelist", &T::queueWhitelist,
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
        "verify_mode", &T::verifyMode,
        "queues_tsv", &T::queuesTsvPath,
        "definitions_json", &T::definitionsJsonPath,
        "log_dir", &T::logDir,
        "log_prefix", &T::logPrefix,
        "ball_min_severity", &T::ballMinSeverity,
        "thread_pool_queue_depth", &T::threadPoolQueueDepth,
        "connection_error_threshold_ms", &T::connectionErrorThresholdMs,
        "shuffle_connection_endpoints", &T::shuffleConnectionEndpoints,
        "infinite_immediate_retry", &T::infiniteImmediateRetry,
        "ball_noisy_prefixes", &T::ballNoisyPrefixes,
        "ball_noisy_min_severity", &T::ballNoisyMinSeverity,
        "enable_otel", &T::enableOtel,
        "enable_otel_traces", &T::enableOtelTraces,
        "enable_otel_metrics", &T::enableOtelMetrics,
        "enable_otel_export", &T::enableOtelExport,
        "otel_protocol", &T::otelProtocol,
        "otel_endpoint", &T::otelEndpoint,
        "otel_export_file", &T::otelExportFile,
        "otel_service_name", &T::otelServiceName,
        "otel_environment", &T::otelEnvironment,
        "enable_http_admin", &T::enableHttpAdmin,
        "http_admin_user", &T::httpAdminUser,
        "http_admin_password", &T::httpAdminPassword,
        "http_admin_port", &T::httpAdminPort,
        "http_page_size", &T::httpPageSize,
        "http_disable_stats", &T::httpDisableStats,
        "http_enable_queue_totals", &T::httpEnableQueueTotals,
        "skip_auto_delete_queues", &T::skipAutoDeleteQueues,
        "skip_exclusive_queues", &T::skipExclusiveQueues,
        "overview_cache", &T::overviewCachePath);
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
    if (const char* v = std::getenv("MQ_LOG_DIR")) cfg.logDir = v;
    if (const char* v = std::getenv("MQ_LOG_PREFIX")) cfg.logPrefix = v;
    if (const char* v = std::getenv("BALL_MIN_SEVERITY")) cfg.ballMinSeverity = v;
    if (const char* v = std::getenv("MQ_THREAD_POOL_QUEUE_DEPTH"))
        cfg.threadPoolQueueDepth = static_cast<std::uint32_t>(std::atoi(v));
    if (const char* v = std::getenv("MQ_CONN_ERROR_THRESHOLD_MS"))
        cfg.connectionErrorThresholdMs = static_cast<std::uint32_t>(std::atoi(v));
    if (const char* v = std::getenv("MQ_SHUFFLE_ENDPOINTS"))
        cfg.shuffleConnectionEndpoints = std::atoi(v) != 0;
    if (const char* v = std::getenv("MQ_INFINITE_IMMEDIATE_RETRY"))
        cfg.infiniteImmediateRetry = std::atoi(v) != 0;
    if (const char* v = std::getenv("MQ_BALL_NOISY_MIN_SEVERITY"))
        cfg.ballNoisyMinSeverity = std::atoi(v);
    if (const char* v = std::getenv("MQ_BALL_NOISY_MIN_SEVERITY"))
        cfg.ballNoisyMinSeverity = std::atoi(v);
    if (const char* v = std::getenv("OTEL_ENABLE")) cfg.enableOtel = std::atoi(v) != 0;
    if (const char* v = std::getenv("OTEL_TRACES")) cfg.enableOtelTraces = std::atoi(v) != 0;
    if (const char* v = std::getenv("OTEL_METRICS")) cfg.enableOtelMetrics = std::atoi(v) != 0;
    if (const char* v = std::getenv("OTEL_EXPORT")) cfg.enableOtelExport = std::atoi(v) != 0;
    if (const char* v = std::getenv("OTEL_PROTOCOL")) cfg.otelProtocol = v;
    if (const char* v = std::getenv("OTEL_ENDPOINT")) cfg.otelEndpoint = v;
    if (const char* v = std::getenv("OTEL_EXPORT_FILE")) cfg.otelExportFile = v;
    if (const char* v = std::getenv("OTEL_SERVICE_NAME")) cfg.otelServiceName = v;
    if (const char* v = std::getenv("OTEL_ENVIRONMENT")) cfg.otelEnvironment = v;
    if (const char* v = std::getenv("MQ_HTTP_ADMIN_ENABLE")) cfg.enableHttpAdmin = std::atoi(v) != 0;
    if (const char* v = std::getenv("MQ_HTTP_ADMIN_USER")) cfg.httpAdminUser = v;
    if (const char* v = std::getenv("MQ_HTTP_ADMIN_PASSWORD")) cfg.httpAdminPassword = v;
    if (const char* v = std::getenv("MQ_HTTP_ADMIN_PORT")) cfg.httpAdminPort = static_cast<std::uint16_t>(std::atoi(v));
    if (const char* v = std::getenv("MQ_SKIP_AUTO_DELETE_QUEUES"))
        cfg.skipAutoDeleteQueues = std::atoi(v) != 0;
    if (const char* v = std::getenv("MQ_OVERVIEW_CACHE")) cfg.overviewCachePath = v;
}

namespace detail {
bsl::vector<bsl::string> parseQueuesTsv(const bsl::string& path)
{
    bsl::vector<bsl::string> out;
    std::ifstream in(path.c_str());
    if (!in) {
        throw std::runtime_error("Failed to open queues TSV: " + path);
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        // take first column before tab
        const auto tabPos = line.find('\t');
        const auto name = line.substr(0, tabPos);
        if (name == "name" || name.empty()) continue;
        out.push_back(name);
    }
    return out;
}

void dedupeQueues(bsl::vector<bsl::string>& queues)
{
    bsl::unordered_set<bsl::string> seen;
    bsl::vector<bsl::string> uniq;
    for (const auto& q : queues) {
        if (q.empty()) continue;
        if (seen.insert(q).second) {
            uniq.push_back(q);
        }
    }
    queues.swap(uniq);
}

// Minimal structures for rabbitmqadmin-ng definitions export
struct DefinitionsQueue {
    bsl::string name;
    bsl::string vhost;
};
struct DefinitionsExchange {
    bsl::string name;
    bsl::string vhost;
};
struct DefinitionsBinding {
    bsl::string vhost;
    bsl::string source;
    bsl::string destination;
    bsl::string destinationType;
    bsl::string routingKey;
};
struct DefinitionsVhost {
    bsl::string name;
};
struct Definitions {
    bsl::vector<DefinitionsQueue> queues;
    bsl::vector<DefinitionsExchange> exchanges;
    bsl::vector<DefinitionsBinding> bindings;
    bsl::vector<DefinitionsVhost> vhosts;
};

Definitions readDefinitions(const bsl::string& path)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open definitions JSON: " + path);
    }
    std::string json((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    Definitions defs;
    // Allow unknown keys because definitions export contains many fields we
    // don't map.
    constexpr auto opts = ::glz::opts{.error_on_unknown_keys = false};
    auto ec = ::glz::read<opts>(defs, json);
    if (ec) {
        throw std::runtime_error(
            "Failed to parse definitions JSON: " + ::glz::format_error(ec, json));
    }
    return defs;
}

void mergeDefinitionsQueues(const Definitions& defs,
                            const bsl::string& targetVhost,
                            bsl::vector<bsl::string>& queues)
{
    for (const auto& q : defs.queues) {
        if (q.vhost == targetVhost) {
            queues.push_back(q.name);
        }
    }
}

}  // namespace detail

}  // namespace osmcli

namespace glz {
template <>
struct meta<osmcli::detail::DefinitionsQueue> {
    using T = osmcli::detail::DefinitionsQueue;
    static constexpr auto value = object("name", &T::name, "vhost", &T::vhost);
};
template <>
struct meta<osmcli::detail::DefinitionsExchange> {
    using T = osmcli::detail::DefinitionsExchange;
    static constexpr auto value = object("name", &T::name, "vhost", &T::vhost);
};
template <>
struct meta<osmcli::detail::DefinitionsBinding> {
    using T = osmcli::detail::DefinitionsBinding;
    static constexpr auto value = object("vhost",
                                         &T::vhost,
                                         "source",
                                         &T::source,
                                         "destination",
                                         &T::destination,
                                         "destination_type",
                                         &T::destinationType,
                                         "routing_key",
                                         &T::routingKey);
};
template <>
struct meta<osmcli::detail::DefinitionsVhost> {
    using T = osmcli::detail::DefinitionsVhost;
    static constexpr auto value = object("name", &T::name);
};
template <>
struct meta<osmcli::detail::Definitions> {
    using T = osmcli::detail::Definitions;
    static constexpr auto value =
        object("queues", &T::queues,
               "exchanges", &T::exchanges,
               "bindings", &T::bindings,
               "vhosts", &T::vhosts);
};
}  // namespace glz

namespace osmcli {

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

DefinitionsSummary loadDefinitionsSummary(const bsl::string& path)
{
    const auto defs = detail::readDefinitions(path);
    DefinitionsSummary out;
    for (const auto& v : defs.vhosts) {
        out.vhosts.push_back(v.name);
    }
    for (const auto& q : defs.queues) {
        out.queues.push_back({q.name, q.vhost});
    }
    for (const auto& ex : defs.exchanges) {
        out.exchanges.push_back({ex.name, ex.vhost});
    }
    for (const auto& b : defs.bindings) {
        out.bindings.push_back({b.vhost, b.source, b.destination, b.destinationType, b.routingKey});
    }
    return out;
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
    app.add_option("--queue", cfg.queueName, "Queue name (single)");
    app.add_option("--queue-list", cfg.queues, "Queue name (repeatable)");
    app.add_option("--queue-whitelist",
                   cfg.queueWhitelist,
                   "Queue allow-list when using HTTP admin discovery (repeatable)");
    app.add_option("--queues-tsv", cfg.queuesTsvPath, "TSV file with queue names (first column 'name')");
    app.add_option("--definitions-json",
                   cfg.definitionsJsonPath,
                   "rabbitmqadmin-ng definitions export JSON (to populate queues by vhost)");
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
    app.add_option("--log-dir", cfg.logDir, "Directory for log files (default: logs next to binary)");
    app.add_option("--log-prefix", cfg.logPrefix, "Log filename prefix (default: order_status_monitor)");
    app.add_option("--ball-min-severity",
                   cfg.ballMinSeverity,
                   "BALL->Quill minimum severity (trace|debug|info|warn|error|fatal)");
    app.add_option("--thread-pool-queue-depth",
                   cfg.threadPoolQueueDepth,
                   "Thread pool queue depth for single-thread callback dispatcher");
    app.add_option("--connection-error-threshold-ms",
                   cfg.connectionErrorThresholdMs,
                   "Optional connection error threshold; when >0, triggers error callback if no connection after this many ms");
    app.add_flag("--shuffle-endpoints",
                 cfg.shuffleConnectionEndpoints,
                 "Shuffle resolved connection endpoints before connecting");
    app.add_flag("--infinite-immediate-retry",
                 cfg.infiniteImmediateRetry,
                 "Enable rmqcpp IIR tunable (retry forever with immediate attempts)");
    app.add_option("--ball-noisy-prefix",
                   cfg.ballNoisyPrefixes,
                   "BALL categories to suppress below the noisy-min severity (repeatable)");
    app.add_option("--ball-noisy-min-severity",
                   cfg.ballNoisyMinSeverity,
                   "Severity threshold for noisy BALL categories (e.g., 500=TRACE, 400=DEBUG, 300=INFO, 200=WARN)");
    app.add_flag("--enable-otel", cfg.enableOtel, "Enable OpenTelemetry export");
    app.add_flag("--disable-otel-traces", cfg.enableOtelTraces, "Disable OpenTelemetry traces")
        ->default_val(false);
    app.add_flag("--disable-otel-metrics", cfg.enableOtelMetrics, "Disable OpenTelemetry metrics")
        ->default_val(false);
    app.add_option("--otel-protocol", cfg.otelProtocol, "OTLP protocol (grpc|http)");
    app.add_option("--otel-endpoint", cfg.otelEndpoint, "OTLP collector endpoint (grpc host:port or http URL)");
    app.add_option("--otel-service-name", cfg.otelServiceName, "OTel resource service.name");
    app.add_option("--otel-environment", cfg.otelEnvironment, "OTel resource deployment.environment");
    app.add_flag("--enable-http-admin", cfg.enableHttpAdmin, "Enable RabbitMQ HTTP management polling");
    app.add_option("--http-admin-user", cfg.httpAdminUser, "HTTP admin username (defaults to AMQP username)");
    app.add_option("--http-admin-password", cfg.httpAdminPassword, "HTTP admin password (defaults to AMQP password)");
    app.add_option("--http-admin-port", cfg.httpAdminPort, "HTTP admin port (default 15672)");
    app.add_option("--http-page-size", cfg.httpPageSize, "HTTP page size (0 = no explicit pagination)");
    app.add_flag("--http-disable-stats", cfg.httpDisableStats, "Disable stats fields in HTTP listings (default: on)");
    app.add_flag("--http-enable-queue-totals", cfg.httpEnableQueueTotals, "Include queue totals even when stats are disabled (default: on)");
    app.add_flag("--skip-auto-delete-queues",
                 cfg.skipAutoDeleteQueues,
                 "Skip auto-delete queues when discovering via HTTP admin");
    app.add_flag("--skip-exclusive-queues",
                 cfg.skipExclusiveQueues,
                 "Skip exclusive queues when discovering via HTTP admin");
    app.add_option("--overview-cache",
                   cfg.overviewCachePath,
                   "Path to cached /api/overview JSON (used if HTTP admin is disabled)");
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
        if (!jc.queues.empty()) cfg.queues = jc.queues;
        if (!jc.queueWhitelist.empty()) cfg.queueWhitelist = jc.queueWhitelist;
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
        if (!jc.definitionsJsonPath.empty())
            cfg.definitionsJsonPath = jc.definitionsJsonPath;
        if (!jc.logDir.empty()) cfg.logDir = jc.logDir;
        if (!jc.logPrefix.empty()) cfg.logPrefix = jc.logPrefix;
        if (!jc.ballMinSeverity.empty()) cfg.ballMinSeverity = jc.ballMinSeverity;
        if (jc.threadPoolQueueDepth) cfg.threadPoolQueueDepth = jc.threadPoolQueueDepth;
        cfg.enableOtel = jc.enableOtel;
        cfg.enableOtelTraces = jc.enableOtelTraces;
        cfg.enableOtelMetrics = jc.enableOtelMetrics;
        if (!jc.otelProtocol.empty()) cfg.otelProtocol = jc.otelProtocol;
        if (!jc.otelEndpoint.empty()) cfg.otelEndpoint = jc.otelEndpoint;
        if (!jc.otelServiceName.empty()) cfg.otelServiceName = jc.otelServiceName;
        if (!jc.otelEnvironment.empty()) cfg.otelEnvironment = jc.otelEnvironment;
        cfg.enableHttpAdmin = jc.enableHttpAdmin;
        if (!jc.httpAdminUser.empty()) cfg.httpAdminUser = jc.httpAdminUser;
        if (!jc.httpAdminPassword.empty()) cfg.httpAdminPassword = jc.httpAdminPassword;
        if (jc.httpAdminPort) cfg.httpAdminPort = jc.httpAdminPort;
        cfg.skipAutoDeleteQueues = jc.skipAutoDeleteQueues;
        if (!jc.overviewCachePath.empty()) cfg.overviewCachePath = jc.overviewCachePath;
    }

    // Apply env overrides
    applyEnvOverrides(cfg);

    // Merge queue sources: CLI/JSON list, single queue, TSV
    if (!cfg.queueName.empty()) {
        cfg.queues.push_back(cfg.queueName);
    }
    if (!cfg.queuesTsvPath.empty()) {
        auto parsed = detail::parseQueuesTsv(cfg.queuesTsvPath);
        cfg.queues.insert(cfg.queues.end(), parsed.begin(), parsed.end());
    }
    if (!cfg.definitionsJsonPath.empty()) {
        auto defs = detail::readDefinitions(cfg.definitionsJsonPath);
        detail::mergeDefinitionsQueues(defs, cfg.vhost, cfg.queues);
    }
    detail::dedupeQueues(cfg.queues);
    if (!cfg.queues.empty()) {
        cfg.queueName = cfg.queues.front();
    }

    // Apply verify mode if provided on CLI
    cfg.verifyMode = parseVerifyMode(verifyModeStr);

    return cfg;
}

}  // namespace osmcli
