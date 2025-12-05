#ifndef RMQADMIN_CORE_H
#define RMQADMIN_CORE_H

#include <bsl_string.h>
#include <bsl_vector.h>
#include <bsl_map.h>
#include <bsl_optional.h>

namespace rmqadmin {

struct AdminConfig {
    bsl::string baseUrl;   // e.g. http://localhost:15672
    bsl::string username;
    bsl::string password;
    bsl::string vhost;     // default "/"
};

enum class Verb { List, Show, Declare, Delete, Publish, Get, Unknown };

struct Command {
    Verb verb;
    bsl::string resource;              // queues, exchanges, bindings, etc.
    bsl::map<bsl::string, bsl::string> params;
};

struct Response {
    int statusCode{0};
    bsl::string body;
    bsl::string contentType;
    bsl::string error;

    explicit operator bool() const { return statusCode >= 200 && statusCode < 300 && error.empty(); }
};

}  // namespace rmqadmin

#endif  // RMQADMIN_CORE_H
