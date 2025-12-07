#ifndef RMQADMIN_RENDER_H
#define RMQADMIN_RENDER_H

#include "core.h"

#include <quill/Logger.h>

namespace rmqadmin {

struct Rendered {
    int exitCode{0};
};

Rendered renderResponse(const Command& cmd,
                        const Response& response,
                        quill::Logger* logger);

}  // namespace rmqadmin

#endif  // RMQADMIN_RENDER_H
