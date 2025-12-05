#include "amqp.h"

#include <rmqa_connectionstring.h>

#include <bsl_iostream.h>

namespace rmqadmin {

AmqpClient::AmqpClient(const AdminConfig& config, void* logger)
: d_config(config)
, d_logger(logger)
{
}

Response AmqpClient::perform(const Command& cmd)
{
    Response r;
    r.statusCode = 501;
    r.error = "AMQP backend not implemented yet for verb";
    return r;
}

}  // namespace rmqadmin
