#include "http_client.h"

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <bsl_iostream.h>

namespace rmqadmin {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

HttpClient::HttpClient(const AdminConfig& config, net::io_context& io)
: d_config(config)
, d_io(io)
{
}

Response HttpClient::perform(const Command& cmd)
{
    // Minimal placeholder: return 501 until we map Command -> HTTP request.
    Response r;
    r.statusCode = 501;
    r.contentType = "text/plain";
    r.body = "HTTP backend not implemented yet for " + cmd.resource;
    return r;
}

}  // namespace rmqadmin
