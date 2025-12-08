#include "config.h"
#include "../../MonitorUtil.hpp"

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

#ifndef OSMCLI_HAVE_OTEL
#  if __has_include(<opentelemetry/sdk/trace/tracer_provider.h>)
#    define OSMCLI_HAVE_OTEL 1
#  else
#    define OSMCLI_HAVE_OTEL 0
#  endif
#endif
#ifndef OSMCLI_HAVE_OTEL_GRPC
#  define OSMCLI_HAVE_OTEL_GRPC 0
#endif
#ifndef OSMCLI_HAVE_OTEL_HTTP
#  define OSMCLI_HAVE_OTEL_HTTP 0
#endif

#if OSMCLI_HAVE_OTEL
#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter.h>
#if OSMCLI_HAVE_OTEL
#if OSMCLI_HAVE_OTEL_GRPC
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter.h>
#endif
#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#endif

#if OSMCLI_HAVE_OTEL
struct OtelContext {
    std::shared_ptr<opentelemetry::trace::TracerProvider> tracerProvider;
    std::shared_ptr<opentelemetry::metrics::MeterProvider> meterProvider;
};

struct SpanJson {
    std::string name;
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::int64_t start_unix_nano;
    std::int64_t end_unix_nano;
};

struct FileSpanExporter : public opentelemetry::sdk::trace::SpanExporter {
    explicit FileSpanExporter(const std::string& path)
    : d_path(path)
    {
        if (!d_path.empty()) {
            d_out.open(d_path, std::ios::app);
        }
    }

    std::unique_ptr<opentelemetry::sdk::trace::Recordable> MakeRecordable() noexcept override
    {
        return std::unique_ptr<opentelemetry::sdk::trace::Recordable>(
            new opentelemetry::sdk::trace::SpanData);
    }

    opentelemetry::sdk::common::ExportResult Export(
        const opentelemetry::nostd::span<std::unique_ptr<opentelemetry::sdk::trace::Recordable>>& spans) noexcept override
    {
        if (!d_out.is_open()) return opentelemetry::sdk::common::ExportResult::kFailure;
        for (auto& rec : spans) {
            auto* sd = dynamic_cast<opentelemetry::sdk::trace::SpanData*>(rec.get());
            if (!sd) continue;
            char traceBuf[32];
            char spanBuf[16];
            char parentBuf[16];
            sd->GetTraceId().ToLowerBase16(traceBuf);
            sd->GetSpanId().ToLowerBase16(spanBuf);
            sd->GetParentSpanId().ToLowerBase16(parentBuf);
            auto nameView = sd->GetName();
            auto start = sd->GetStartTime().time_since_epoch();
            auto end = start + sd->GetDuration();
            SpanJson sj{
                std::string(nameView.data(), nameView.size()),
                std::string(traceBuf, sizeof traceBuf),
                std::string(spanBuf, sizeof spanBuf),
                std::string(parentBuf, sizeof parentBuf),
                static_cast<std::int64_t>(start.count()),
                static_cast<std::int64_t>(end.count()),
            };
            auto jsonExp = ::glz::write_json(sj);
            if (jsonExp) {
                d_out << *jsonExp << "\n";
            }
        }
        d_out.flush();
        return opentelemetry::sdk::common::ExportResult::kSuccess;
    }

    bool ForceFlush(std::chrono::microseconds) noexcept override { return true; }
    bool Shutdown(std::chrono::microseconds) noexcept override { return true; }

  private:
    std::string d_path;
    std::ofstream d_out;
};
#endif

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

template <class T>
struct PagedResponse {
    std::vector<T> items;
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
    std::string overviewJson;
    std::vector<std::string> skippedExclusive;
    std::vector<std::string> skippedAutoDelete;
    std::vector<std::string> filteredByWhitelist;
};

struct LatencySample {
    std::string queue;
    std::int64_t duration_ns{};
    std::uint64_t tsc_delta{};
    std::uint64_t tsc_entry{};
    std::uint64_t tsc_exit{};
    std::uint64_t real_entry_ns{};
    std::uint64_t real_exit_ns{};
    std::optional<std::uint64_t> socket_ts_ns;
};
}  // namespace osmcli_http

namespace {
using namespace osmcli;
using namespace osmcli_http;
#if OSMCLI_HAVE_OTEL
namespace otel = opentelemetry;
namespace otel_sdk = opentelemetry::sdk;
namespace otel_trace = opentelemetry::sdk::trace;
namespace otel_metrics = opentelemetry::sdk::metrics;
namespace otel_resource = opentelemetry::sdk::resource;
namespace otel_nostd = opentelemetry::nostd;
#endif
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
#if OSMCLI_HAVE_OTEL
    otel_nostd::shared_ptr<opentelemetry::trace::Tracer> tracer;
    otel_nostd::shared_ptr<opentelemetry::metrics::Meter> meter;
    otel_nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t> > counterMessages;
    otel_nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t> > counterEnqueueFails;
#endif
    std::atomic<uint64_t> messages{0};
    std::atomic<uint64_t> enqueueFailures{0};
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
        // Per-category thresholds: channel/connection -> WARN+, others -> cfg-driven (mapSeverity).
        const bool isChannel = cat.rfind("RMQAMQP.CHANNEL", 0) == 0;
        const bool isConn = cat.rfind("RMQAMQP.CONNECTION", 0) == 0;
        if ((isChannel || isConn) && ff.severity() < ball::Severity::e_WARN) {
            return;
        }
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

template <typename F>
class ScopeExit {
  public:
    explicit ScopeExit(F&& f) : f_(std::move(f)), active_(true) {}
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit(ScopeExit&& other) noexcept : f_(std::move(other.f_)), active_(other.active_) { other.active_ = false; }
    ~ScopeExit() { if (active_) f_(); }
  private:
    F f_;
    bool active_;
};

ball::Severity::Level parseBallSeverity(const bsl::string& s)
{
    std::string lower(s.data(), s.size());
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "fatal") return ball::Severity::e_FATAL;
    if (lower == "error" || lower == "err") return ball::Severity::e_ERROR;
    if (lower == "warn" || lower == "warning") return ball::Severity::e_WARN;
    if (lower == "info") return ball::Severity::e_INFO;
    if (lower == "debug") return ball::Severity::e_DEBUG;
    return ball::Severity::e_TRACE;
}

opentelemetry::sdk::resource::Resource buildResource(const osmcli::ConnectionConfig& cfg)
{
    otel_resource::ResourceAttributes attrs;
    attrs["service.name"] = std::string(cfg.otelServiceName.data(), cfg.otelServiceName.size());
    if (!cfg.otelEnvironment.empty()) {
        attrs["deployment.environment"] =
            std::string(cfg.otelEnvironment.data(), cfg.otelEnvironment.size());
    }
    if (!cfg.vhost.empty()) {
        attrs["messaging.rabbitmq.vhost"] =
            std::string(cfg.vhost.data(), cfg.vhost.size());
    }
    return otel_resource::Resource::Create(attrs);
}

OtelContext initOtel(const osmcli::ConnectionConfig& cfg,
                     quill::Logger* logger,
                     const std::filesystem::path& resolvedLogDir)
{
    OtelContext ctx;
    if (!cfg.enableOtel || !cfg.enableOtelExport) {
        return ctx;
    }
    auto warn = [&](const std::string& msg) {
        if (logger) {
            QUILL_LOG_WARNING(logger, "otel disabled: {}", msg);
        }
    };
    const std::string proto(cfg.otelProtocol.data(), cfg.otelProtocol.size());
    if (proto != "grpc" && proto != "http" && proto != "file") {
        warn("invalid otel_protocol (use grpc|http|file)");
        return ctx;
    }
    if ((proto == "grpc" || proto == "http") && cfg.otelEndpoint.empty()) {
        warn("otel_endpoint is required for grpc/http exporters");
        return ctx;
    }
    if (proto == "http") {
        const std::string ep(cfg.otelEndpoint.data(), cfg.otelEndpoint.size());
        if (ep.rfind("http://", 0) != 0 && ep.rfind("https://", 0) != 0) {
            warn("otel_endpoint for http should be a full URL");
            return ctx;
        }
    }
    std::filesystem::path fileExportPath;
    if (proto == "file") {
        if (!cfg.otelExportFile.empty()) {
            fileExportPath = std::filesystem::path(
                std::string(cfg.otelExportFile.data(), cfg.otelExportFile.size()));
        }
        else if (!resolvedLogDir.empty()) {
            fileExportPath = resolvedLogDir / "otel_spans.ndjson";
        }
        if (fileExportPath.empty()) {
            warn("otel protocol=file but no export path configured");
            return ctx;
        }
    }
    auto resource = buildResource(cfg);

    if (cfg.enableOtelTraces) {
        bool tracerInitialized = false;
#if defined(OSMCLI_HAVE_OTEL_GRPC) && OSMCLI_HAVE_OTEL_GRPC
        if (cfg.otelProtocol == "grpc") {
            otel::exporter::otlp::OtlpGrpcExporterOptions opts;
            opts.endpoint = std::string(cfg.otelEndpoint.data(), cfg.otelEndpoint.size());
            opts.use_ssl_credentials = false;
            auto exporter =
                std::unique_ptr<otel_trace::SpanExporter>(
                    new otel::exporter::otlp::OtlpGrpcExporter(opts));
            otel_trace::BatchSpanProcessorOptions procOpts;
            auto processor = std::unique_ptr<otel_trace::SpanProcessor>(
                new otel_trace::BatchSpanProcessor(std::move(exporter), procOpts));
            ctx.tracerProvider =
                std::make_shared<otel_trace::TracerProvider>(std::move(processor), resource);
            otel::trace::Provider::SetTracerProvider(ctx.tracerProvider);
            tracerInitialized = true;
        }
#endif
        if (!tracerInitialized && proto == "file") {
            auto exporter = std::unique_ptr<otel_trace::SpanExporter>(
                new FileSpanExporter(fileExportPath.string()));
            otel_trace::BatchSpanProcessorOptions procOpts;
            auto processor = std::unique_ptr<otel_trace::SpanProcessor>(
                new otel_trace::BatchSpanProcessor(std::move(exporter), procOpts));
            ctx.tracerProvider =
                std::make_shared<otel_trace::TracerProvider>(std::move(processor), resource);
            otel::trace::Provider::SetTracerProvider(ctx.tracerProvider);
            tracerInitialized = true;
        }
        if (!tracerInitialized) {
            otel::exporter::otlp::OtlpHttpExporterOptions opts;
            opts.url = std::string(cfg.otelEndpoint.data(), cfg.otelEndpoint.size());
            auto exporter =
                std::unique_ptr<otel_trace::SpanExporter>(
                    new otel::exporter::otlp::OtlpHttpExporter(opts));
            otel_trace::BatchSpanProcessorOptions procOpts;
            auto processor = std::unique_ptr<otel_trace::SpanProcessor>(
                new otel_trace::BatchSpanProcessor(std::move(exporter), procOpts));
            ctx.tracerProvider =
                std::make_shared<otel_trace::TracerProvider>(std::move(processor), resource);
            otel::trace::Provider::SetTracerProvider(ctx.tracerProvider);
        }
    }

    if (cfg.enableOtelMetrics) {
        if (proto == "file") {
            if (logger) {
                QUILL_LOG_WARNING(logger,
                                  "otel protocol=file: metrics export not supported; skipping");
            }
            return ctx;
        }
#if defined(OSMCLI_HAVE_OTEL_GRPC) && OSMCLI_HAVE_OTEL_GRPC
        otel::exporter::otlp::OtlpGrpcMetricExporterOptions opts;
        opts.endpoint = std::string(cfg.otelEndpoint.data(), cfg.otelEndpoint.size());
        opts.use_ssl_credentials = false;
        auto metricExporter =
            std::unique_ptr<otel_metrics::PushMetricExporter>(
                new otel::exporter::otlp::OtlpGrpcMetricExporter(opts));
#else
        otel::exporter::otlp::OtlpHttpMetricExporterOptions opts;
        opts.url = std::string(cfg.otelEndpoint.data(), cfg.otelEndpoint.size());
        auto metricExporter =
            std::unique_ptr<otel_metrics::PushMetricExporter>(
                new otel::exporter::otlp::OtlpHttpMetricExporter(opts));
#endif
        otel_metrics::PeriodicExportingMetricReaderOptions readerOpts;
        readerOpts.export_interval_millis = std::chrono::milliseconds(1000);
        readerOpts.export_timeout_millis = std::chrono::milliseconds(1000);
        auto reader = std::unique_ptr<otel_metrics::MetricReader>(
            new otel_metrics::PeriodicExportingMetricReader(std::move(metricExporter),
                                                            readerOpts));
        auto mp = std::make_shared<otel_metrics::MeterProvider>();
        mp->AddMetricReader(std::move(reader));
        ctx.meterProvider = mp;
        otel::metrics::Provider::SetMeterProvider(ctx.meterProvider);
    }

    return ctx;
}
#endif

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
                    const std::string& authHeader,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds(15000),
                    quill::Logger* logger = nullptr)
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

    auto setDeadline = [&](auto& s) {
        beast::get_lowest_layer(s).expires_after(timeout);
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
        setDeadline(stream);
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);
        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        setDeadline(stream);
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
        setDeadline(stream);
        stream.connect(results);
        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        setDeadline(stream);
        http::read(stream, buffer, res);
        stream.socket().shutdown(tcp::socket::shutdown_both);
        checkStatus(res);
        return res.body();
    }
}

std::string addPageParams(const std::string& base, int page, int pageSize)
{
    std::string out = base;
    out += (base.find('?') == std::string::npos) ? "?" : "&";
    out += "page=" + std::to_string(page) + "&page_size=" + std::to_string(pageSize);
    return out;
}

template <class T>
std::vector<T> parseArray(const std::string& json)
{
    std::vector<T> out;
    constexpr auto opts = ::glz::opts{.error_on_unknown_keys = false};
    auto ec = ::glz::read<opts>(out, json);
    if (!ec) {
        return out;
    }
    // Try paged envelope: { "items": [...] }
    osmcli_http::PagedResponse<T> paged;
    auto ec2 = ::glz::read<opts>(paged, json);
    if (ec2) {
        throw std::runtime_error("Failed to parse HTTP response: " + ::glz::format_error(ec2, json));
    }
    return paged.items;
}

template <class T>
std::vector<T> fetchPaged(const osmcli::ConnectionConfig& cfg,
                          const std::string& baseTarget,
                          const std::string& authHeader,
                          quill::Logger* logger,
                          int pageSize = 500)
{
    std::vector<T> all;
    int page = 1;
    while (true) {
        const std::string target = addPageParams(baseTarget, page, pageSize);
        std::string body;
        try {
            body = httpGet(cfg, target, authHeader, std::chrono::milliseconds(15000), logger);
            auto chunk = parseArray<T>(body);
            if (chunk.empty()) break;
            all.insert(all.end(), chunk.begin(), chunk.end());
            if (static_cast<int>(chunk.size()) < pageSize) break;
        }
        catch (const std::exception& ex) {
            if (logger) {
                std::string snippet = body;
                if (snippet.size() > 256) snippet = snippet.substr(0, 256);
                QUILL_LOG_WARNING(logger,
                                   "[http] pagination fetch failed for {}: {} (page={}) body_snippet='{}'",
                                   target,
                                   ex.what(),
                                   page,
                                   snippet);
            }
            break;
        }
        ++page;
    }
    return all;
}

HttpAdminSummary fetchHttpAdmin(const osmcli::ConnectionConfig& cfg, quill::Logger* logger)
{
    HttpAdminSummary summary;
    const std::string user = cfg.httpAdminUser.empty()
                                 ? std::string(cfg.username.data(), cfg.username.size())
                                 : std::string(cfg.httpAdminUser.data(), cfg.httpAdminUser.size());
    const std::string pass = cfg.httpAdminPassword.empty()
                                 ? std::string(cfg.password.data(), cfg.password.size())
                                 : std::string(cfg.httpAdminPassword.data(), cfg.httpAdminPassword.size());
    const std::string auth = basicAuthHeader(user, pass);

    try {
        const auto overviewJson = httpGet(cfg, "/api/overview", auth);
        summary.overviewJson = overviewJson;
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
    const auto vhostsJson = httpGet(cfg, "/api/vhosts", auth);
    auto vhostsObj = parseArray<HttpVhost>(vhostsJson);
    summary.vhosts.reserve(vhostsObj.size());
    for (const auto& v : vhostsObj) summary.vhosts.emplace_back(v.name.data(), v.name.size());

    summary.exchanges =
        fetchPaged<HttpExchange>(cfg, "/api/exchanges?disable_stats=true", auth, logger);
    summary.queues = fetchPaged<HttpQueue>(
        cfg, "/api/queues?disable_stats=true&enable_queue_totals=true", auth, logger);
    summary.bindings = fetchPaged<HttpBinding>(cfg, "/api/bindings", auth, logger);
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

template <class T>
struct meta<osmcli_http::PagedResponse<T> > {
    using Obj = osmcli_http::PagedResponse<T>;
    static constexpr auto value = object("items", &Obj::items);
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
               "overview", &T::overview,
               "skippedExclusive", &T::skippedExclusive,
               "skippedAutoDelete", &T::skippedAutoDelete,
               "filteredByWhitelist", &T::filteredByWhitelist);
};

template <>
struct meta<LatencySample> {
    using T = LatencySample;
    static constexpr auto value =
        object("queue", &T::queue,
               "duration_ns", &T::duration_ns,
               "tsc_delta", &T::tsc_delta,
               "tsc_entry", &T::tsc_entry,
               "tsc_exit", &T::tsc_exit,
               "real_entry_ns", &T::real_entry_ns,
               "real_exit_ns", &T::real_exit_ns,
               "socket_timestamp_ns", &T::socket_ts_ns);
};

template <>
struct meta<SpanJson> {
    using T = SpanJson;
    static constexpr auto value =
        object("name", &T::name,
               "trace_id", &T::trace_id,
               "span_id", &T::span_id,
               "parent_span_id", &T::parent_span_id,
               "start_unix_nano", &T::start_unix_nano,
               "end_unix_nano", &T::end_unix_nano);
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
    std::filesystem::path resolvedLogDir;
    std::string runStamp;
    {
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
        runStamp = fmt::format("{:%Y%m%d.%H%M%S}.{:09d}", tm, static_cast<int>(ns));
    }
    if (logTarget != "stdout" && logTarget != "stderr") {
        resolvedLogDir = std::filesystem::path(logTarget).parent_path();
    }

#if OSMCLI_HAVE_OTEL
    OtelContext otelCtx;
    if (cfg.enableOtel) {
        try {
            otelCtx = initOtel(cfg, logger, resolvedLogDir);
        }
        catch (const std::exception& ex) {
            QUILL_LOG_WARNING(logger,
                               "failed to init OpenTelemetry (continuing without): {}",
                               ex.what());
        }
    }
#endif

    // Bridge rmqcpp's BALL logging into Quill to avoid UNINITIALIZED_LOGGER_MANAGER noise.
    ball::LoggerManagerConfiguration ballConfig;
    const auto ballMin = parseBallSeverity(cfg.ballMinSeverity);
    // Capture everything from BALL and forward to Quill: record/pass at chosen min,
    // trigger at ERROR, abort at FATAL.
    ballConfig.setDefaultThresholdLevelsIfValid(ballMin,
                                                ballMin,
                                                ball::Severity::e_ERROR,
                                                ball::Severity::e_FATAL);
    ball::LoggerManagerScopedGuard ballGuard(ballConfig);
    auto ballObserver =
        bsl::shared_ptr<ball::Observer>(new QuillBallObserver(logger));
    ball::LoggerManager::singleton().registerObserver(ballObserver, "quill");

    if (cfg.enableHttpAdmin) {
        try {
            auto httpSummary = fetchHttpAdmin(cfg, logger);
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
                std::filesystem::path overviewPath;
                if (!cfg.overviewCachePath.empty()) {
                    overviewPath = std::filesystem::path(
                        std::string(cfg.overviewCachePath.data(), cfg.overviewCachePath.size()));
                }
                else if (!resolvedLogDir.empty()) {
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
                    overviewPath = resolvedLogDir / fmt::format("http_overview-{}.json", timestamp);
                }
                if (!overviewPath.empty()) {
                    try {
                        if (overviewPath.has_parent_path()) {
                            std::filesystem::create_directories(overviewPath.parent_path());
                        }
                        std::string body;
                        if (!httpSummary.overviewJson.empty()) {
                            body = httpSummary.overviewJson;
                        }
                        else {
                            auto bodyExp = ::glz::write_json(*httpSummary.overview);
                            if (!bodyExp) {
                                throw std::runtime_error("failed to serialize overview JSON");
                            }
                            body = *bodyExp;
                        }
                        std::ofstream out(overviewPath);
                        out << body;
                        QUILL_LOG_INFO(logger,
                                       "[http] wrote overview JSON to {}",
                                       overviewPath.string());
                    }
                    catch (const std::exception& ex) {
                        QUILL_LOG_WARNING(logger,
                                          "[http] failed to write overview JSON to {}: {}",
                                          overviewPath.string(),
                                          ex.what());
                    }
                }
            }

            // Use live queue list from HTTP instead of stale definitions
            std::vector<std::string> discovered;
            std::vector<std::string> skippedExclusive;
            std::vector<std::string> skippedAutoDelete;
            std::vector<std::string> filteredWhitelist;
            std::unordered_set<std::string> whitelistSet;
            for (const auto& qn : cfg.queueWhitelist) {
                whitelistSet.emplace(qn.data(), qn.size());
            }
            for (const auto& q : httpSummary.queues) {
                if (q.vhost == std::string(cfg.vhost.data(), cfg.vhost.size())) {
                    const std::string qname(q.name.data(), q.name.size());
                    if (q.exclusive) {
                        skippedExclusive.emplace_back(qname);
                        continue;
                    }
                    if (cfg.skipAutoDeleteQueues && q.auto_delete) {
                        skippedAutoDelete.emplace_back(qname);
                        continue;
                    }
                    if (!whitelistSet.empty() && !whitelistSet.count(qname)) {
                        filteredWhitelist.emplace_back(qname);
                        continue;
                    }
                    discovered.emplace_back(qname);
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
                const std::string includedList = fmt::format("{}", fmt::join(discovered, ","));
                QUILL_LOG_INFO(logger,
                               "[http] using live queue list for vhost={} count={} included=[{}]",
                               std::string(cfg.vhost.data(), cfg.vhost.size()),
                               cfg.queues.size(),
                               includedList);
                if (!skippedExclusive.empty()) {
                    const std::string skipped =
                        fmt::format("{}", fmt::join(skippedExclusive, ","));
                    httpSummary.skippedExclusive = skippedExclusive;
                    QUILL_LOG_INFO(logger,
                                   "[http] skipped exclusive queues (not safe to attach consumers): {}",
                                   skipped);
                }
                if (!skippedAutoDelete.empty()) {
                    const std::string skipped =
                        fmt::format("{}", fmt::join(skippedAutoDelete, ","));
                    httpSummary.skippedAutoDelete = skippedAutoDelete;
                    QUILL_LOG_INFO(logger,
                                   "[http] skipped auto-delete queues (disabled via config): {}",
                                   skipped);
                }
                if (!filteredWhitelist.empty()) {
                    dedupeStrings(filteredWhitelist);
                    httpSummary.filteredByWhitelist = filteredWhitelist;
                    const std::string filtered =
                        fmt::format("{}", fmt::join(filteredWhitelist, ","));
                    QUILL_LOG_INFO(logger,
                                   "[http] filtered by whitelist (not included): {}",
                                   filtered);
                }
            } else {
                QUILL_LOG_ERROR(logger,
                                "[http] no queues discovered for vhost={} (falling back to configured/definitions list)",
                                std::string(cfg.vhost.data(), cfg.vhost.size()));
            }

            // Persist full HTTP summary to JSON for offline comparison.
            if (!resolvedLogDir.empty()) {
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
                std::filesystem::path summaryPath =
                    resolvedLogDir / fmt::format("http_summary-{}.json", timestamp);
                try {
                    if (summaryPath.has_parent_path()) {
                        std::filesystem::create_directories(summaryPath.parent_path());
                    }
                    auto jsonExp = ::glz::write_json(httpSummary);
                    if (!jsonExp) {
                        throw std::runtime_error("failed to serialize HTTP summary");
                    }
                    std::ofstream out(summaryPath);
                    out << *jsonExp;
                    QUILL_LOG_INFO(logger,
                                   "[http] wrote full HTTP summary to {}",
                                   summaryPath.string());
                }
                catch (const std::exception& ex) {
                    QUILL_LOG_WARNING(logger,
                                      "[http] failed to write HTTP summary: {}",
                                      ex.what());
                }
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
    } else if (cfg.enableHttpAdmin && !cfg.definitionsJsonPath.empty()) {
        QUILL_LOG_WARNING(logger,
                          "enable_http_admin=true: ignoring definitions_json={}",
                          std::string(cfg.definitionsJsonPath.data(), cfg.definitionsJsonPath.size()));
    }

    try {
        App app;
        app.logger = logger;
#if OSMCLI_HAVE_OTEL
        if (cfg.enableOtel && otelCtx.tracerProvider) {
            app.tracer = otelCtx.tracerProvider->GetTracer("order_status_monitor_cli");
        }
        if (cfg.enableOtel && otelCtx.meterProvider) {
            app.meter = otelCtx.meterProvider->GetMeter("order_status_monitor_cli");
            if (app.meter) {
                app.counterMessages = app.meter->CreateUInt64Counter("osm.messages",
                    "Messages consumed");
                app.counterEnqueueFails = app.meter->CreateUInt64Counter("osm.enqueue.failures",
                    "Consumer enqueue failures");
            }
        }
#endif
        rmqa::RabbitContextOptions opts;
        // Single-threaded callback pool
        // Increase queue depth to avoid drop when consuming many queues.
        app.threadPool = bsl::make_unique<ThreadPool>(
            ThreadAttributes(), 1, 1, cfg.threadPoolQueueDepth);
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
                auto onMessage = [logger, &app, qnameStd, &resolvedLogDir, runStamp](rmqp::MessageGuard& guard) {
                    const auto& msg = guard.message();
                    const auto& env = guard.envelope();
                    std::string exchange(env.exchange().data(),
                                         env.exchange().size());
                    std::string routingKey(env.routingKey().data(),
                                           env.routingKey().size());
                    const auto entrySteady = std::chrono::steady_clock::now();
                    const std::uint64_t entryRealNs = TW::getRealtimeNs();
                    const std::uint64_t entryTsc = TW::getTSC();
                    const auto writeLatencySample = [&](std::int64_t durNs,
                                                       std::uint64_t exitTsc,
                                                       std::uint64_t exitRealNs,
                                                       std::optional<std::uint64_t> socketTsNs = std::nullopt) {
                        if (resolvedLogDir.empty()) return;
                        std::filesystem::path latPath =
                            resolvedLogDir / fmt::format("latency_samples-{}.ndjson", runStamp);
                        try {
                            if (latPath.has_parent_path()) {
                                std::filesystem::create_directories(latPath.parent_path());
                            }
                            LatencySample sample;
                            sample.queue = qnameStd;
                            sample.duration_ns = durNs;
                            sample.tsc_delta = exitTsc - entryTsc;
                            sample.tsc_entry = entryTsc;
                            sample.tsc_exit = exitTsc;
                            sample.real_entry_ns = entryRealNs;
                            sample.real_exit_ns = exitRealNs;
                            sample.socket_ts_ns = socketTsNs;
                            auto jsonExp = ::glz::write_json(sample);
                            if (!jsonExp) {
                                QUILL_LOG_WARNING(logger, "failed to serialize latency sample");
                                return;
                            }
                            std::ofstream out(latPath, std::ios::app);
                            out << *jsonExp << "\n";
                        }
                        catch (const std::exception& ex) {
                            QUILL_LOG_WARNING(logger, "failed to write latency sample: {}", ex.what());
                        }
                    };
                    ScopeExit exitGuard([&] {
                        const auto exitSteady = std::chrono::steady_clock::now();
                        const std::uint64_t exitRealNs = TW::getRealtimeNs();
                        const std::uint64_t exitTsc = TW::getTSC();
                        const auto durNs =
                            std::chrono::duration_cast<std::chrono::nanoseconds>(exitSteady - entrySteady).count();
                        QUILL_LOG_DEBUG(logger,
                                        "[latency] queue={} dt_ns={} tsc_entry={} tsc_exit={} real_entry={} real_exit={}",
                                        qnameStd,
                                        durNs,
                                        entryTsc,
                                        exitTsc,
                                        entryRealNs,
                                        exitRealNs);
                        writeLatencySample(durNs, exitTsc, exitRealNs);
                    });
#if OSMCLI_HAVE_OTEL
                    if (app.counterMessages) {
                        app.counterMessages->Add(1, {{"queue", qnameStd}});
                    }
#endif
                    app.messages.fetch_add(1, std::memory_order_relaxed);
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
                app.enqueueFailures.fetch_add(1, std::memory_order_relaxed);
#if OSMCLI_HAVE_OTEL
                if (app.counterEnqueueFails) {
                    app.counterEnqueueFails->Add(1, {{"queue", qnameStd}});
                }
#endif
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

        // Health summary logger (periodic)
        std::atomic<bool> summaryStop{false};
        std::unique_ptr<std::thread> summaryThread = std::make_unique<std::thread>([&] {
            while (!summaryStop.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                const auto msgs = app.messages.exchange(0);
                const auto fails = app.enqueueFailures.exchange(0);
                QUILL_LOG_INFO(logger,
                               "[health] last 10s: messages={} enqueue_failures={}",
                               msgs,
                               fails);
            }
        });

        while (!stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        for (auto& c : app.consumers) {
            c->cancel();
        }
        if (timerThread) timerThread->join();
        summaryStop.store(true);
        if (summaryThread) summaryThread->join();
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
