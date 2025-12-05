#ifndef RMQADMIN_EXEC_H
#define RMQADMIN_EXEC_H

#include "core.h"

namespace rmqadmin {

class Executor {
  public:
    Executor(const AdminConfig& cfg, void* logger);
    Response run(const Command& cmd);

  private:
    AdminConfig d_cfg;
    void* d_logger;  // quill::Logger*
};

}  // namespace rmqadmin

#endif  // RMQADMIN_EXEC_H
