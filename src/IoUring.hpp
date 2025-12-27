#pragma once

#include <liburing.h>
#include <liburing/io_uring.h>

#include <coroutine>
#include <cstddef>
#include <vector>

#include "Task.hpp"
#include "TaskResult.hpp"

struct IoUring {
   private:
    io_uring ring = {};
    int cqe_count = 0;

   public:
    IoUring(int entries = 128, uint32_t flags = 0) {
        io_uring_params p = {};
        p.flags = flags;

        int ret = io_uring_queue_init_params(entries, &ring, &p);
        if (ret < 0) {
            print_error("Error io_uring_queue_init_params " << ret);
            raise(SIGTERM);
        }
    }
    ~IoUring() noexcept { io_uring_queue_exit(&ring); }

    IoUring(const IoUring&) = delete;
    IoUring& operator=(const IoUring&) = delete;

    io_uring_sqe* io_uring_get_sqe_safe() noexcept {
        auto* sqe = io_uring_get_sqe(&ring);
        if (sqe) {
            return sqe;
        } else {
            debug_info(": SQ is full, flushing " << cqe_count << " cqe(s)");
            io_uring_cq_advance(&ring, cqe_count);
            cqe_count = 0;
            io_uring_submit(&ring);
            sqe = io_uring_get_sqe(&ring);
            if (sqe) {
                return sqe;
            } else {
                print_error("io_uring_get_sqe return NULL after a submit, this should not happend");
                raise(SIGTERM);
                return nullptr;
            }
        }
    }

    void progress_one() {
        cqe_count = 0;
        int ret = io_uring_submit_and_wait(&ring, 1);
        if (ret > 0) {
            debug_info("io_uring_submit " << ret);
        }

        const int count = 64;
        io_uring_cqe* cqes[count];
        int completion = io_uring_peek_batch_cqe(&ring, cqes, count);
        if (completion) {
            debug_info("peek " << completion << " io_uring requests");
            for (int i = 0; i < completion; i++) {
                ++cqe_count;
                auto resumer_ptr = io_uring_cqe_get_data(cqes[i]);
                debug_info("completed a io_uring coroutine " << resumer_ptr);
                if (resumer_ptr) {
                    auto& resum = *reinterpret_cast<TaskResult<int>*>(resumer_ptr);
                    resum.resume(cqes[i]->res);
                }
            }
        }

        if (cqe_count > 0) {
            debug_info("Procesed " << cqe_count << " cqe(s)");
            io_uring_cq_advance(&ring, cqe_count);
            cqe_count = 0;
        }
    }
};

struct IoUringAwaitable {
    io_uring_sqe* m_sqe;
    TaskResult<int> m_result;

    IoUringAwaitable(IoUring& io_uring) : m_sqe(io_uring.io_uring_get_sqe_safe()) {
        debug_info("io_uring_get_sqe_safe " << m_sqe);
    }

    bool await_ready() const noexcept {
        debug_info("io_uringAwaitable ready");
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        m_result.m_handle = handle;
        io_uring_sqe_set_data(m_sqe, &m_result);
        debug_info("io_uringAwaitable stored and suspended " << &m_result);
    }

    auto await_resume() noexcept {
        debug_info("io_uringAwaitable resume with result " << m_result.m_result);
        return m_result.m_result;
    }
};

struct AcceptAwaitable : public IoUringAwaitable {
    AcceptAwaitable(IoUring& io_uring, int fd, struct sockaddr* addr, socklen_t* addrlen, int flags)
        : IoUringAwaitable(io_uring) {
        debug_info("AcceptAwaitable (" << fd << ", " << addr << ", " << addrlen << ", " << flags << ")");
        io_uring_prep_accept(m_sqe, fd, addr, addrlen, flags);
    }
};

struct ReadAwaitable : public IoUringAwaitable {
    ReadAwaitable(IoUring& io_uring, int fd, void* buf, size_t size, off_t offset) : IoUringAwaitable(io_uring) {
        debug_info("ReadAwaitable (" << fd << ", " << buf << ", " << size << ", " << offset << ")");
        io_uring_prep_read(m_sqe, fd, buf, size, offset);
    }
};

inline Task<int> ReadAllAwaitable(IoUring& io_uring, int fd, void* buf, uint64_t len, int64_t offset) {
    int64_t ret = 0;
    int r;
    int l = len;
    int64_t off = offset;
    debug_info(">> Begin pread(" << fd << ", " << len << ", " << offset << ")");
    char* buffer = static_cast<char*>(buf);

    do {
        r = co_await ReadAwaitable(io_uring, fd, buffer, l, off);
        if (r <= 0) { /* fail */
            if (ret == 0) ret = r;
            break;
        }

        l = l - r;
        buffer = buffer + r;
        off = off + r;
        ret = ret + r;

    } while ((l > 0) && (r >= 0));

    debug_info(">> End  pread(" << fd << ", " << len << ", " << offset << ")= " << ret);
    co_return ret;
}

struct WriteAwaitable : public IoUringAwaitable {
    WriteAwaitable(IoUring& io_uring, int fd, const void* buf, size_t size, off_t offset) : IoUringAwaitable(io_uring) {
        debug_info("WriteAwaitable (" << fd << ", " << buf << ", " << size << ", " << offset << ")");
        io_uring_prep_write(m_sqe, fd, buf, size, offset);
    }
};

inline Task<int> WriteAllAwaitable(IoUring& io_uring, int fd, const void* buf, size_t size, off_t offset) {
    int64_t ret = 0;
    int r;
    int l = size;
    int64_t off = offset;
    const char* buffer = static_cast<const char*>(buf);
    debug_info(">> Begin pwrite(" << fd << ", " << len << ", " << offset << ")");

    do {
        r = co_await WriteAwaitable(io_uring, fd, buffer, l, off);
        if (r <= 0) { /* fail */
            if (ret == 0) ret = r;
            break;
        }

        l = l - r;
        buffer = buffer + r;
        off = off + r;
        ret = ret + r;

    } while ((l > 0) && (r >= 0));

    debug_info(">> End pwrite(" << fd << ", " << len << ", " << offset << ") = " << ret);
    co_return ret;
}

struct WritevAwaitable : public IoUringAwaitable {
    WritevAwaitable(IoUring& io_uring, int fd, const struct iovec* iovecs, unsigned nr_vecs, off_t offset)
        : IoUringAwaitable(io_uring) {
        debug_info("WritevAwaitable (" << fd << ", vecs: " << nr_vecs << ", offset: " << offset << ")");
        io_uring_prep_writev(m_sqe, fd, iovecs, nr_vecs, offset);
    }
};

inline Task<int> WritevAllAwaitable(IoUring& io_uring, int fd, const struct iovec* iovecs, int nr_vecs, off_t offset) {
    int64_t total_written = 0;
    int r;
    off_t current_off = offset;

    // We create a local copy of iovecs because we might need to modify
    // iov_base and iov_len if a partial write occurs.
    std::vector<struct iovec> v(iovecs, iovecs + nr_vecs);
    size_t current_vec_idx = 0;

    do {
        // We pass the pointer to the current starting vector and the remaining count
        r = co_await WritevAwaitable(io_uring, fd, &v[current_vec_idx], v.size() - current_vec_idx, current_off);

        if (r <= 0) {
            if (total_written == 0) total_written = r;
            break;
        }

        total_written += r;
        current_off += r;

        // Logic to "advance" the iovecs based on bytes actually written (r)
        size_t processed_bytes = r;
        while (current_vec_idx < v.size() && processed_bytes > 0) {
            if (processed_bytes >= v[current_vec_idx].iov_len) {
                // This specific buffer was fully written
                processed_bytes -= v[current_vec_idx].iov_len;
                current_vec_idx++;
            } else {
                // Partial write in this specific buffer: adjust it for the next loop
                v[current_vec_idx].iov_base = static_cast<char*>(v[current_vec_idx].iov_base) + processed_bytes;
                v[current_vec_idx].iov_len -= processed_bytes;
                processed_bytes = 0;
            }
        }

    } while (current_vec_idx < v.size() && r >= 0);

    co_return total_written;
}

struct FsyncAwaitable : public IoUringAwaitable {
    FsyncAwaitable(IoUring& io_uring, int fd) : IoUringAwaitable(io_uring) {
        debug_info("FsyncAwaitable (" << fd << ")");
        io_uring_prep_fsync(m_sqe, fd, 0);
    }
};

struct CloseAwaitable : public IoUringAwaitable {
    CloseAwaitable(IoUring& io_uring, int fd) : IoUringAwaitable(io_uring) {
        debug_info("CloseAwaitable (" << fd << ")");
        io_uring_prep_close(m_sqe, fd);
    }
};