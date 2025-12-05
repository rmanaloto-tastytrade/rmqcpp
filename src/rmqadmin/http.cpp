#include "http.h"

#include <bsl_iostream.h>

namespace rmqadmin {

HttpClient::HttpClient(const AdminConfig& config)
: d_config(config)
{
}

Response HttpClient::perform(const Command& cmd)
{
    // Stub implementation: a real implementation will use Boost.Beast HTTP over
    // TLS and map Command->HTTP request to the management API.
    Response r;
    r.statusCode = 501;
    r.contentType = "text/plain";
    r.body = "Not implemented: " + cmd.resource;
    return r;
}

}  // namespace rmqadmin
