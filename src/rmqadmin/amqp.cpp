#include "amqp.h"

#include <rmqa_connectionstring.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_vhost.h>
#include <rmqa_topology.h>
#include <rmqt_message.h>
#include <rmqio_asioeventloop.h>

#include <bsl_iostream.h>

namespace rmqadmin {

AmqpClient::AmqpClient(const AdminConfig& config)
: d_config(config)
{
}

Response AmqpClient::perform(const Command& cmd)
{
    Response r;
    if (d_config.amqpUri.empty()) {
        r.statusCode = 400;
        r.error = "AMQP URI not provided";
        return r;
    }

    bsl::optional<rmqt::VHostInfo> info = rmqa::ConnectionString::parse(d_config.amqpUri);
    if (!info) {
        r.statusCode = 400;
        r.error = "Failed to parse AMQP URI";
        return r;
    }

    // Single-threaded: drive a shared io_context and pass it to rmqcpp.
    boost::asio::io_context io;
    rmqio::AsioEventLoop loop(io);

    rmqa::RabbitContext ctx;
    bsl::shared_ptr<rmqa::VHost> vhost =
        ctx.createVHostConnection("rmqadmin", info.value(), loop);
    rmqa::Topology topo;

    if (cmd.verb == Verb::Publish) {
        bsl::string exchange = "amq.default";
        bsl::string routingKey;
        if (auto it = cmd.params.find("exchange"); it != cmd.params.end()) exchange = it->second;
        if (auto it = cmd.params.find("routing_key"); it != cmd.params.end()) routingKey = it->second;
        bsl::string payload;
        if (auto it = cmd.params.find("payload"); it != cmd.params.end()) payload = it->second;

        rmqt::ExchangeHandle exch = topo.addExchange(exchange);
        rmqt::Message msg(payload.data(), payload.size());
        rmqt::PublishOptions opts;
        rmqt::Result<rmqt::PublishResult> res =
            vhost->publish(topo, exch, routingKey, msg, opts);
        io.run();
        if (res) {
            r.statusCode = 200;
            r.contentType = "application/json";
            r.body = "{\"routed\":true}";
        }
        else {
            r.statusCode = 500;
            r.error = res.error();
        }
        return r;
    }

    if (cmd.verb == Verb::Get) {
        bsl::string queue;
        if (auto it = cmd.params.find("queue"); it != cmd.params.end()) queue = it->second;
        if (queue.empty()) {
            r.statusCode = 400;
            r.error = "queue is required for get";
            return r;
        }
        rmqt::QueueHandle qh = topo.addQueue(queue);
        rmqt::Result<rmqt::Message> res = vhost->basicGet(topo, qh, true);  // auto-ack
        io.run();
        if (res) {
            r.statusCode = 200;
            r.contentType = "text/plain";
            r.body.assign(res.value().data(), res.value().size());
        }
        else {
            r.statusCode = 500;
            r.error = res.error();
        }
        return r;
    }

    r.statusCode = 501;
    r.error = "AMQP backend not implemented for this verb";
    return r;
}

}  // namespace rmqadmin
