#include "cli.h"
#include "exec.h"
#include "render.h"

#include <bsl_iostream.h>

int main(int argc, char** argv)
{
    using namespace rmqadmin;

    CliParseResult parsed;
    try {
        parsed = parseCli(argc, argv);
    }
    catch (const std::exception& ex) {
        bsl::cerr << "Failed to parse CLI: " << ex.what() << bsl::endl;
        return 1;
    }

    Executor exec(parsed.config);
    Response resp = exec.run(parsed.command);
    Rendered rendered = renderResponse(parsed.command, resp);
    return rendered.exitCode;
}
