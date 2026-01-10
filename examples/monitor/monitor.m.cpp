#include <rmqa_connectionstring.h>
#include <rmqa_consumer.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_topology.h>
#include <rmqa_vhost.h>
#include <rmqp_consumer.h>
#include <rmqt_fieldvalue.h>
#include <rmqt_message.h>
#include <rmqt_result.h>

#include <CLI/CLI.hpp>
#include <bsl_chrono.h>
#include <bsl_iostream.h>
#include <bsl_memory_resource.h>
#include <bsl_optional.h>
#include <bsl_set.h>
#include <bsl_string.h>
#include <bsl_vector.h>

#include <bslmt_threadutil.h>
#include <string>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>

using namespace BloombergLP;
namespace pmr = bsl::pmr;

namespace {
struct Flags {
    bsl::string amqpUri;
    bool enableEvent;
    bool enableFirehose;
    bool enableMetadataStore;
    bool enableLog;
    bool enableManagementPoller;
    bsl::string managementUrl;
    Flags()
    : amqpUri("amqp://guest:guest@localhost:5672")
    , enableEvent(true)
    , enableFirehose(false)
    , enableMetadataStore(true)
    , enableLog(false)
    , enableManagementPoller(false)
    , managementUrl("")
    {
    }
};

void logMessage(const bsl::string& source,
                const rmqt::Message& message,
                const rmqt::Envelope& envelope,
                quill::Logger* logger)
{
    // Convert bsl::string to std::string to use Quill codecs
    std::string src(source.data(), source.size());
    std::string rk(envelope.routingKey().data(), envelope.routingKey().size());
    std::string exch(envelope.exchange().data(), envelope.exchange().size());
    QUILL_LOG_INFO(logger,
                   "{} rk={} exchange={} payload_size={} redelivered={}",
                   src,
                   rk,
                   exch,
                   static_cast<unsigned long>(message.payloadSize()),
                   envelope.redelivered());
}

rmqt::Result<rmqa::Consumer> createConsumer(rmqa::VHost& vhost,
                                            rmqa::Topology& topology,
                                            const rmqt::QueueHandle& queue,
                                            const bsl::string& name,
                                            quill::Logger* logger)
{
    rmqt::ConsumerConfig cfg;
    cfg.setConsumerTag(name);
    cfg.setPrefetchCount(50);

    return vhost.createConsumer(
        topology,
        queue,
        [name, logger](rmqp::MessageGuard& guard) {
            logMessage(name, guard.message(), guard.envelope(), logger);
            guard.ack();
        },
        cfg);
}

rmqt::QueueHandle bindQueue(rmqa::Topology& topology,
                            const rmqt::ExchangeHandle& exch,
                            const bsl::string& queueName,
                            const bsl::string& routingKey)
{
    rmqt::QueueHandle queue = topology.addQueue(queueName);
    topology.bind(exch, queue, routingKey);
    return queue;
}

} // namespace

int main(int argc, char** argv)
{
    quill::BackendOptions backendOptions;
    quill::Backend::start(backendOptions);
    auto sink =
        quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
    quill::Logger* logger =
        quill::Frontend::create_or_get_logger("rmqmonitor", std::move(sink));

    Flags flags;
    CLI::App app{"rmqmonitor"};
    app.add_option("amqp-uri", flags.amqpUri, "AMQP URI (amqp://user:pass@host:port/vhost)")->required();
    app.add_flag_function("--no-event",
                          [&](std::int64_t) { flags.enableEvent = false; },
                          "Disable event exchange consumer");
    app.add_flag("--firehose", flags.enableFirehose, "Enable firehose consumer (amq.rabbitmq.trace)");
    app.add_flag_function("--metadata",
                          [&](std::int64_t) { flags.enableMetadataStore = true; },
                          "Enable metadata store consumer (default on)");
    app.add_flag_function("--no-metadata",
                          [&](std::int64_t) { flags.enableMetadataStore = false; },
                          "Disable metadata store consumer");
    app.add_flag("--log-exchange", flags.enableLog, "Enable log exchange consumer (amq.rabbitmq.log)");
    app.add_option("--management-url", flags.managementUrl, "Management API base URL (HTTPS) for future polling");
    CLI11_PARSE(app, argc, argv);

    bsl::optional<rmqt::VHostInfo> vhostInfo =
        rmqa::ConnectionString::parse(flags.amqpUri);

    if (!vhostInfo) {
        bsl::cerr << "Failed to parse AMQP URI: " << flags.amqpUri << bsl::endl;
        return 1;
    }

    rmqa::RabbitContext rabbit;
    bsl::shared_ptr<rmqa::VHost> vhost = rabbit.createVHostConnection(
        "monitor", vhostInfo.value());

    rmqa::Topology topology;

    // Use a small pmr arena to avoid heap traffic for container bookkeeping.
    char consumerArena[16 * 1024];
    pmr::monotonic_buffer_resource consumerPool(consumerArena,
                                                sizeof consumerArena);
    pmr::vector<bsl::shared_ptr<rmqa::Consumer> > consumers{&consumerPool};

    // Event exchange consumer
    if (flags.enableEvent) {
        pmr::string exchName("amq.rabbitmq.event", &consumerPool);
        pmr::string queueName("monitor.events", &consumerPool);
        rmqt::ExchangeHandle eventExch = topology.addExchange(exchName);
        rmqt::QueueHandle eventQueue =
            bindQueue(topology, eventExch, queueName, "#");
        rmqt::Result<rmqa::Consumer> eventConsumer =
            createConsumer(*vhost, topology, eventQueue, "monitor-event", logger);
        if (eventConsumer) {
            consumers.push_back(eventConsumer.value());
        }
        else {
            bsl::cerr << "Event exchange not available or consumer failed: "
                      << eventConsumer.error() << bsl::endl;
        }
    }

    // Firehose consumer (optional)
    if (flags.enableFirehose) {
        pmr::string exchName("amq.rabbitmq.trace", &consumerPool);
        pmr::string queueName("monitor.firehose", &consumerPool);
        rmqt::ExchangeHandle fireExch = topology.addExchange(exchName);
        rmqt::QueueHandle fireQueue =
            bindQueue(topology, fireExch, queueName, "#");
        rmqt::Result<rmqa::Consumer> firehoseConsumer =
            createConsumer(*vhost, topology, fireQueue, "monitor-firehose", logger);
        if (firehoseConsumer) {
            consumers.push_back(firehoseConsumer.value());
        }
        else {
            bsl::cerr << "Firehose not available or consumer failed: "
                      << firehoseConsumer.error() << bsl::endl;
        }
    }

    // Metadata store (optional; queue/exchange names depend on plugin config)
    if (flags.enableMetadataStore) {
        pmr::string exchName("amq.rabbitmq.metadata", &consumerPool);
        pmr::string queueName("monitor.metadata", &consumerPool);
        rmqt::ExchangeHandle metaExch = topology.addExchange(exchName);
        rmqt::QueueHandle metaQueue =
            bindQueue(topology, metaExch, queueName, "#");
        rmqt::Result<rmqa::Consumer> metadataConsumer =
            createConsumer(*vhost, topology, metaQueue, "monitor-metadata", logger);
        if (metadataConsumer) {
            consumers.push_back(metadataConsumer.value());
        }
        else {
            bsl::cerr << "Metadata store not available or consumer failed: "
                      << metadataConsumer.error() << bsl::endl;
        }
    }

    // Log exchange consumer (internal events)
    if (flags.enableLog) {
        pmr::string exchName("amq.rabbitmq.log", &consumerPool);
        pmr::string queueName("monitor.log", &consumerPool);
        rmqt::ExchangeHandle logExch = topology.addExchange(exchName);
        rmqt::QueueHandle logQueue =
            bindQueue(topology, logExch, queueName, "#");
        rmqt::Result<rmqa::Consumer> logConsumer =
            createConsumer(*vhost, topology, logQueue, "monitor-log", logger);
        if (logConsumer) {
            consumers.push_back(logConsumer.value());
        }
        else {
            bsl::cerr << "Log exchange not available or consumer failed: "
                      << logConsumer.error() << bsl::endl;
        }
    }

    if (flags.enableManagementPoller && !flags.managementUrl.empty()) {
        bsl::cerr << "Management API polling not implemented; supply HTTP client"
                  << bsl::endl;
    }

    // Keep running until interrupted
    while (true) {
        bslmt::ThreadUtil::microSleep(0, 5 * 1000 * 1000);
    }
}
