#ifndef TEST_TOOLS_BUFFER_H
#define TEST_TOOLS_BUFFER_H

#include "string.h"

namespace test {

class Buffer {
public:
    static const size_t DEFAULT_PADDING = 16;

    explicit Buffer(size_t size = 0, size_t padding = DEFAULT_PADDING);

    char* data();
    const char* data() const;
    size_t size() const;

    std::string toString() const;

    bool isPaddingValid() const;

    Buffer& checkEquals(const std::string &str);
    Buffer& checkStartsWith(const std::string &str);
    Buffer& checkEndsWith(const std::string &str);
    Buffer& checkPadding();

    explicit operator char*();
    explicit operator const char*() const;

private:
    std::string d_, p_;
};

} // namespace test

inline char* test::Buffer::data() {
    return &d_.front() + p_.size();
}

inline const char* test::Buffer::data() const {
    return &d_.front() + p_.size();
}

inline size_t test::Buffer::size() const {
    return d_.size() - p_.size() * 2;
}

inline std::string test::Buffer::toString() const {
    return d_.substr(p_.size(), size());
}

inline test::Buffer& test::Buffer::checkEquals(const std::string &str) {
    test::checkEquals(toString(), str);
    return *this;
}

inline test::Buffer& test::Buffer::checkStartsWith(const std::string &str) {
    test::checkStartsWith(toString(), str);
    return *this;
}

inline test::Buffer& test::Buffer::checkEndsWith(const std::string &str) {
    test::checkEndsWith(toString(), str);
    return *this;
}

inline test::Buffer::operator char*() {
    return data();
}

inline test::Buffer::operator const char*() const {
    return data();
}

#endif // TEST_TOOLS_BUFFER_H
