#ifndef TEST_TOOLS_STREAM_H
#define TEST_TOOLS_STREAM_H

#include "spark_wiring_print.h"

#include "string.h"

namespace test {

class StringOutputStream: public Print {
public:
    StringOutputStream() = default;

    const StringOutputStream& checkEquals(const std::string &str) const;
    const StringOutputStream& checkStartsWith(const std::string &str) const;
    const StringOutputStream& checkEndsWith(const std::string &str) const;
    const StringOutputStream& checkEmpty() const;

    const std::string& toString() const;

    virtual size_t write(const uint8_t *data, size_t size) override;
    virtual size_t write(uint8_t byte) override;

private:
    std::string s_;
};

} // namespace test

// test::StringOutputStream
inline const test::StringOutputStream& test::StringOutputStream::checkEquals(const std::string &str) const {
    test::checkEquals(s_, str);
    return *this;
}

inline const test::StringOutputStream& test::StringOutputStream::checkStartsWith(const std::string &str) const {
    test::checkStartsWith(s_, str);
    return *this;
}

inline const test::StringOutputStream& test::StringOutputStream::checkEndsWith(const std::string &str) const {
    test::checkEndsWith(s_, str);
    return *this;
}

inline const test::StringOutputStream& test::StringOutputStream::checkEmpty() const {
    test::checkEmpty(s_);
    return *this;
}

inline const std::string& test::StringOutputStream::toString() const {
    return s_;
}

inline size_t test::StringOutputStream::write(const uint8_t *data, size_t size) {
    s_.append((const char*)data, size);
    return size;
}

inline size_t test::StringOutputStream::write(uint8_t byte) {
    return write(&byte, 1);
}

#endif // TEST_TOOLS_STREAM_H
