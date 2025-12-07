#include "config.h"

#include <rmqa_rabbitcontext.h>
#include <rmqa_vhost.h>
#include <rmqa_topology.h>
#include <rmqa_consumer.h>
#include <rmqt_securityparameters.h>
#include <rmqt_consumerconfig.h>

#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/SimpleSetup.h>
#include <quill/sinks/ConsoleSink.h>

#include <bsl_string_view.h>
#include <bsl_memory.h>
#include <bsl_vector.h>
#include <csignal>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace {
using namespace osmcli;
namespace rmqa = BloombergLP::rmqa;
namespace rmqt = BloombergLP::rmqt;
namespace rmqp = BloombergLP::rmqp;

struct App {
    bsl::unique_ptr<rmqa::RabbitContext> ctx;
    bsl::shared_ptr<rmqa::VHost> vhost;
    bsl::shared_ptr<rmqa::Consumer> consumer;
    quill::Logger* logger{nullptr};
};

}  // namespace

int main(int argc, char** argv)
{
    auto cfg = osmcli::loadConfig(argc, argv);

    // Set up quill logger
    auto logger = quill::simple_logger("stdout");

    try {
        App app;
        app.logger = logger;
        rmqa::RabbitContextOptions opts;
        app.ctx = bsl::make_unique<rmqa::RabbitContext>(opts);

        auto endpoint = cfg.endpoint();
        auto creds = cfg.credentials();
        app.vhost =
            app.ctx->createVHostConnection("osm-cli", endpoint, creds);
        if (!app.vhost) {
            QUILL_LOG_ERROR(logger, "failed to create vhost connection");
            return 1;
        }

        rmqa::Topology topology;
        auto exch = topology.addExchange(cfg.exchange,
                                         rmqt::ExchangeType::TOPIC,
                                         cfg.autoDelete ? rmqt::AutoDelete::ON
                                                        : rmqt::AutoDelete::OFF,
                                         cfg.durable ? rmqt::Durable::ON
                                                     : rmqt::Durable::OFF);
        auto directExch =
            topology.addExchange(cfg.directExchange,
                                 rmqt::ExchangeType::DIRECT,
                                 cfg.autoDelete ? rmqt::AutoDelete::ON
                                                : rmqt::AutoDelete::OFF,
                                 cfg.durable ? rmqt::Durable::ON
                                             : rmqt::Durable::OFF);
        auto queue =
            topology.addQueue(cfg.queueName,
                              cfg.autoDelete ? rmqt::AutoDelete::ON
                                             : rmqt::AutoDelete::OFF,
                              cfg.durable ? rmqt::Durable::ON
                                          : rmqt::Durable::OFF);

        for (const auto& rk : cfg.topicBindings) {
            topology.bind(exch, queue, rk);
        }
        for (const auto& rk : cfg.directBindings) {
            topology.bind(directExch, queue, rk);
        }

        auto onMessage = [&](rmqp::MessageGuard& guard) {
            const auto& msg = guard.message();
            const auto& env = guard.envelope();
            std::string exchange(env.exchange().data(), env.exchange().size());
            std::string routingKey(env.routingKey().data(),
                                   env.routingKey().size());
            QUILL_LOG_INFO(logger,
                           "delivery tag={} exchange={} rk={} bytes={}",
                           env.deliveryTag(),
                           exchange,
                           routingKey,
                           msg.payloadSize());
            guard.ack();
        };

        rmqt::ConsumerConfig cconfig;
        cconfig.setPrefetchCount(cfg.prefetch);
        auto consumerResult =
            app.vhost->createConsumer(topology, queue, onMessage, cconfig);
        if (!consumerResult) {
            const auto& err = consumerResult.error();
            QUILL_LOG_ERROR(logger,
                            "failed to create consumer: {}",
                            std::string(err.data(), err.size()));
            return 1;
        }
        app.consumer = consumerResult.value();
        QUILL_LOG_INFO(logger,
                       "connected host={} port={} vhost={} queue={} prefetch={}",
                       std::string(cfg.host.data(), cfg.host.size()),
                       cfg.port,
                       std::string(cfg.vhost.data(), cfg.vhost.size()),
                       std::string(cfg.queueName.data(), cfg.queueName.size()),
                       cfg.prefetch);

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

        app.consumer->cancel();
        if (timerThread) timerThread->join();
    }
    catch (const std::exception& ex) {
        QUILL_LOG_ERROR(logger, "fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
