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

#include "spark_wiring_print.h"
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

    bool operator==(const char *str) const;
    bool operator!=(const char *str) const;
    bool operator==(const String &str) const;
    bool operator!=(const String &str) const;

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

    JSONString name() const;
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

class JSONWriter {
public:
    JSONWriter();
    virtual ~JSONWriter() = default;

    JSONWriter& beginArray();
    JSONWriter& endArray();
    JSONWriter& beginObject();
    JSONWriter& endObject();
    JSONWriter& name(const char *name);
    JSONWriter& name(const char *name, size_t size);
    JSONWriter& name(const String &name);
    JSONWriter& value(bool val);
    JSONWriter& value(int val);
    JSONWriter& value(unsigned val);
    JSONWriter& value(float val);
    JSONWriter& value(const char *val);
    JSONWriter& value(const char *val, size_t size);
    JSONWriter& value(const String &val);
    JSONWriter& nullValue();

protected:
    virtual void write(const char *data, size_t size) = 0;
    virtual void printf(const char *fmt, ...);

private:
    enum State {
        BEGIN, // Beginning of the document, an array, or an object
        ELEMENT, // Expecting next element
        VALUE // Expecting property value
    };

    State state_;

    void writeSeparator();
    void writeEscaped(const char *str, size_t size);
    void write(char c);
};

class JSONStreamWriter: public JSONWriter {
public:
    explicit JSONStreamWriter(Print &stream);

    Print* stream() const;

protected:
    virtual void write(const char *data, size_t size) override;

private:
    Print &strm_;
};

class JSONBufferWriter: public JSONWriter {
public:
    explicit JSONBufferWriter(char *buf, size_t size);

    char* buffer() const;
    size_t bufferSize() const;

    size_t size() const;

protected:
    virtual void write(const char *data, size_t size) override;
    virtual void printf(const char *fmt, ...) override;

private:
    char *buf_;
    size_t bufSize_, n_;
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

inline bool spark::JSONString::operator!=(const char *str) const {
    return !operator==(str);
}

inline bool spark::JSONString::operator!=(const String &str) const {
    return !operator==(str);
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

inline spark::JSONString spark::JSONObjectIterator::name() const {
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

// spark::JSONWriter
inline spark::JSONWriter::JSONWriter() :
        state_(BEGIN) {
}

inline spark::JSONWriter& spark::JSONWriter::name(const char *name) {
    return this->name(name, strlen(name));
}

inline spark::JSONWriter& spark::JSONWriter::name(const String &name) {
    return this->name(name.c_str(), name.length());
}

inline spark::JSONWriter& spark::JSONWriter::value(const char *val) {
    return value(val, strlen(val));
}

inline spark::JSONWriter& spark::JSONWriter::value(const String &val) {
    return value(val.c_str(), val.length());
}

inline void spark::JSONWriter::write(char c) {
    write(&c, 1);
}

// spark::JSONStreamWriter
inline spark::JSONStreamWriter::JSONStreamWriter(Print &stream) :
        strm_(stream) {
}

inline Print* spark::JSONStreamWriter::stream() const {
    return &strm_;
}

inline void spark::JSONStreamWriter::write(const char *data, size_t size) {
    strm_.write((const uint8_t*)data, size);
}

// spark::JSONBufferWriter
inline spark::JSONBufferWriter::JSONBufferWriter(char *buf, size_t size) :
        buf_(buf),
        bufSize_(size),
        n_(0) {
}

inline char* spark::JSONBufferWriter::buffer() const {
    return buf_;
}

inline size_t spark::JSONBufferWriter::bufferSize() const {
    return bufSize_;
}

inline size_t spark::JSONBufferWriter::size() const {
    return n_;
}

#endif // SPARK_WIRING_JSON_H
