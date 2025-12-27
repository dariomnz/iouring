#pragma once

#include <coroutine>
#include <utility>

template <typename T>
struct Task {
    // The return type of a coroutine must contain a nested struct or type alias called `promise_type`
    struct promise_type {
        // Keep a coroutine handle referring to the parent coroutine if any. That is, if we
        // co_await a coroutine within another coroutine, this handle will be used to continue
        // working from where we left off.
        std::coroutine_handle<> precursor;

        // Place to hold the results produced by the coroutine
        T data;

        // Invoked when we first enter a coroutine. We initialize the precursor handle
        // with a resume point from where the task is ultimately suspended
        Task get_return_object() noexcept { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }

        // When the caller enters the coroutine, we have the option to suspend immediately.
        // Let's choose not to do that here
        std::suspend_never initial_suspend() const noexcept { return {}; }

        // If an exception was thrown in the coroutine body, we would handle it here
        void unhandled_exception() {}

        // The coroutine is about to complete (via co_return or reaching the end of the coroutine body).
        // The awaiter returned here defines what happens next
        auto final_suspend() const noexcept {
            struct awaiter {
                // Return false here to return control to the thread's event loop. Remember that we're
                // running on some async thread at this point.
                bool await_ready() const noexcept { return false; }

                void await_resume() const noexcept {}

                // Returning a coroutine handle here resumes the coroutine it refers to (needed for
                // continuation handling). If we wanted, we could instead enqueue that coroutine handle
                // instead of immediately resuming it by enqueuing it and returning void.
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto precursor = h.promise().precursor;
                    if (precursor) {
                        return precursor;
                    }
                    return std::noop_coroutine();
                }
            };
            return awaiter{};
        }
        // When the coroutine co_returns a value, this method is used to publish the result
        void return_value(T value) noexcept { data = std::move(value); }
    };

    // The following methods make our task type conform to the awaitable concept, so we can
    // co_await for a task to complete

    bool await_ready() const noexcept {
        // No need to suspend if this task has no outstanding work
        return handle.done();
    }

    T await_resume() const noexcept {
        // The returned value here is what `co_await our_task` evaluates to
        return std::move(handle.promise().data);
    }

    void await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        // The coroutine itself is being suspended (async work can beget other async work)
        // Record the argument as the continuation point when this is resumed later. See
        // the final_suspend awaiter on the promise_type above for where this gets used
        handle.promise().precursor = coroutine;
    }

    bool done() const { return handle.done(); }

    // This handle is assigned to when the coroutine itself is suspended (see await_suspend above)
    std::coroutine_handle<promise_type> handle;
};
