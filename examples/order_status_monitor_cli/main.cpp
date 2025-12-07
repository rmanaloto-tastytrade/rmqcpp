#include "config.h"

#include <rmqa_rabbitcontext.h>
#include <rmqa_vhost.h>
#include <rmqa_topology.h>
#include <rmqa_consumer.h>
#include <rmqt_securityparameters.h>
#include <rmqt_consumerconfig.h>

#include <glaze/glaze.hpp>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/SimpleSetup.h>
#include <quill/sinks/ConsoleSink.h>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>

#include <bdlmt_threadpool.h>
#include <bslmt_threadattributes.h>

#include <bsl_string_view.h>
#include <bsl_memory.h>
#include <bsl_vector.h>
#include <csignal>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <string>
#include <string_view>
#include <optional>
#include <unordered_set>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <openssl/err.h>
#include <ball_context.h>
#include <ball_loggermanager.h>
#include <ball_loggermanagerconfiguration.h>
#include <ball_observer.h>
#include <ball_record.h>
#include <ball_recordattributes.h>
#include <ball_severity.h>
#include <bsl_memory.h>

namespace osmcli_http {
struct HttpVhost {
    bsl::string name;
};
struct HttpExchange {
    bsl::string name;
    bsl::string vhost;
    bsl::string type;
    bool durable{false};
    bool auto_delete{false};
};
struct HttpQueue {
    bsl::string name;
    bsl::string vhost;
    bool exclusive{false};
    bool durable{false};
    bool auto_delete{false};
};
struct HttpBinding {
    bsl::string vhost;
    bsl::string source;
    bsl::string destination;
    bsl::string destination_type;
    bsl::string routing_key;
};

struct HttpAdminSummary {
    std::vector<std::string> vhosts;
    std::vector<HttpExchange> exchanges;
    std::vector<HttpQueue> queues;
    std::vector<HttpBinding> bindings;
    struct Overview {
        std::string cluster_name;
        std::string rabbitmq_version;
        std::string erlang_version;
        struct ObjectTotals {
            int consumers{};
            int exchanges{};
            int queues{};
            int connections{};
            int channels{};
        } object_totals;
    };
    std::optional<Overview> overview;
};
}  // namespace osmcli_http

namespace {
using namespace osmcli;
using namespace osmcli_http;
namespace rmqa = BloombergLP::rmqa;
namespace rmqt = BloombergLP::rmqt;
namespace rmqp = BloombergLP::rmqp;
namespace ball = BloombergLP::ball;
using BloombergLP::bdlmt::ThreadPool;
using BloombergLP::bslmt::ThreadAttributes;
namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

struct App {
    bsl::unique_ptr<ThreadPool> threadPool;
    bsl::unique_ptr<rmqa::RabbitContext> ctx;
    bsl::shared_ptr<rmqa::VHost> vhost;
    bsl::vector<bsl::shared_ptr<rmqa::Consumer> > consumers;
    quill::Logger* logger{nullptr};
};

class QuillBallObserver : public ball::Observer {
  public:
    explicit QuillBallObserver(quill::Logger* logger)
    : d_logger(logger)
    {
    }

    void publish(const ball::Record& record, const ball::Context& context) override
    {
        if (!d_logger) return;
        const auto& ff = record.fixedFields();
        const std::string_view cat = ff.category();
        const std::string_view msg = ff.message();
        const auto lvl = mapSeverity(ff.severity());
        const auto sevName = severityName(ff.severity());
        switch (lvl) {
        case quill::LogLevel::Critical:
            QUILL_LOG_CRITICAL(d_logger, "[BALL:{}:{}] {}", sevName, cat, msg);
            break;
        case quill::LogLevel::Error:
            QUILL_LOG_ERROR(d_logger, "[BALL:{}:{}] {}", sevName, cat, msg);
            break;
        case quill::LogLevel::Warning:
            QUILL_LOG_WARNING(d_logger, "[BALL:{}:{}] {}", sevName, cat, msg);
            break;
        case quill::LogLevel::Info:
            QUILL_LOG_INFO(d_logger, "[BALL:{}:{}] {}", sevName, cat, msg);
            break;
        default:
            QUILL_LOG_DEBUG(d_logger, "[BALL:{}:{}] {}", sevName, cat, msg);
            break;
        }
    }

    void releaseRecords() override {}

  private:
    quill::Logger* d_logger;

    static quill::LogLevel mapSeverity(int severity)
    {
        using S = ball::Severity::Level;
        if (severity >= S::e_FATAL) return quill::LogLevel::Critical;
        if (severity >= S::e_ERROR) return quill::LogLevel::Error;
        if (severity >= S::e_WARN) return quill::LogLevel::Warning;
        if (severity >= S::e_INFO) return quill::LogLevel::Info;
        return quill::LogLevel::Debug;
    }

    static std::string_view severityName(int severity)
    {
        using S = ball::Severity::Level;
        if (severity >= S::e_FATAL) return "FATAL";
        if (severity >= S::e_ERROR) return "ERROR";
        if (severity >= S::e_WARN) return "WARN";
        if (severity >= S::e_INFO) return "INFO";
        if (severity >= S::e_TRACE) return "TRACE";
        return "DEBUG";
    }
};

std::string basicAuthHeader(const std::string& user, const std::string& password)
{
    const std::string creds = user + ":" + password;
    std::string encoded(boost::beast::detail::base64::encoded_size(creds.size()), '\0');
    auto len =
        boost::beast::detail::base64::encode(encoded.data(), creds.data(), creds.size());
    encoded.resize(len);
    return "Basic " + encoded;
}

std::string httpGet(const osmcli::ConnectionConfig& cfg,
                    const std::string& target,
                    const std::string& authHeader)
{
    asio::io_context ioc;
    const std::string host(cfg.host.data(), cfg.host.size());
    const std::string port = std::to_string(cfg.httpAdminPort);
    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "rmqcpp-osm");
    if (!authHeader.empty()) {
        req.set(http::field::authorization, authHeader);
    }

    auto checkStatus = [&](auto& res) {
        if (res.result() != http::status::ok) {
            throw std::runtime_error(fmt::format("HTTP {} {} for {}", res.result_int(), res.reason(), target));
        }
    };

    if (cfg.useTls) {
        ssl::context ctx(ssl::context::sslv23_client);
        if (!cfg.caCertPath.empty()) {
            ctx.load_verify_file(std::string(cfg.caCertPath.data(), cfg.caCertPath.size()));
            ctx.set_verify_mode(ssl::verify_peer);
        }
        else {
            ctx.set_verify_mode(ssl::verify_none);
        }

        tcp::resolver resolver(ioc);
        ssl::stream<beast::tcp_stream> stream{ioc, ctx};
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            throw beast::system_error(
                static_cast<int>(::ERR_get_error()),
                asio::error::get_ssl_category());
        }
        auto results = resolver.resolve(host, port);
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);
        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        beast::error_code ec;
        stream.shutdown(ec);
        checkStatus(res);
        return res.body();
    }
    else {
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream{ioc};
        auto results = resolver.resolve(host, port);
        stream.connect(results);
        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        stream.socket().shutdown(tcp::socket::shutdown_both);
        checkStatus(res);
        return res.body();
    }
}

template <class T>
std::vector<T> parseArray(const std::string& json)
{
    std::vector<T> out;
    constexpr auto opts = ::glz::opts{.error_on_unknown_keys = false};
    auto ec = ::glz::read<opts>(out, json);
    if (ec) {
        throw std::runtime_error("Failed to parse HTTP response: " + ::glz::format_error(ec, json));
    }
    return out;
}

HttpAdminSummary fetchHttpAdmin(const osmcli::ConnectionConfig& cfg)
{
    HttpAdminSummary summary;
    const std::string user = cfg.httpAdminUser.empty()
                                 ? std::string(cfg.username.data(), cfg.username.size())
                                 : std::string(cfg.httpAdminUser.data(), cfg.httpAdminUser.size());
    const std::string pass = cfg.httpAdminPassword.empty()
                                 ? std::string(cfg.password.data(), cfg.password.size())
                                 : std::string(cfg.httpAdminPassword.data(), cfg.httpAdminPassword.size());
    const std::string auth = basicAuthHeader(user, pass);

    const auto vhostsJson = httpGet(cfg, "/api/vhosts", auth);
    auto vhostsObj = parseArray<HttpVhost>(vhostsJson);
    summary.vhosts.reserve(vhostsObj.size());
    for (const auto& v : vhostsObj) summary.vhosts.emplace_back(v.name.data(), v.name.size());

    summary.exchanges = parseArray<HttpExchange>(httpGet(cfg, "/api/exchanges", auth));
    summary.queues = parseArray<HttpQueue>(
        httpGet(cfg, "/api/queues?disable_stats=true&enable_queue_totals=true", auth));
    summary.bindings = parseArray<HttpBinding>(httpGet(cfg, "/api/bindings", auth));
    try {
        const auto overviewJson = httpGet(cfg, "/api/overview", auth);
        HttpAdminSummary::Overview ov;
        constexpr auto opts = ::glz::opts{.error_on_unknown_keys = false};
        auto ec = ::glz::read<opts>(ov, overviewJson);
        if (!ec) {
            summary.overview = ov;
        }
    }
    catch (const std::exception&) {
        summary.overview = std::nullopt;
    }
    return summary;
}

void dedupeStrings(std::vector<std::string>& names)
{
    std::unordered_set<std::string> seen;
    std::vector<std::string> out;
    out.reserve(names.size());
    for (const auto& n : names) {
        if (n.empty()) continue;
        if (seen.insert(n).second) out.push_back(n);
    }
    names.swap(out);
}

std::optional<HttpAdminSummary::Overview>
loadOverviewCache(const std::string& path)
{
    if (path.empty()) return std::nullopt;
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return std::nullopt;
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    HttpAdminSummary::Overview ov;
    constexpr auto opts = ::glz::opts{.error_on_unknown_keys = false};
    auto ec = ::glz::read<opts>(ov, json);
    if (ec) return std::nullopt;
    return ov;
}

}  // namespace

namespace glz {
template <>
struct meta<osmcli_http::HttpVhost> {
    using T = osmcli_http::HttpVhost;
    static constexpr auto value = object("name", &T::name);
};
template <>
struct meta<osmcli_http::HttpExchange> {
    using T = osmcli_http::HttpExchange;
    static constexpr auto value =
        object("name", &T::name,
               "vhost", &T::vhost,
               "type", &T::type,
               "durable", &T::durable,
               "auto_delete", &T::auto_delete);
};
template <>
struct meta<osmcli_http::HttpQueue> {
    using T = osmcli_http::HttpQueue;
    static constexpr auto value =
        object("name", &T::name,
               "vhost", &T::vhost,
               "exclusive", &T::exclusive,
               "durable", &T::durable,
               "auto_delete", &T::auto_delete);
};
template <>
struct meta<osmcli_http::HttpBinding> {
    using T = osmcli_http::HttpBinding;
    static constexpr auto value =
        object("vhost", &T::vhost,
               "source", &T::source,
               "destination", &T::destination,
               "destination_type", &T::destination_type,
               "routing_key", &T::routing_key);
};

template <>
struct meta<osmcli_http::HttpAdminSummary::Overview::ObjectTotals> {
    using T = osmcli_http::HttpAdminSummary::Overview::ObjectTotals;
    static constexpr auto value =
        object("consumers", &T::consumers,
               "exchanges", &T::exchanges,
               "queues", &T::queues,
               "connections", &T::connections,
               "channels", &T::channels);
};
template <>
struct meta<osmcli_http::HttpAdminSummary::Overview> {
    using T = osmcli_http::HttpAdminSummary::Overview;
    static constexpr auto value =
        object("cluster_name", &T::cluster_name,
               "rabbitmq_version", &T::rabbitmq_version,
               "erlang_version", &T::erlang_version,
               "object_totals", &T::object_totals);
};

template <>
struct meta<osmcli_http::HttpAdminSummary> {
    using T = osmcli_http::HttpAdminSummary;
    static constexpr auto value =
        object("vhosts", &T::vhosts,
               "exchanges", &T::exchanges,
               "queues", &T::queues,
               "bindings", &T::bindings,
               "overview", &T::overview);
};
}  // namespace glz

int main(int argc, char** argv)
{
    auto cfg = osmcli::loadConfig(argc, argv);

    // Set up quill logger (default file under logs/, optionally stdout/stderr)
    std::filesystem::path exeDir;
    try {
        exeDir = std::filesystem::canonical(std::filesystem::path(argv[0])).parent_path();
    }
    catch (...) {
        exeDir = std::filesystem::current_path();
    }

    std::string logTarget = "stdout";
    if (cfg.logDir == "stdout" || cfg.logDir == "stderr") {
        logTarget = cfg.logDir;
    }
    else {
        std::filesystem::path logDir(cfg.logDir);
        if (logDir.empty()) {
            logDir = "logs";
        }
        if (logDir.is_relative()) {
            logDir = exeDir / logDir;
        }
        std::string prefix =
            cfg.logPrefix.empty() ? "order_status_monitor"
                                  : std::string(cfg.logPrefix.data(), cfg.logPrefix.size());

        auto now = std::chrono::system_clock::now();
        const auto sec = std::chrono::time_point_cast<std::chrono::seconds>(now);
        const auto ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - sec).count();
        std::time_t tt = std::chrono::system_clock::to_time_t(sec);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        const auto timestamp =
            fmt::format("{:%Y%m%d.%H%M%S}.{:09d}", tm, static_cast<int>(ns));

        std::filesystem::path logPath = logDir / fmt::format("{}-{}.log", prefix, timestamp);

        try {
            if (logPath.has_parent_path()) {
                std::filesystem::create_directories(logPath.parent_path());
            }
            logTarget = logPath.string();
        }
        catch (const std::exception& ex) {
            std::fprintf(stderr,
                         "warning: failed to prepare log directory for %s: %s; "
                         "falling back to stdout\n",
                         logPath.string().c_str(),
                         ex.what());
            logTarget = "stdout";
        }
    }
    auto logger = quill::simple_logger(logTarget);

    // Bridge rmqcpp's BALL logging into Quill to avoid UNINITIALIZED_LOGGER_MANAGER noise.
    ball::LoggerManagerConfiguration ballConfig;
    // Capture everything from BALL and forward to Quill: record at TRACE,
    // pass/publish at TRACE, trigger at ERROR, abort at FATAL.
    ballConfig.setDefaultThresholdLevelsIfValid(ball::Severity::e_TRACE,
                                                ball::Severity::e_TRACE,
                                                ball::Severity::e_ERROR,
                                                ball::Severity::e_FATAL);
    ball::LoggerManagerScopedGuard ballGuard(ballConfig);
    auto ballObserver =
        bsl::shared_ptr<ball::Observer>(new QuillBallObserver(logger));
    ball::LoggerManager::singleton().registerObserver(ballObserver, "quill");

    if (cfg.enableHttpAdmin) {
        try {
            auto httpSummary = fetchHttpAdmin(cfg);
            if (!httpSummary.overview && !cfg.overviewCachePath.empty()) {
                if (auto cached = loadOverviewCache(cfg.overviewCachePath)) {
                    httpSummary.overview = *cached;
                    const std::string cachePath(cfg.overviewCachePath.data(),
                                               cfg.overviewCachePath.size());
                    QUILL_LOG_INFO(logger,
                                   "[http] loaded overview from cache {}",
                                   cachePath);
                } else {
                    const std::string cachePath(cfg.overviewCachePath.data(),
                                               cfg.overviewCachePath.size());
                    QUILL_LOG_INFO(logger,
                                   "[http] no overview available (HTTP failed and cache "
                                   "missing/unreadable: {})",
                                   cachePath);
                }
            }
            QUILL_LOG_INFO(logger,
                           "http admin summary: vhosts={} exchanges={} queues={} bindings={}",
                           httpSummary.vhosts.size(),
                           httpSummary.exchanges.size(),
                           httpSummary.queues.size(),
                           httpSummary.bindings.size());
            for (const auto& v : httpSummary.vhosts) {
                std::vector<std::string> exNames;
                std::vector<std::string> qNames;
                std::vector<std::string> bindingDescs;
                for (const auto& ex : httpSummary.exchanges) {
                    if (ex.vhost == v) exNames.emplace_back(ex.name.data(), ex.name.size());
                }
                for (const auto& q : httpSummary.queues) {
                    if (q.vhost == v) qNames.emplace_back(q.name.data(), q.name.size());
                }
                for (const auto& b : httpSummary.bindings) {
                    if (b.vhost == v) {
                        bindingDescs.emplace_back(fmt::format(
                            "{} -> {} ({}) rk={}", b.source, b.destination, b.destination_type, b.routing_key));
                    }
                }
                const std::string exList = fmt::format("{}", fmt::join(exNames, ","));
                const std::string qList = fmt::format("{}", fmt::join(qNames, ","));
                const std::string bList = fmt::format("{}", fmt::join(bindingDescs, ","));
                QUILL_LOG_INFO(logger,
                               "[http] vhost={} exchanges={} [{}] queues={} [{}] bindings={} [{}]",
                               v,
                               exNames.size(),
                               exList,
                               qNames.size(),
                               qList,
                               bindingDescs.size(),
                               bList);
            }

            if (httpSummary.overview) {
                const auto& ov = *httpSummary.overview;
                QUILL_LOG_INFO(logger,
                               "[http] overview cluster={} rabbitmq={} erlang={} totals: queues={} exchanges={} connections={} channels={} consumers={}",
                               ov.cluster_name,
                               ov.rabbitmq_version,
                               ov.erlang_version,
                               ov.object_totals.queues,
                               ov.object_totals.exchanges,
                               ov.object_totals.connections,
                               ov.object_totals.channels,
                               ov.object_totals.consumers);
            }

            // Use live queue list from HTTP instead of stale definitions
            std::vector<std::string> discovered;
            std::vector<std::string> skippedExclusive;
            std::vector<std::string> skippedAutoDelete;
            for (const auto& q : httpSummary.queues) {
                if (q.vhost == std::string(cfg.vhost.data(), cfg.vhost.size())) {
                    if (q.exclusive) {
                        skippedExclusive.emplace_back(q.name.data(), q.name.size());
                        continue;
                    }
                    if (cfg.skipAutoDeleteQueues && q.auto_delete) {
                        skippedAutoDelete.emplace_back(q.name.data(), q.name.size());
                        continue;
                    }
                    discovered.emplace_back(q.name.data(), q.name.size());
                }
            }
            dedupeStrings(discovered);
            if (!discovered.empty()) {
                cfg.queues.clear();
                cfg.queues.reserve(discovered.size());
                for (const auto& q : discovered) {
                    cfg.queues.push_back(bsl::string(q.data(), q.size()));
                }
                cfg.queueName = cfg.queues.front();
                QUILL_LOG_INFO(logger,
                               "[http] using live queue list for vhost={} count={}",
                               std::string(cfg.vhost.data(), cfg.vhost.size()),
                               cfg.queues.size());
                if (!skippedExclusive.empty()) {
                    const std::string skipped =
                        fmt::format("{}", fmt::join(skippedExclusive, ","));
                    QUILL_LOG_INFO(logger,
                                   "[http] skipped exclusive queues (not safe to attach consumers): {}",
                                   skipped);
                }
                if (!skippedAutoDelete.empty()) {
                    const std::string skipped =
                        fmt::format("{}", fmt::join(skippedAutoDelete, ","));
                    QUILL_LOG_INFO(logger,
                                   "[http] skipped auto-delete queues (disabled via config): {}",
                                   skipped);
                }
            } else {
                QUILL_LOG_ERROR(logger,
                                "[http] no queues discovered for vhost={} (falling back to configured/definitions list)",
                                std::string(cfg.vhost.data(), cfg.vhost.size()));
            }
        }
        catch (const std::exception& ex) {
            QUILL_LOG_ERROR(logger, "http admin fetch failed: {}", ex.what());
        }
    }

    if (!cfg.enableHttpAdmin && !cfg.definitionsJsonPath.empty()) {
        try {
            auto defs = osmcli::loadDefinitionsSummary(cfg.definitionsJsonPath);
            QUILL_LOG_INFO(logger,
                           "definitions summary: vhosts={} queues={} exchanges={} bindings={}",
                           defs.vhosts.size(),
                           defs.queues.size(),
                           defs.exchanges.size(),
                           defs.bindings.size());
            for (const auto& v : defs.vhosts) {
                std::string vhostName(v.data(), v.size());
                bsl::vector<std::string> qNames;
                bsl::vector<std::string> exNames;
                bsl::vector<std::string> bindingDescs;
                for (const auto& q : defs.queues) {
                    if (q.vhost == v) qNames.emplace_back(q.name.data(), q.name.size());
                }
                for (const auto& ex : defs.exchanges) {
                    if (ex.vhost == v) exNames.emplace_back(ex.name.data(), ex.name.size());
                }
                for (const auto& b : defs.bindings) {
                    if (b.vhost == v) {
                        bindingDescs.emplace_back(fmt::format(
                            "{} -> {} ({}) rk={}", b.source, b.destination, b.destinationType, b.routingKey));
                    }
                }
                const auto queuesStr = fmt::format("{}", fmt::join(qNames, ","));
                const auto exchangesStr = fmt::format("{}", fmt::join(exNames, ","));
                const auto bindingsStr = fmt::format("{}", fmt::join(bindingDescs, ","));
                QUILL_LOG_INFO(logger,
                               "vhost={} queues={} [{}] exchanges={} [{}] bindings={} [{}]",
                               vhostName,
                               qNames.size(),
                               queuesStr,
                               exNames.size(),
                               exchangesStr,
                               bindingDescs.size(),
                               bindingsStr);
            }
        }
        catch (const std::exception& ex) {
            QUILL_LOG_ERROR(logger,
                            "failed to load definitions summary from {}: {}",
                            std::string(cfg.definitionsJsonPath.data(), cfg.definitionsJsonPath.size()),
                            ex.what());
        }
    }

    try {
        App app;
        app.logger = logger;
        rmqa::RabbitContextOptions opts;
        // Single-threaded callback pool
        app.threadPool = bsl::make_unique<ThreadPool>(
            ThreadAttributes(), 1, 1, 60000);
        app.threadPool->start();
        opts.setThreadpool(app.threadPool.get());
        app.ctx = bsl::make_unique<rmqa::RabbitContext>(opts);

        auto endpoint = cfg.endpoint();
        auto creds = cfg.credentials();
        app.vhost =
            app.ctx->createVHostConnection("osm-cli", endpoint, creds);
        if (!app.vhost) {
            QUILL_LOG_ERROR(logger, "failed to create vhost connection");
            return 1;
        }

        // Only depend on pre-existing broker topology; use passive declarations
        // to avoid altering queues/exchanges.
        rmqa::Topology topology;
        if (cfg.queues.empty()) {
            throw std::runtime_error("queue is required (use --queue/--queue-list or provide queues in config)");
        }

        rmqt::ConsumerConfig cconfig;
        cconfig.setPrefetchCount(cfg.prefetch);

        // Create passive queues and consumers for each requested queue
        for (const auto& qname : cfg.queues) {
            // Per-queue callback to include queue name in logs
            std::string qnameStd(qname.data(), qname.size());
            auto onMessage = [logger, qnameStd](rmqp::MessageGuard& guard) {
                const auto& msg = guard.message();
                const auto& env = guard.envelope();
                std::string exchange(env.exchange().data(),
                                     env.exchange().size());
                std::string routingKey(env.routingKey().data(),
                                       env.routingKey().size());
                QUILL_LOG_INFO(logger,
                               "delivery tag={} exchange={} queue={} rk={} bytes={}",
                               env.deliveryTag(),
                               exchange,
                               qnameStd,
                               routingKey,
                               msg.payloadSize());
                guard.ack();
            };

            rmqa::Topology topology;
            auto queue = topology.addPassiveQueue(qname);
            auto consumerResult =
                app.vhost->createConsumer(topology, queue, onMessage, cconfig);
            if (!consumerResult) {
                const auto& err = consumerResult.error();
                QUILL_LOG_ERROR(
                    logger,
                    "failed to create consumer queue={} vhost={} (consider refreshing definitions or enabling http admin): {}",
                    qnameStd,
                    std::string(cfg.vhost.data(), cfg.vhost.size()),
                    std::string(err.data(), err.size()));
                continue;
            }
            app.consumers.push_back(consumerResult.value());
            QUILL_LOG_INFO(logger,
                           "consuming host={} port={} vhost={} queue={} prefetch={}",
                           std::string(cfg.host.data(), cfg.host.size()),
                           cfg.port,
                           std::string(cfg.vhost.data(), cfg.vhost.size()),
                           qnameStd,
                           cfg.prefetch);
        }
        if (app.consumers.empty()) {
            QUILL_LOG_ERROR(logger, "no consumers started (all failed)");
            return 1;
        }

        static std::atomic<bool> stop{false};
        auto sigHandler = [](int) { stop.store(true, std::memory_order_relaxed); };
        std::signal(SIGINT, sigHandler);
        std::signal(SIGTERM, sigHandler);

        std::unique_ptr<std::thread> timerThread;
        if (cfg.runSeconds > 0) {
            timerThread = std::make_unique<std::thread>([&] {
                std::this_thread::sleep_for(std::chrono::seconds(cfg.runSeconds));
                QUILL_LOG_INFO(logger, "run-seconds elapsed, stopping");
                stop.store(true);
            });
        }

        while (!stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        for (auto& c : app.consumers) {
            c->cancel();
        }
        if (timerThread) timerThread->join();
        if (app.threadPool) {
            app.threadPool->stop();
        }
    }
    catch (const std::exception& ex) {
        QUILL_LOG_ERROR(logger, "fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
