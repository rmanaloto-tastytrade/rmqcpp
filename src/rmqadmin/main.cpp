#include "cli.h"
#include "exec.h"
#include "render.h"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/sinks/ConsoleSink.h>

int main(int argc, char** argv)
{
    using namespace rmqadmin;

    quill::BackendOptions backendOptions;
    quill::Backend::start(backendOptions);
    auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
    quill::Logger* logger = quill::Frontend::create_or_get_logger("rmqadmin", std::move(sink));

    CliParseResult parsed;
    try {
        parsed = parseCli(argc, argv);
    }
    catch (const std::exception& ex) {
        QUILL_LOG_ERROR(logger, "Failed to parse CLI: {}", ex.what());
        return 1;
    }
    if (parsed.helpRequested) {
        return parsed.exitCode;
    }

    Executor exec(parsed.config, logger);
    Response resp = exec.run(parsed.command);
    Rendered rendered = renderResponse(parsed.command, resp, logger);
    return rendered.exitCode;
}
