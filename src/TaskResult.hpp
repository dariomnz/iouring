#pragma once

#include "debug.hpp"
#include <coroutine>

template<typename T>
struct TaskResult {
    void resume(T result) noexcept {
        debug_info("Stored result in co_result " << result);
        m_result = result;
        m_handle.resume();
    }

    std::coroutine_handle<> m_handle;
    T m_result;
};
