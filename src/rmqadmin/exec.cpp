#include "exec.h"

#include "http_client.h"
#include "amqp.h"

namespace rmqadmin {

Executor::Executor(const AdminConfig& cfg)
: d_cfg(cfg)
{
}

Response Executor::run(const Command& cmd)
{
    if ((cmd.verb == Verb::Publish || cmd.verb == Verb::Get) &&
        cmd.backend == BackendHint::AmqpPreferred) {
        AmqpClient amqp(d_cfg);
        Response r = amqp.perform(cmd);
        if (r) return r;
        // fall back to HTTP if AMQP fails
    }

    boost::asio::io_context io;
    HttpClient client(d_cfg, io);
    return client.perform(cmd);
}

}  // namespace rmqadmin
