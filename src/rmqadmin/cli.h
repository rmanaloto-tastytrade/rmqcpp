#ifndef RMQADMIN_CLI_H
#define RMQADMIN_CLI_H

#include "core.h"

namespace rmqadmin {

struct CliParseResult {
    AdminConfig config;
    Command command;
    bool helpRequested{false};
    int exitCode{0};
};

CliParseResult parseCli(int argc, char** argv);

}  // namespace rmqadmin

#endif  // RMQADMIN_CLI_H
