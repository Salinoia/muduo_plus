// CurrentThread.h
#pragma once

#include <unistd.h>

namespace CurrentThread {
    // 线程局部存储（TLS）缓存线程ID，避免频繁系统调用
    extern thread_local int t_cachedTid;  // TLS变量，每个线程独享一份副本

    // 缓存当前线程ID（内部调用系统调用）
    void cacheTid();

    // 获取当前线程ID（内联函数减少调用开销）
    inline int tid() {
        if (__builtin_expect(t_cachedTid == 0, 0)) {  // 分支预测优化
            cacheTid();
        }
        return t_cachedTid;
    }
}  // namespace CurrentThread
