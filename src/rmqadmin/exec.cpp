#include "exec.h"

#include "http.h"

namespace rmqadmin {

Executor::Executor(const AdminConfig& cfg)
: d_cfg(cfg)
{
}

Response Executor::run(const Command& cmd)
{
    HttpClient client(d_cfg);
    return client.perform(cmd);
}

}  // namespace rmqadmin
