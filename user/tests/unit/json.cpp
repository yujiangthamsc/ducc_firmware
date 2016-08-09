#include "catch.hpp"

#include "spark_wiring_json.h"
#include "spark_wiring_print.h"

#include "tools/stream.h"
#include "tools/buffer.h"

#include <string>
#include <cstdlib>

namespace {

using namespace spark;

template<typename T>
T fromString(const std::string &s, bool *ok = nullptr) {
    char *end = nullptr;
    const T val = strtod(s.c_str(), &end);
    if (!end || *end != '\0') {
        return T();
    }
    if (ok) {
        *ok = true;
    }
    return val;
}

template<>
bool fromString<bool>(const std::string &s, bool *ok) {
    if (ok) {
        *ok = true;
    }
    if (s.empty() || s == "false") { // Empty string or "false" in lower case
        return false;
    }
    bool isNum = false;
    const double val = fromString<double>(s, &isNum); // Check if string can be converted to a number
    if (isNum) {
        return val;
    }
    return true; // Any other non-empty string
}

template<typename T>
inline T fromString(const JSONString &s, bool *ok = nullptr) {
    return fromString<T>(std::string(s.data(), s.size()), ok);
}

void checkString(const JSONString &s, const std::string &val) {
    const size_t n = s.size();
    REQUIRE(n == val.size());
    CHECK(s.isEmpty() == !n);
    const char *d = s.data();
    REQUIRE(d != nullptr);
    CHECK(d[n] == '\0'); // JSONString::data() returns null-terminated string
    CHECK(std::string(d, n) == val);
}

void checkString(const JSONValue &v, const std::string &val) {
    REQUIRE(v.type() == JSON_TYPE_STRING);
    CHECK(v.isString() == true);
    CHECK(v.isValid() == true);
    const JSONString s = v.toString();
    CHECK(v.toBool() == fromString<bool>(s));
    CHECK(v.toInt() == fromString<int>(s));
    CHECK(v.toDouble() == fromString<double>(s));
    checkString(s, val);
}

inline void checkString(const JSONValue &v, const char *val) { // Helper function for checkNext()
    checkString(v, std::string(val));
}

template<typename T>
void checkNumber(const JSONValue &v, T val) {
    REQUIRE(v.type() == JSON_TYPE_NUMBER);
    CHECK(v.isNumber());
    CHECK(v.isValid());
    CHECK((T)v.toBool() == (bool)val);
    CHECK((T)v.toInt() == (int)val);
    CHECK((T)v.toDouble() == (double)val);
    CHECK(fromString<T>(v.toString()) == val);
}

void checkBool(const JSONValue &v, bool val) {
    REQUIRE(v.type() == JSON_TYPE_BOOL);
    CHECK(v.isBool() == true);
    CHECK(v.isValid() == true);
    CHECK(v.toBool() == val);
    CHECK((bool)v.toInt() == val);
    CHECK((bool)v.toDouble() == val);
    checkString(v.toString(), val ? "true" : "false");
}

void checkNull(const JSONValue &v) {
    REQUIRE(v.type() == JSON_TYPE_NULL);
    CHECK(v.isNull() == true);
    CHECK(v.isValid() == true);
    CHECK(v.toBool() == false);
    CHECK(v.toInt() == 0);
    CHECK(v.toDouble() == 0.0);
    checkString(v.toString(), "");
}

void checkInvalid(const JSONValue &v) {
    REQUIRE(v.type() == JSON_TYPE_INVALID);
    CHECK(v.isValid() == false);
    CHECK(v.toBool() == false);
    CHECK(v.toInt() == 0);
    CHECK(v.toDouble() == 0.0);
    checkString(v.toString(), "");
}

JSONValue next(JSONArrayIterator &it) {
    const size_t n = it.count();
    REQUIRE(it.next() == true);
    CHECK(it.count() == n - 1);
    return it.value();
}

JSONValue next(JSONObjectIterator &it, JSONString *name) {
    const size_t n = it.count();
    REQUIRE(it.next() == true);
    CHECK(it.count() == n - 1);
    *name = it.name();
    return it.value();
}

template<typename T>
inline void checkNext(JSONArrayIterator &it, void(*checkValue)(const JSONValue&, T), T val) {
    checkValue(next(it), val);
}

inline void checkNext(JSONArrayIterator &it, void(*checkValue)(const JSONValue&)) {
    checkValue(next(it));
}

template<typename T>
inline void checkNext(JSONObjectIterator &it, void(*checkValue)(const JSONValue&, T), const std::string &name, T val) {
    JSONString s;
    const JSONValue v = next(it, &s);
    checkString(s, name);
    checkValue(v, val);
}

inline void checkNext(JSONObjectIterator &it, void(*checkValue)(const JSONValue&), const std::string &name) {
    JSONString s;
    const JSONValue v = next(it, &s);
    checkString(s, name);
    checkValue(v);
}

void checkAtEnd(JSONArrayIterator &it) {
    REQUIRE(it.count() == 0);
    CHECK(it.next() == false);
}

void checkAtEnd(JSONObjectIterator &it) {
    REQUIRE(it.count() == 0);
    CHECK(it.next() == false);
}

inline JSONValue parse(const char *json) {
    return JSONValue::parseCopy(json);
}

} // namespace

namespace spark {

inline std::ostream& operator<<(std::ostream &strm, const JSONString &str) {
    return strm << '"' << str.data() << '"';
}

} // namespace spark

TEST_CASE("JSONValue") {
    SECTION("construction") {
        checkInvalid(JSONValue());
    }

    SECTION("null") {
        checkNull(parse("null"));
    }

    SECTION("bool") {
        checkBool(parse("true"), true);
        checkBool(parse("false"), false);
    }

    SECTION("number") {
        SECTION("int") {
            checkNumber(parse("0"), 0);
            checkNumber(parse("1"), 1);
            checkNumber(parse("-1"), -1);
            checkNumber(parse("12345"), 12345);
            checkNumber(parse("-12345"), -12345);
            checkNumber(parse("-2147483648"), -2147483648); // INT_MIN
            checkNumber(parse("2147483647"), 2147483647); // INT_MAX
        }
        SECTION("double") {
            checkNumber(parse("0.0"), 0.0);
            checkNumber(parse("1.0"), 1.0);
            checkNumber(parse("-1.0"), -1.0);
            checkNumber(parse("0.5"), 0.5);
            checkNumber(parse("-0.5"), -0.5);
            checkNumber(parse("3.1416"), 3.1416);
            checkNumber(parse("-3.1416"), -3.1416);
            checkNumber(parse("2.22507e-308"), 2.22507e-308); // ~DBL_MIN
            checkNumber(parse("1.79769e+308"), 1.79769e+308); // ~DBL_MAX
        }
    }

    SECTION("string") {
        checkString(parse("\"\""), ""); // Empty string
        checkString(parse("\"abc\""), "abc");
        checkString(parse("\"a\""), "a"); // Single character
        checkString(parse("\"\\\"\""), "\""); // Single escaped character
        checkString(parse("\"\\\"\\/\\\\\\b\\f\\n\\r\\t\""), "\"/\\\b\f\n\r\t"); // Named escaped characters
        checkString(parse("\"\\u0000\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\u0008\\u0009\\u000a\\u000b"
                "\\u000c\\u000d\\u000e\\u000f\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017\\u0018\\u0019"
                "\\u001a\\u001b\\u001c\\u001d\\u001e\\u001f\""),
                std::string("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15"
                        "\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f", 32)); // Control characters
        checkString(parse("\"\\u2014\""), "\\u2014"); // Unicode characters are not processed
    }

    SECTION("in-place processing vs copying") {
        SECTION("in-place processing") {
            char json[] = "\"abc\"";
            const JSONValue v = JSONValue::parse(json, sizeof(json));
            json[2] = 'B';
            CHECK(v.toString() == "aBc");
        }
        SECTION("copying") {
            char json[] = "\"abc\"";
            const JSONValue v = JSONValue::parseCopy(json, sizeof(json));
            json[2] = 'B';
            CHECK(v.toString() == "abc"); // Source data is not affected
        }
        SECTION("implicit copying") {
            char json[] = "1x";
            const JSONValue v = JSONValue::parse(json, 1); // No space for term. null
            CHECK(v.toInt() == 1);
            CHECK(v.toString() == "1");
            const char *s = v.toString().data();
            CHECK(s[1] == '\0'); // JSONString::data() returns null-terminated string
            CHECK(json[1] == 'x');
        }
    }

    SECTION("parsing errors") {
        checkInvalid(parse("")); // Empty source data
        checkInvalid(parse("[")); // Malformed array
        checkInvalid(parse("]"));
        checkInvalid(parse("[1,"));
        checkInvalid(parse("{")); // Malformed object
        checkInvalid(parse("}"));
        checkInvalid(parse("{null"));
        checkInvalid(parse("{false"));
        checkInvalid(parse("{1"));
        checkInvalid(parse("{\"1\""));
        checkInvalid(parse("{\"1\":"));
        checkInvalid(parse("\"\\x\"")); // Unknown escaped character
        checkInvalid(parse("\"\\U0001\"")); // Uppercase 'U'
        checkInvalid(parse("\"\\u000x\"")); // Invalid hex value
        checkInvalid(parse("\"\\u001\""));
        checkInvalid(parse("\"\\u01\""));
        checkInvalid(parse("\"\\u\""));
    }
}

TEST_CASE("JSONString") {
    // Most of the functionality is already checked in JSONValue test cases via checkString() function
    // and its overloads. Here we mostly check remaining methods of the class
    SECTION("construction") {
        checkString(JSONString(), "");
        checkString(JSONString(JSONValue()), ""); // Constructing from invalid JSONValue
    }

    SECTION("comparison") {
        const JSONString s1(parse("\"\""));
        const JSONString s2(parse("\"abc\""));
        CHECK(s1 == JSONString()); // JSONString
        CHECK(s1 == s1);
        CHECK(s2 == s2);
        CHECK(s1 != s2);
        CHECK((s1 == "" && "" == s1)); // const char*
        CHECK((s1 != "abc" && "abc" != s1));
        CHECK((s2 == "abc" && "abc" == s2));
        CHECK((s2 != "abcd" && "abcd" != s2));
        CHECK((s1 == String() && String() == s1)); // String
        CHECK((s1 != String("abc") && String("abc") != s1));
        CHECK((s2 == String("abc") && String("abc") == s2));
        CHECK((s2 != String("abcd") && String("abcd") != s2));
    }

    SECTION("casting") {
        const JSONString s1(parse("\"\""));
        const JSONString s2(parse("\"abc\""));
        CHECK(strcmp((const char*)s1, "") == 0); // const char*
        CHECK(strcmp((const char*)s2, "abc") == 0);
        CHECK((String)s1 == String()); // String
        CHECK((String)s2 == String("abc"));
    }
}

TEST_CASE("JSONArrayIterator") {
    SECTION("construction") {
        JSONArrayIterator it1;
        checkInvalid(it1.value());
        checkAtEnd(it1);
        const JSONValue v;
        JSONArrayIterator it2(v); // Constructing from invalid JSONValue
        checkInvalid(it2.value());
        checkAtEnd(it2);
    }

    SECTION("empty array") {
        JSONArrayIterator it(parse("[]"));
        checkInvalid(it.value());
        checkAtEnd(it);
    }

    SECTION("single element") {
        JSONArrayIterator it(parse("[null]"));
        checkNext(it, checkNull);
        checkAtEnd(it);
    }

    SECTION("primitive elements") {
        JSONArrayIterator it(parse("[null,true,2,3.14,\"abcd\"]"));
        checkNext(it, checkNull);
        checkNext(it, checkBool, true);
        checkNext(it, checkNumber, 2);
        checkNext(it, checkNumber, 3.14);
        checkNext(it, checkString, "abcd");
        checkAtEnd(it);
    }

    SECTION("nested array") {
        JSONArrayIterator it1(parse("[1.1,[2.1,2.2,2.3],1.3]"));
        checkNext(it1, checkNumber, 1.1);
        JSONArrayIterator it2(next(it1)); // Nested array
        checkNext(it2, checkNumber, 2.1);
        checkNext(it2, checkNumber, 2.2);
        checkNext(it2, checkNumber, 2.3);
        checkAtEnd(it2);
        checkNext(it1, checkNumber, 1.3);
        checkAtEnd(it1);
    }

    SECTION("nested object") {
        JSONArrayIterator it1(parse("[1.1,{\"2.1\":2.1,\"2.2\":2.2,\"2.3\":2.3},1.3]"));
        checkNext(it1, checkNumber, 1.1);
        JSONObjectIterator it2(next(it1)); // Nested object
        checkNext(it2, checkNumber, "2.1", 2.1);
        checkNext(it2, checkNumber, "2.2", 2.2);
        checkNext(it2, checkNumber, "2.3", 2.3);
        checkAtEnd(it2);
        checkNext(it1, checkNumber, 1.3);
        checkAtEnd(it1);
    }

    SECTION("deeply nested array") {
        JSONArrayIterator it1(parse("[[[[[[[[[[[]]]]]]]]]]]"));
        for (int i = 0; i < 10; ++i) {
            JSONArrayIterator it2(next(it1));
            checkAtEnd(it1);
            it1 = it2;
        }
        checkAtEnd(it1); // Innermost array
    }
}

TEST_CASE("JSONObjectIterator") {
    SECTION("construction") {
        JSONObjectIterator it1;
        checkString(it1.name(), "");
        checkInvalid(it1.value());
        checkAtEnd(it1);
        const JSONValue v;
        JSONObjectIterator it2(v); // Constructing from invalid JSONValue
        checkString(it2.name(), "");
        checkInvalid(it2.value());
        checkAtEnd(it2);
    }

    SECTION("empty object") {
        JSONObjectIterator it(parse("{}"));
        checkString(it.name(), "");
        checkInvalid(it.value());
        checkAtEnd(it);
    }

    SECTION("single element") {
        JSONObjectIterator it(parse("{\"null\":null}"));
        checkNext(it, checkNull, "null");
        checkAtEnd(it);
    }

    SECTION("primitive elements") {
        JSONObjectIterator it(parse("{\"null\":null,\"bool\":true,\"int\":2,\"double\":3.14,\"string\":\"abcd\"}"));
        checkNext(it, checkNull, "null");
        checkNext(it, checkBool, "bool", true);
        checkNext(it, checkNumber, "int", 2);
        checkNext(it, checkNumber, "double", 3.14);
        checkNext(it, checkString, "string", "abcd");
        checkAtEnd(it);
    }

    SECTION("nested object") {
        JSONObjectIterator it1(parse("{\"1.1\":1.1,\"1.2\":{\"2.1\":2.1,\"2.2\":2.2,\"2.3\":2.3},\"1.3\":1.3}"));
        checkNext(it1, checkNumber, "1.1", 1.1);
        JSONString s;
        JSONObjectIterator it2(next(it1, &s)); // Nested object
        checkString(s, "1.2");
        checkNext(it2, checkNumber, "2.1", 2.1);
        checkNext(it2, checkNumber, "2.2", 2.2);
        checkNext(it2, checkNumber, "2.3", 2.3);
        checkAtEnd(it2);
        checkNext(it1, checkNumber, "1.3", 1.3);
        checkAtEnd(it1);
    }

    SECTION("nested array") {
        JSONObjectIterator it1(parse("{\"1.1\":1.1,\"1.2\":[2.1,2.2,2.3],\"1.3\":1.3}"));
        checkNext(it1, checkNumber, "1.1", 1.1);
        JSONString s;
        JSONArrayIterator it2(next(it1, &s)); // Nested array
        checkString(s, "1.2");
        checkNext(it2, checkNumber, 2.1);
        checkNext(it2, checkNumber, 2.2);
        checkNext(it2, checkNumber, 2.3);
        checkAtEnd(it2);
        checkNext(it1, checkNumber, "1.3", 1.3);
        checkAtEnd(it1);
    }

    SECTION("deeply nested object") {
        JSONObjectIterator it1(parse("{\"1\":{\"2\":{\"3\":{\"4\":{\"5\":{\"6\":{\"7\":{\"8\":{\"9\":{\"10\":{}}}}}}}}}}}"));
        for (int i = 1; i <= 10; ++i) {
            JSONString s;
            JSONObjectIterator it2(next(it1, &s));
            checkString(s, std::to_string(i));
            checkAtEnd(it1);
            it1 = it2;
        }
        checkAtEnd(it1); // Innermost object
    }
}

TEST_CASE("JSONStreamWriter") {
    test::StringOutputStream data;
    JSONStreamWriter json(data);

    SECTION("construction") {
        CHECK(json.stream() == &data);
        data.checkEmpty();
    }

    SECTION("null") {
        json.nullValue();
        data.checkEquals("null");
    }

    SECTION("bool") {
        SECTION("true") {
            json.value(true);
            data.checkEquals("true");
        }
        SECTION("false") {
            json.value(false);
            data.checkEquals("false");
        }
    }

    SECTION("number") {
        SECTION("int") {
            SECTION("0") {
                json.value(0);
                data.checkEquals("0");
            }
            SECTION("1") {
                json.value(1);
                data.checkEquals("1");
            }
            SECTION("-1") {
                json.value(-1);
                data.checkEquals("-1");
            }
            SECTION("12345") {
                json.value(12345);
                data.checkEquals("12345");
            }
            SECTION("-12345") {
                json.value(-12345);
                data.checkEquals("-12345");
            }
            SECTION("min") {
                json.value((int)-2147483648); // INT_MIN
                data.checkEquals("-2147483648");
            }
            SECTION("max") {
                json.value((int)2147483647); // INT_MAX
                data.checkEquals("2147483647");
            }
        }
        SECTION("double") {
            SECTION("0.0") {
                json.value(0.0);
                data.checkEquals("0");
            }
            SECTION("1.0") {
                json.value(1.0);
                data.checkEquals("1");
            }
            SECTION("-1.0") {
                json.value(-1.0);
                data.checkEquals("-1");
            }
            SECTION("0.5") {
                json.value(0.5);
                data.checkEquals("0.5");
            }
            SECTION("-0.5") {
                json.value(-0.5);
                data.checkEquals("-0.5");
            }
            SECTION("3.1416") {
                json.value(3.1416);
                data.checkEquals("3.1416");
            }
            SECTION("-3.1416") {
                json.value(-3.1416);
                data.checkEquals("-3.1416");
            }
            SECTION("min") {
                json.value(2.22507e-308); // ~DBL_MIN
                data.checkEquals("2.22507e-308");
            }
            SECTION("max") {
                json.value(1.79769e+308); // ~DBL_MAX
                data.checkEquals("1.79769e+308");
            }
        }
    }

    SECTION("string") {
        SECTION("empty") {
            json.value("");
            data.checkEquals("\"\"");
        }
        SECTION("test string") {
            json.value("abc");
            data.checkEquals("\"abc\"");
        }
        SECTION("single character") {
            json.value("a");
            data.checkEquals("\"a\"");
        }
        SECTION("single escaped character") {
            json.value("\"");
            data.checkEquals("\"\\\"\"");
        }
        SECTION("named escaped characters") {
            json.value("\"/\\\b\f\n\r\t");
            data.checkEquals("\"\\\"/\\\\\\b\\f\\n\\r\\t\""); // '/' is never escaped
        }
        SECTION("control characters") {
            json.value("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16"
                    "\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f", 32);
            data.checkEquals("\"\\u0000\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\b\\t\\n\\u000b\\f\\r\\u000e"
                    "\\u000f\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017\\u0018\\u0019\\u001a\\u001b\\u001c"
                    "\\u001d\\u001e\\u001f\"");
        }
    }

    SECTION("array") {
        SECTION("empty") {
            json.beginArray();
            json.endArray();
            data.checkEquals("[]");
        }
        SECTION("single element") {
            json.beginArray();
            json.nullValue();
            json.endArray();
            data.checkEquals("[null]");
        }
        SECTION("primitive elements") {
            json.beginArray();
            json.nullValue().value(true).value(2).value(3.14).value("abcd");
            json.endArray();
            data.checkEquals("[null,true,2,3.14,\"abcd\"]");
        }
        SECTION("nested array") {
            json.beginArray();
            json.value(1.1);
            json.beginArray();
            json.value(2.1).value(2.2).value(2.3);
            json.endArray();
            json.value(1.3);
            json.endArray();
            data.checkEquals("[1.1,[2.1,2.2,2.3],1.3]");
        }
        SECTION("nested object") {
            json.beginArray();
            json.value(1.1);
            json.beginObject();
            json.name("2.1").value(2.1);
            json.name("2.2").value(2.2);
            json.name("2.3").value(2.3);
            json.endObject();
            json.value(1.3);
            json.endArray();
            data.checkEquals("[1.1,{\"2.1\":2.1,\"2.2\":2.2,\"2.3\":2.3},1.3]");
        }
        SECTION("deeply nested array") {
            json.beginArray();
            for (int i = 0; i < 10; ++i) {
                json.beginArray();
            }
            for (int i = 0; i < 10; ++i) {
                json.endArray();
            }
            json.endArray();
            data.checkEquals("[[[[[[[[[[[]]]]]]]]]]]");
        }
    }

    SECTION("object") {
        SECTION("empty") {
            json.beginObject();
            json.endObject();
            data.checkEquals("{}");
        }
        SECTION("single element") {
            json.beginObject();
            json.name("null").nullValue();
            json.endObject();
            data.checkEquals("{\"null\":null}");
        }
        SECTION("primitive elements") {
            json.beginObject();
            json.name("null").nullValue();
            json.name("bool").value(true);
            json.name("int").value(2);
            json.name("double").value(3.14);
            json.name("string").value("abcd");
            json.endObject();
            data.checkEquals("{\"null\":null,\"bool\":true,\"int\":2,\"double\":3.14,\"string\":\"abcd\"}");
        }
        SECTION("nested object") {
            json.beginObject();
            json.name("1.1").value(1.1);
            json.name("1.2").beginObject();
            json.name("2.1").value(2.1);
            json.name("2.2").value(2.2);
            json.name("2.3").value(2.3);
            json.endObject();
            json.name("1.3").value(1.3);
            json.endObject();
            data.checkEquals("{\"1.1\":1.1,\"1.2\":{\"2.1\":2.1,\"2.2\":2.2,\"2.3\":2.3},\"1.3\":1.3}");
        }
        SECTION("nested array") {
            json.beginObject();
            json.name("1.1").value(1.1);
            json.name("1.2").beginArray();
            json.value(2.1).value(2.2).value(2.3);
            json.endArray();
            json.name("1.3").value(1.3);
            json.endObject();
            data.checkEquals("{\"1.1\":1.1,\"1.2\":[2.1,2.2,2.3],\"1.3\":1.3}");
        }
        SECTION("deeply nested object") {
            json.beginObject();
            for (int i = 1; i <= 10; ++i) {
                json.name(std::to_string(i).c_str()).beginObject();
            }
            for (int i = 1; i <= 10; ++i) {
                json.endObject();
            }
            json.endObject();
            data.checkEquals("{\"1\":{\"2\":{\"3\":{\"4\":{\"5\":{\"6\":{\"7\":{\"8\":{\"9\":{\"10\":{}}}}}}}}}}}");
        }
        SECTION("escaped name characters") {
            json.beginObject();
            json.name("a\tb\n").value("a\tb\n");
            json.endObject();
            data.checkEquals("{\"a\\tb\\n\":\"a\\tb\\n\"}");
        }
    }
}

TEST_CASE("JSONBufferWriter") {
    // Primary functionality of this class is inherited from the abstract JSONWriter class, and is already
    // checked in the JSONStreamWriter's test cases. Here we only check methods specific to JSONBufferWriter
    SECTION("construction") {
        test::Buffer data; // Empty buffer
        JSONBufferWriter json((char*)data, data.size());
        CHECK(json.buffer() == (char*)data);
        CHECK(json.bufferSize() == data.size());
        CHECK(json.dataSize() == 0);
        data.checkPadding();
    }

    SECTION("exact buffer size") {
        test::Buffer data(25); // 25 bytes
        JSONBufferWriter json((char*)data, data.size());
        json.beginArray().nullValue().value(true).value(2).value(3.14).value("abcd").endArray();
        CHECK(json.dataSize() == 25);
        data.checkEquals("[null,true,2,3.14,\"abcd\"]");
        data.checkPadding();
    }

    SECTION("too small buffer") {
        test::Buffer data;
        JSONBufferWriter json((char*)data, data.size());
        json.beginArray().nullValue().value(true).value(2).value(3.14).value("abcd").endArray();
        CHECK(json.dataSize() == 25); // Size of the actual JSON data
        data.checkPadding();
    }
}
