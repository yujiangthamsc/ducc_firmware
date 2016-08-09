#include "string.h"

#include "catch.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/hex.hpp>

std::string test::toHex(const std::string &str) {
    std::string s = boost::algorithm::hex(str);
    boost::algorithm::to_lower(s);
    return s;
}

std::string test::fromHex(const std::string &str) {
    try {
        return boost::algorithm::unhex(str);
    } catch (const boost::algorithm::hex_decode_error&) {
        return std::string();
    }
}

bool test::isPrintable(const std::string &str) {
    return boost::algorithm::all(str, boost::algorithm::is_print(std::locale::classic()));
}

void test::checkEquals(const std::string &str1, const std::string &str2) {
    if (isPrintable(str1) && isPrintable(str2)) {
        CHECK(str1 == str2);
    } else {
        CHECK(toHex(str1) == toHex(str2)); // Shows better diagnostic output
    }
}

void test::checkStartsWith(const std::string &str, const std::string &start) {
    const std::string s = (str.size() > start.size()) ? str.substr(0, start.size()) : str;
    checkEquals(s, start);
}

void test::checkEndsWith(const std::string &str, const std::string &end) {
    const std::string s = (str.size() > end.size()) ? str.substr(str.size() - end.size(), end.size()) : str;
    checkEquals(s, end);
}

void test::checkEmpty(const std::string &str) {
    checkEquals(str, "");
}
