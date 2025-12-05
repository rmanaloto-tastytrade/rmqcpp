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

static bsl::string urlEncode(const bsl::string& in)
{
    static const char hex[] = "0123456789ABCDEF";
    bsl::string out;
    for (unsigned char c : in) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        }
        else {
            out.push_back('%');
            out.push_back(hex[(c >> 4) & 0xF]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

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

    // Build target: /api/{resource} with vhost if required
    bsl::string target = url.target;
    if (target.back() == '/') target.pop_back();
    auto needsVhost = [&](const bsl::string& res) {
        return res == "queues" || res == "exchanges" || res == "bindings" ||
               res == "consumers" || res == "permissions";
    };

    bsl::string resourcePath = "/api/" + cmd.resource;
    if (needsVhost(cmd.resource)) {
        resourcePath += "/" + urlEncode(d_config.vhost);
    }

    // For show, append name if provided.
    if (cmd.verb == Verb::Show) {
        auto it = cmd.params.find("name");
        if (it == cmd.params.end() || it->second.empty()) {
            r.statusCode = 400;
            r.error = "name is required for show";
            return r;
        }
        resourcePath += "/" + urlEncode(it->second);
    }

    target += resourcePath;

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

    http::verb method = http::verb::get;
    switch (cmd.verb) {
        case Verb::List:
        case Verb::Show:
            method = http::verb::get;
            break;
        case Verb::Delete:
            method = http::verb::delete_;
            break;
        case Verb::Publish:
        case Verb::Declare:
        case Verb::Get:
        default:
            method = http::verb::get;  // TODO: implement these verbs
            break;
    }

    http::request<http::string_body> req{method, target, 11};
    req.set(http::field::host, url.host);
    req.set(http::field::user_agent, "rmqadmin-cpp");
    if (!d_config.username.empty()) {
        bsl::string creds = d_config.username + ":" + d_config.password;
        // Use Beast base64; acceptable here for a client CLI.
        bsl::string auth =
            "Basic " + bsl::string(beast::detail::base64_encode(creds));
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
