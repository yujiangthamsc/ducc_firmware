/*
 * Copyright (c) 2016 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SPARK_WIRING_JSON_H
#define SPARK_WIRING_JSON_H

#include <cstring>

#include "jsmn.h"

#include "spark_wiring_string.h"
#include "spark_wiring_array.h"

namespace spark {

class JSONString;

class JSONValue {
public:
    enum Type {
        TYPE_INVALID,
        TYPE_NULL,
        TYPE_BOOL,
        TYPE_NUMBER,
        TYPE_STRING,
        TYPE_ARRAY,
        TYPE_OBJECT
    };

    JSONValue();

    bool toBool() const;
    int toInt() const;
    float toFloat() const;
    JSONString toString() const;

    Type type() const;

    bool isNull() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    bool isValid() const;

private:
    const jsmntok_t *t_;
    const char *d_;

    JSONValue(const jsmntok_t *token, const char *data);

    friend class JSONString;
    friend class JSONArrayIterator;
    friend class JSONObjectIterator;
    friend class JSONParser;
};

class JSONObjectIterator;

class JSONString {
public:
    JSONString();
    explicit JSONString(const JSONValue &value);

    const char* data() const;

    size_t size() const;
    bool isEmpty() const;

    operator String() const;

    bool operator==(const char *s) const;
    bool operator!=(const char *s) const;

private:
    const char *d_;
    size_t n_;

    JSONString(const jsmntok_t *token, const char *data);

    friend class JSONObjectIterator;
};

class JSONArrayIterator {
public:
    JSONArrayIterator();
    explicit JSONArrayIterator(const JSONValue &value);

    bool next();

    JSONValue value() const;

    size_t count() const;

    bool isValid() const;

private:
    const jsmntok_t *t_, *v_;
    const char *d_;
    size_t n_;
};

class JSONObjectIterator {
public:
    JSONObjectIterator();
    explicit JSONObjectIterator(const JSONValue &value);

    bool next();

    JSONString key() const;
    JSONValue value() const;

    size_t count() const;

    bool isValid() const;

private:
    const jsmntok_t *t_, *k_, *v_;
    const char *d_;
    size_t n_;
};

class JSONParser {
public:
    JSONParser();
    explicit JSONParser(const char *data);
    JSONParser(const char *data, size_t size);

    bool parse(const char *data);
    bool parse(const char *data, size_t size);

    JSONValue value() const;

    bool isValid() const;

private:
    Array<jsmntok_t> t_;
    const char *d_;
};

} // namespace spark

// spark::JSONValue
inline spark::JSONValue::JSONValue() :
        t_(nullptr),
        d_(nullptr) {
}

inline spark::JSONValue::JSONValue(const jsmntok_t *token, const char *data) :
        t_(token),
        d_(data) {
}

inline spark::JSONString spark::JSONValue::toString() const {
    return JSONString(*this);
}

inline bool spark::JSONValue::isNull() const {
    return type() == TYPE_NULL;
}

inline bool spark::JSONValue::isNumber() const {
    return type() == TYPE_NUMBER;
}

inline bool spark::JSONValue::isString() const {
    return type() == TYPE_STRING;
}

inline bool spark::JSONValue::isArray() const {
    return type() == TYPE_ARRAY;
}

inline bool spark::JSONValue::isObject() const {
    return type() == TYPE_OBJECT;
}

inline bool spark::JSONValue::isValid() const {
    return type() != TYPE_INVALID;
}

// spark::JSONString
inline spark::JSONString::JSONString() :
        d_(nullptr),
        n_(0) {
}

inline spark::JSONString::JSONString(const JSONValue &value) :
        JSONString(value.t_, value.d_) {
}

inline const char* spark::JSONString::data() const {
    return d_;
}

inline size_t spark::JSONString::size() const {
    return n_;
}

inline bool spark::JSONString::isEmpty() const {
    return !n_;
}

inline spark::JSONString::operator String() const {
    return String(d_, n_);
}

inline bool spark::JSONString::operator!=(const char *s) const {
    return !operator==(s);
}

// spark::JSONArrayIterator
inline spark::JSONArrayIterator::JSONArrayIterator() :
        t_(nullptr),
        v_(nullptr),
        d_(nullptr),
        n_(0) {
}

inline size_t spark::JSONArrayIterator::count() const {
    return n_;
}

inline spark::JSONString spark::JSONObjectIterator::key() const {
    return JSONString(k_, d_);
}

inline spark::JSONValue spark::JSONArrayIterator::value() const {
    return JSONValue(v_, d_);
}

inline bool spark::JSONArrayIterator::isValid() const {
    return t_ && d_;
}

// spark::JSONObjectIterator
inline spark::JSONObjectIterator::JSONObjectIterator() :
        t_(nullptr),
        k_(nullptr),
        v_(nullptr),
        d_(nullptr),
        n_(0) {
}

inline spark::JSONValue spark::JSONObjectIterator::value() const {
    return JSONValue(v_, d_);
}

inline size_t spark::JSONObjectIterator::count() const {
    return n_;
}

inline bool spark::JSONObjectIterator::isValid() const {
    return t_ && d_;
}

// spark::JSONParser
inline spark::JSONParser::JSONParser() :
        d_(nullptr) {
}

inline spark::JSONParser::JSONParser(const char *data) :
        JSONParser() {
    parse(data);
}

inline spark::JSONParser::JSONParser(const char *data, size_t size) :
        JSONParser() {
    parse(data, size);
}

inline bool spark::JSONParser::parse(const char *data) {
    return parse(data, strlen(data));
}

inline spark::JSONValue spark::JSONParser::value() const {
    return JSONValue(t_.data(), d_);
}

inline bool spark::JSONParser::isValid() const {
    return !t_.isEmpty();
}

#endif // SPARK_WIRING_JSON_H
