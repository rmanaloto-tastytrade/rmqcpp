#include "render.h"

#include <bsl_iostream.h>

namespace rmqadmin {

Rendered renderResponse(const Command& cmd, const Response& response)
{
    Rendered r;
    if (response) {
        bsl::cout << response.body << bsl::endl;
    }
    else {
        r.exitCode = 1;
        bsl::cerr << "Request failed for '" << cmd.resource << "': "
                  << response.statusCode << " " << response.error << bsl::endl;
        if (!response.body.empty()) {
            bsl::cerr << response.body << bsl::endl;
        }
    }
    return r;
}

}  // namespace rmqadmin
