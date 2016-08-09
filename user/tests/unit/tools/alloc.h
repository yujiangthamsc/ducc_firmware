#ifndef TEST_TOOLS_ALLOC_H
#define TEST_TOOLS_ALLOC_H

#include "buffer.h"

#include <unordered_map>
#include <mutex>

namespace test {

class Allocator {
public:
    explicit Allocator(size_t padding = Buffer::DEFAULT_PADDING);

    void* malloc(size_t size);
    void* calloc(size_t count, size_t size);
    void* realloc(void* ptr, size_t size);
    void free(void* ptr);

    void reset();

    void check();

    // This class is non-copyable
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

private:
    struct FreedBuffer {
        Buffer buffer;
        std::string data;
    };

    std::unordered_map<void*, Buffer> alloc_;
    std::unordered_map<void*, FreedBuffer> free_;
    size_t padding_;
    bool failed_;

    std::recursive_mutex mutex_;
};

} // namespace test

inline test::Allocator::Allocator(size_t padding) :
        padding_(padding),
        failed_(false) {
    reset();
}

#endif // TEST_TOOLS_ALLOC_H
