#include "CurrentThread.h"

#include <sys/syscall.h>

namespace CurrentThread {
    thread_local int t_cachedTid = 0;  // 定义并初始化 TLS 变量

    void cacheTid() {  // 函数实现
        if (t_cachedTid == 0) {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}  // namespace CurrentThread
