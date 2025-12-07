#include "render.h"

#include <quill/LogMacros.h>

#include <bsl_iostream.h>

namespace rmqadmin {

Rendered renderResponse(const Command& cmd,
                        const Response& response,
                        quill::Logger* logger)
{
    Rendered r;
    if (response) {
        if (logger) {
            QUILL_LOG_INFO(logger,
                           "Command {} {} succeeded status={} size={}",
                           static_cast<int>(cmd.verb),
                           std::string(cmd.resource.data(), cmd.resource.size()),
                           response.statusCode,
                           response.body.size());
        }
        // Maintain stdout output for tooling that expects response bodies.
        if (!response.body.empty()) {
            bsl::cout << response.body << bsl::endl;
        }
    }
    else {
        r.exitCode = 1;
        if (logger) {
            QUILL_LOG_ERROR(logger,
                            "Request failed for {} status={} error={} body-bytes={}",
                            std::string(cmd.resource.data(), cmd.resource.size()),
                            response.statusCode,
                            std::string(response.error.data(), response.error.size()),
                            response.body.size());
        }
        if (!response.body.empty()) {
            bsl::cout << response.body << bsl::endl;
        }
    }
    return r;
}

}  // namespace rmqadmin
