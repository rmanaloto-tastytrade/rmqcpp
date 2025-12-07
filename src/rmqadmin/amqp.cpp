#include "amqp.h"

#include <rmqa_connectionstring.h>
#include <rmqa_producer.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_topology.h>
#include <rmqa_vhost.h>

#include <rmqt_endpoint.h>
#include <rmqp_producer.h>
#include <rmqt_simpleendpoint.h>
#include <rmqt_message.h>
#include <rmqt_vhostinfo.h>

#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include <bsls_timeinterval.h>
#include <bsl_memory.h>
#include <bsl_vector.h>
#include <string>

namespace rmqadmin {

AmqpClient::AmqpClient(const AdminConfig& config, void* logger)
: d_config(config)
, d_logger(logger)
{
}

Response AmqpClient::perform(const Command& cmd)
{
    Response r;
    if (cmd.verb != Verb::Publish) {
        r.statusCode = 501;
        r.error      = "AMQP backend implemented only for publish";
        return r;
    }

    if (d_config.amqpUri.empty()) {
        r.statusCode = 400;
        r.error      = "amqp-uri is required for AMQP backend";
        return r;
    }

    auto vinfoOpt = BloombergLP::rmqa::ConnectionString::parse(d_config.amqpUri);
    if (!vinfoOpt) {
        r.statusCode = 400;
        r.error      = "invalid amqp-uri";
        return r;
    }
    BloombergLP::rmqt::VHostInfo vinfo = *vinfoOpt;
    // Normalize vhost: default to "/", and decode %2F if present.
    bsl::string vhostPath = vinfo.endpoint()->vhost();
    if (vhostPath.empty()) {
        vhostPath = "/";
    }
    else {
        // Decode a literal %2F/%2f into "/"
        const bsl::string needle1 = "%2F";
        const bsl::string needle2 = "%2f";
        auto replaceAll = [](bsl::string& s, const bsl::string& needle, const bsl::string& repl) {
            size_t pos = 0;
            while ((pos = s.find(needle, pos)) != bsl::string::npos) {
                s.replace(pos, needle.size(), repl);
                pos += repl.size();
            }
        };
        replaceAll(vhostPath, needle1, "/");
        replaceAll(vhostPath, needle2, "/");
    }
    if (vhostPath != vinfo.endpoint()->vhost()) {
        auto ep   = vinfo.endpoint();
        auto host = ep->hostname();
        auto port = ep->port();
        auto newEp = bsl::make_shared<BloombergLP::rmqt::SimpleEndpoint>(
            host, vhostPath, port);
        vinfo = BloombergLP::rmqt::VHostInfo(newEp, vinfo.credentials());
    }

    bsl::string exchangeName = "amq.default";
    if (auto it = cmd.params.find("exchange"); it != cmd.params.end() &&
        !it->second.empty()) {
        exchangeName = it->second;
    }

    bsl::string routingKey;
    if (auto it = cmd.params.find("routing_key"); it != cmd.params.end()) {
        routingKey = it->second;
    }
    if (routingKey.empty()) {
        r.statusCode = 400;
        r.error      = "routing_key is required for AMQP publish";
        return r;
    }

    auto logger = static_cast<quill::Logger*>(d_logger);

    BloombergLP::rmqa::RabbitContext ctx;
    auto vhost = ctx.createVHostConnection("rmqadmin", vinfo);
    if (!vhost) {
        r.statusCode = 503;
        r.error      = "failed to create AMQP vhost";
        if (logger) {
            QUILL_LOG_ERROR(logger, "AMQP vhost creation failed for {}",
                            std::string(d_config.amqpUri.data(), d_config.amqpUri.size()));
            }
        return r;
    }

    BloombergLP::rmqa::Topology topology;
    BloombergLP::rmqt::ExchangeHandle exchangeHandle;
    if (exchangeName.empty()) {
        // No exchange provided: use the default exchange ("")
        exchangeHandle = topology.defaultExchange();
    }
    else {
        // Respect explicit exchange names (including amq.default)
        exchangeHandle = topology.addPassiveExchange(exchangeName);
    }

    // Build message payload
    bsl::shared_ptr<bsl::vector<uint8_t>> data(
        new bsl::vector<uint8_t>(cmd.body.begin(), cmd.body.end()));
    BloombergLP::rmqt::Message message(data);

    uint16_t maxConfirms = 16;
    BloombergLP::rmqt::Result<BloombergLP::rmqa::Producer> prodRes =
        vhost->createProducer(topology, exchangeHandle, maxConfirms);
    if (!prodRes) {
        r.statusCode = 503;
        r.error      = prodRes.error();
        if (logger) {
            QUILL_LOG_ERROR(logger, "AMQP producer create failed: {}",
                            std::string(prodRes.error().data(), prodRes.error().size()));
        }
        return r;
    }

    auto prod = prodRes.value();

    // Capture returns to surface NO_ROUTE/returns.
    bool returned = false;
    auto confirmCb = [&](const BloombergLP::rmqt::Message&,
                         const bsl::string&,
                         const BloombergLP::rmqt::ConfirmResponse& resp) {
        if (resp.status() == BloombergLP::rmqt::ConfirmResponse::RETURN) {
            returned = true;
        }
    };

    BloombergLP::rmqp::Producer::SendStatus status =
        prod->send(message,
                   routingKey,
                   confirmCb,
                   BloombergLP::bsls::TimeInterval(1));

    if (status == BloombergLP::rmqp::Producer::SENDING && !returned) {
        r.statusCode  = 200;
        r.contentType = "text/plain";
        r.body        = "AMQP publish ok";
        if (logger) {
            QUILL_LOG_INFO(logger,
                           "AMQP publish exchange={} rk={} bytes={} status={}",
                           std::string(exchangeName.data(), exchangeName.size()),
                           std::string(routingKey.data(), routingKey.size()),
                           cmd.body.size(),
                           static_cast<int>(status));
        }
    }
    else {
        r.statusCode = 502;
        r.error      = returned ? "AMQP publish returned (NO_ROUTE?)"
                                : "AMQP send failed";
        if (logger) {
            QUILL_LOG_ERROR(logger,
                            "AMQP publish failed status={} returned={} exchange={} rk={}",
                            static_cast<int>(status),
                            returned,
                            std::string(exchangeName.data(), exchangeName.size()),
                            std::string(routingKey.data(), routingKey.size()));
        }
    }
    return r;
}

}  // namespace rmqadmin
