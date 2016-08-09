#ifndef TEST_TOOLS_STRING_H
#define TEST_TOOLS_STRING_H

#include <string>

namespace test {

std::string toHex(const std::string &str);
std::string fromHex(const std::string &str);

bool isPrintable(const std::string &str);

void checkEquals(const std::string &str1, const std::string &str2);
void checkStartsWith(const std::string &str, const std::string &start);
void checkEndsWith(const std::string &str, const std::string &end);
void checkEmpty(const std::string &str);

} // namespace test

#endif // TEST_TOOLS_STRING_H
