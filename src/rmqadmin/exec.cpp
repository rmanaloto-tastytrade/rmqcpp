#include "exec.h"

#include "http_client.h"
#include "amqp.h"

namespace rmqadmin {

Executor::Executor(const AdminConfig& cfg, void* logger)
: d_cfg(cfg)
, d_logger(logger)
{
}

Response Executor::run(const Command& cmd)
{
    if ((cmd.verb == Verb::Publish || cmd.verb == Verb::Get) &&
        cmd.backend == BackendHint::AmqpPreferred) {
        AmqpClient amqp(d_cfg, d_logger);
        Response r = amqp.perform(cmd);
        if (r) return r;
        // fall back to HTTP if AMQP fails
    }

    boost::asio::io_context io;
    HttpClient client(d_cfg, io, d_logger);
    return client.perform(cmd);
}

}  // namespace rmqadmin
