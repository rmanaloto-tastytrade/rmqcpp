#include "cli.h"

#include <CLI/CLI.hpp>

namespace rmqadmin {

static Verb parseVerb(const bsl::string& verb)
{
    if (verb == "list") return Verb::List;
    if (verb == "show") return Verb::Show;
    if (verb == "declare") return Verb::Declare;
    if (verb == "delete") return Verb::Delete;
    if (verb == "publish") return Verb::Publish;
    if (verb == "get") return Verb::Get;
    return Verb::Unknown;
}

CliParseResult parseCli(int argc, char** argv)
{
    CliParseResult out;
    CLI::App app{"rmqadmin (C++26) – management HTTP client"};

    app.add_option("--url", out.config.baseUrl, "Management base URL (e.g. http://localhost:15672)")
        ->default_val("http://localhost:15672");
    app.add_option("-u,--username", out.config.username, "Username")->default_val("guest");
    app.add_option("-p,--password", out.config.password, "Password")->default_val("guest");
    app.add_option("-V,--vhost", out.config.vhost, "VHost")->default_val("/");
    app.add_option("--amqp-uri", out.config.amqpUri, "AMQP URI for publish/get via AMQP (optional)");
    app.add_flag("--tls-insecure", out.config.tlsInsecure, "Do not verify TLS certificates for HTTPS");

    // Common parameter helpers
    bsl::string name;
    app.add_option("--name", name, "Name (queue/exchange/etc.) for show/delete/declare");

    bsl::vector<bsl::string> kvParams;
    app.add_option("--param", kvParams, "Extra param key=value pairs")->take_all();

    bsl::string payload;
    app.add_option("--payload", payload, "Payload for publish/get (raw)");

    bool useAmqpPublish = false;
    bool useAmqpGet     = false;
    app.add_flag("--amqp-publish", useAmqpPublish, "Use AMQP backend for publish");
    app.add_flag("--amqp-get", useAmqpGet, "Use AMQP backend for get");

    bsl::string verbStr;
    bsl::string resource;
    app.add_option("verb", verbStr, "Command verb (list/show/declare/delete/publish/get)")->required();
    app.add_option("resource", resource, "Resource (queues/exchanges/bindings/... )")->required();

    app.parse(argc, argv);

    out.command.verb = parseVerb(verbStr);
    out.command.resource = resource;
    if (!name.empty()) {
        out.command.params["name"] = name;
    }
    for (const auto& kv : kvParams) {
        auto pos = kv.find('=');
        if (pos != bsl::string::npos) {
            out.command.params[kv.substr(0, pos)] = kv.substr(pos + 1);
        }
    }
    if ((out.command.verb == Verb::Publish && useAmqpPublish) ||
        (out.command.verb == Verb::Get && useAmqpGet)) {
        out.command.backend = BackendHint::AmqpPreferred;
    }
    if ((out.command.verb == Verb::Publish || out.command.verb == Verb::Get) &&
        !payload.empty()) {
        out.command.body = payload;
    }
    return out;
}

}  // namespace rmqadmin
