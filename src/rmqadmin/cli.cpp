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

    bsl::string verbStr;
    bsl::string resource;
    app.add_option("verb", verbStr, "Command verb (list/show/declare/delete/publish/get)")->required();
    app.add_option("resource", resource, "Resource (queues/exchanges/bindings/... )")->required();

    CLI11_PARSE(app, argc, argv);

    out.command.verb = parseVerb(verbStr);
    out.command.resource = resource;
    return out;
}

}  // namespace rmqadmin
