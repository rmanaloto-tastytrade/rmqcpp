#include "http_client.h"

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>

#include <bsl_iostream.h>
#include <bsl_string_view.h>
#include <cctype>

namespace rmqadmin {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

namespace {
struct ParsedUrl {
    bsl::string host;
    bsl::string port{"15672"};
    bsl::string target{"/"};
    bool https{false};
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

static bsl::string jsonEscape(const bsl::string& in)
{
    bsl::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof buf, "\\u%04x", c & 0xFF);
                    out += buf;
                }
                else {
                    out.push_back(c);
                }
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
        out.https = true;
        out.port = "443";
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

    bsl::string resourcePath;
    // Special cases for publish/get
    if (cmd.verb == Verb::Publish) {
        bsl::string exch = "amq.default";
        if (auto it = cmd.params.find("exchange"); it != cmd.params.end()) exch = it->second;
        resourcePath = "/api/exchanges/" + urlEncode(d_config.vhost) + "/" + urlEncode(exch) + "/publish";
        bsl::string routingKey;
        if (auto it = cmd.params.find("routing_key"); it != cmd.params.end()) routingKey = it->second;
        bsl::string payload = cmd.body.empty() ? bsl::string() : cmd.body;
        bsl::string json = "{";
        json += "\"properties\":{},";
        json += "\"routing_key\":\"" + jsonEscape(routingKey) + "\",";
        json += "\"payload\":\"" + jsonEscape(payload) + "\",";
        json += "\"payload_encoding\":\"string\"";
        json += "}";
        reqBody = json;
    } else if (cmd.verb == Verb::Get) {
        bsl::string queue;
        if (auto it = cmd.params.find("queue"); it != cmd.params.end()) queue = it->second;
        if (queue.empty()) {
            r.statusCode = 400;
            r.error = "queue is required for get";
            return r;
        }
        resourcePath = "/api/queues/" + urlEncode(d_config.vhost) + "/" + urlEncode(queue) + "/get";
        bsl::string json = "{";
        json += "\"count\":1,";
        json += "\"ackmode\":\"ack_requeue_true\",";
        json += "\"encoding\":\"auto\"";
        json += "}";
        reqBody = json;
    } else {
        resourcePath = "/api/" + cmd.resource;
        if (needsVhost(cmd.resource)) {
            resourcePath += "/" + urlEncode(d_config.vhost);
        }
        // For show/delete, append name if provided.
        if (cmd.verb == Verb::Show || cmd.verb == Verb::Delete || cmd.verb == Verb::Declare) {
            auto it = cmd.params.find("name");
            if (it == cmd.params.end() || it->second.empty()) {
                r.statusCode = 400;
                r.error = "name is required for show/delete/declare";
                return r;
            }
            resourcePath += "/" + urlEncode(it->second);
        }
        if (cmd.verb == Verb::Declare) {
            // Very minimal declare body for queues/exchanges
            bsl::string json = "{";
            json += "\"auto_delete\":false,\"durable\":true,\"arguments\":{}";
            if (cmd.resource == "exchanges") {
                auto itType = cmd.params.find("type");
                if (itType != cmd.params.end()) {
                    json += ",\"type\":\"" + jsonEscape(itType->second) + "\"";
                }
            }
            json += "}";
            reqBody = json;
        }
    }

    target += resourcePath;

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
            method = http::verb::post;
            break;
        case Verb::Declare:
            method = http::verb::put;
            break;
        case Verb::Get:
            method = http::verb::post;
            break;
        default:
            method = http::verb::get;
            break;
    }

    http::request<http::string_body> req{method, target, 11};
    if (!reqBody.empty()) {
        req.body() = reqBody;
        req.set(http::field::content_type, "application/json");
        req.prepare_payload();
    }
    req.set(http::field::host, url.host);
    req.set(http::field::user_agent, "rmqadmin-cpp");
    if (!d_config.username.empty()) {
        bsl::string creds = d_config.username + ":" + d_config.password;
        // Use Beast base64; acceptable here for a client CLI.
        bsl::string auth =
            "Basic " + bsl::string(beast::detail::base64_encode(creds));
        req.set(http::field::authorization, auth);
    }

    beast::flat_buffer buffer;
    http::response<http::string_body> res;

    auto handle_response = [&](auto& stream, auto& ecIn) -> bool {
        http::write(stream, req, ecIn);
        if (ecIn) return false;
        http::read(stream, buffer, res, ecIn);
        if (ecIn) return false;
        return true;
    };

    beast::error_code ec;
    if (url.https) {
        ssl::context ctx(ssl::context::tls_client);
        ctx.set_default_verify_paths();
        if (d_config.tlsInsecure) {
            ctx.set_verify_mode(ssl::verify_none);
        }
        else {
            ctx.set_verify_mode(ssl::verify_peer);
        }
        ssl::stream<beast::tcp_stream> stream(d_io, ctx);
        net::ip::tcp::resolver resolver(d_io);
        auto const results = resolver.resolve(url.host, url.port, ec);
        if (ec) { r.statusCode = 503; r.error = ec.message(); return r; }
        beast::get_lowest_layer(stream).connect(results, ec);
        if (ec) { r.statusCode = 503; r.error = ec.message(); return r; }
        if(! SSL_set_tlsext_host_name(stream.native_handle(), url.host.c_str())) {
            ec.assign(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
        }
        stream.handshake(ssl::stream_base::client, ec);
        if (ec) { r.statusCode = 503; r.error = ec.message(); return r; }
        if (!handle_response(stream, ec)) { r.statusCode = 503; r.error = ec.message(); return r; }
        stream.shutdown(ec);
    }
    else {
        beast::tcp_stream stream(d_io);
        net::ip::tcp::resolver resolver(d_io);
        auto const results = resolver.resolve(url.host, url.port, ec);
        if (ec) { r.statusCode = 503; r.error = ec.message(); return r; }
        stream.connect(results, ec);
        if (ec) { r.statusCode = 503; r.error = ec.message(); return r; }
        if (!handle_response(stream, ec)) { r.statusCode = 503; r.error = ec.message(); return r; }
        stream.socket().shutdown(net::ip::tcp::socket::shutdown_both, ec);
    }

    r.statusCode = static_cast<int>(res.result_int());
    r.body = std::move(res.body());
    r.contentType = res[http::field::content_type].to_string().c_str();
    return r;
}

}  // namespace rmqadmin
