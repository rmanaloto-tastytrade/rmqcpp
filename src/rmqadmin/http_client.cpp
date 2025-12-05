#include "http_client.h"

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <bsl_iostream.h>
#include <bsl_string_view.h>

namespace rmqadmin {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

namespace {
struct ParsedUrl {
    bsl::string host;
    bsl::string port{"15672"};
    bsl::string target{"/"};
    bool valid{false};
};

ParsedUrl parseBase(const bsl::string& base)
{
    ParsedUrl out;
    bsl::string_view v(base.data(), base.size());
    // naive parse: [http[s]://]host[:port][/path]
    if (v.rfind("http://", 0) == 0) {
        v.remove_prefix(7);
    }
    else if (v.rfind("https://", 0) == 0) {
        v.remove_prefix(8);
    }
    auto slash = v.find('/');
    bsl::string_view hostport = slash == bsl::string_view::npos ? v : v.substr(0, slash);
    if (slash != bsl::string_view::npos) {
        out.target.assign(v.substr(slash).data(), v.substr(slash).size());
    }
    auto colon = hostport.find(':');
    if (colon == bsl::string_view::npos) {
        out.host.assign(hostport.data(), hostport.size());
    }
    else {
        out.host.assign(hostport.substr(0, colon).data(), colon);
        out.port.assign(hostport.substr(colon + 1).data(), hostport.size() - colon - 1);
    }
    out.valid = !out.host.empty();
    if (out.target.empty()) out.target = "/";
    return out;
}

}  // namespace

HttpClient::HttpClient(const AdminConfig& config, net::io_context& io)
: d_config(config)
, d_io(io)
{
}

Response HttpClient::perform(const Command& cmd)
{
    Response r;

    ParsedUrl url = parseBase(d_config.baseUrl);
    if (!url.valid) {
        r.statusCode = 400;
        r.error = "Invalid base URL";
        return r;
    }

    // Build target: /api/{resource}
    bsl::string target = url.target;
    if (target.back() == '/') target.pop_back();
    target += "/api/" + cmd.resource;

    beast::tcp_stream stream(d_io);
    net::ip::tcp::resolver resolver(d_io);
    beast::error_code ec;
    auto const results = resolver.resolve(url.host, url.port, ec);
    if (ec) {
        r.statusCode = 503;
        r.error = ec.message();
        return r;
    }
    stream.connect(results, ec);
    if (ec) {
        r.statusCode = 503;
        r.error = ec.message();
        return r;
    }

    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, url.host);
    req.set(http::field::user_agent, "rmqadmin-cpp");
    if (!d_config.username.empty()) {
        bsl::string creds = d_config.username + ":" + d_config.password;
        bsl::string auth = "Basic " + bsl::string(beast::detail::base64_encode(creds));
        req.set(http::field::authorization, auth);
    }

    http::write(stream, req, ec);
    if (ec) {
        r.statusCode = 503;
        r.error = ec.message();
        return r;
    }

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res, ec);
    if (ec) {
        r.statusCode = 503;
        r.error = ec.message();
        return r;
    }

    stream.socket().shutdown(net::ip::tcp::socket::shutdown_both, ec);

    r.statusCode = static_cast<int>(res.result_int());
    r.body = std::move(res.body());
    r.contentType = res[http::field::content_type].to_string().c_str();
    return r;
}

}  // namespace rmqadmin
