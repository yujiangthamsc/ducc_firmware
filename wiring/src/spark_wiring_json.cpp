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

#include "spark_wiring_json.h"

#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

namespace {

// Skips token and all its children tokens if any
const jsmntok_t* skipToken(const jsmntok_t *t) {
    size_t n = 1;
    do {
        if (t->type == JSMN_OBJECT) {
            n += t->size * 2;
        } else if (t->type == JSMN_ARRAY) {
            n += t->size;
        }
        ++t;
        --n;
    } while (n);
    return t;
}

} // namespace

// spark::JSONValue
bool spark::JSONValue::toBool() const {
    switch (type()) {
    case TYPE_BOOL:
        return d_[t_->start] == 't'; // Valid literals are always in lower case
    case TYPE_NUMBER:
        return toFloat();
    default:
        return false;
    }
}

int spark::JSONValue::toInt() const {
    if (!t_ || (t_->type != JSMN_STRING && t_->type != JSMN_PRIMITIVE) || !d_) {
        return 0;
    }
    return String(d_ + t_->start, t_->end - t_->start).toInt(); // TODO: Remove memory allocations
}

float spark::JSONValue::toFloat() const {
    if (!t_ || (t_->type != JSMN_STRING && t_->type != JSMN_PRIMITIVE) || !d_) {
        return 0.0;
    }
    return String(d_ + t_->start, t_->end - t_->start).toFloat(); // TODO: Remove memory allocations
}

spark::JSONValue::Type spark::JSONValue::type() const {
    if (!t_) {
        return TYPE_INVALID;
    }
    switch (t_->type) {
    case JSMN_PRIMITIVE: {
        if (!d_) {
            return TYPE_INVALID;
        }
        const char c = d_[t_->start];
        if (c == '-' || (c >= '0' && c <= '9')) {
            return TYPE_NUMBER;
        } else if (c == 't' || c == 'f') {
            return TYPE_BOOL;
        } else if (c == 'n') {
            return TYPE_NULL;
        }
        return TYPE_INVALID;
    }
    case JSMN_STRING:
        return TYPE_STRING;
    case JSMN_ARRAY:
        return TYPE_ARRAY;
    case JSMN_OBJECT:
        return TYPE_OBJECT;
    default:
        return TYPE_INVALID;
    }
}

// spark::JSONString
spark::JSONString::JSONString(const jsmntok_t *token, const char *data) :
        JSONString() {
    if (token && (token->type == JSMN_STRING || token->type == JSMN_PRIMITIVE) && data) {
        d_ = data + token->start;
        n_ = token->end - token->start;
    }
}

bool spark::JSONString::operator==(const char *str) const {
    const size_t n = strlen(str);
    return n_ == n && strncmp(d_, str, n) == 0;
}

bool spark::JSONString::operator==(const String &str) const {
    const size_t n = str.length();
    return n_ == n && strncmp(d_, str.c_str(), n) == 0;
}

// spark::JSONArrayIterator
spark::JSONArrayIterator::JSONArrayIterator(const JSONValue &value) :
        JSONArrayIterator() {
    const jsmntok_t *t = value.t_;
    if (t && t->type == JSMN_ARRAY) {
        t_ = t + 1;
        d_ = value.d_;
        n_ = t->size;
    }
}

bool spark::JSONArrayIterator::next() {
    if (!n_) {
        return false;
    }
    v_ = t_;
    if (--n_) {
        t_ = skipToken(t_);
    }
    return true;
}

// spark::JSONObjectIterator
spark::JSONObjectIterator::JSONObjectIterator(const JSONValue &value) :
        JSONObjectIterator() {
    const jsmntok_t *t = value.t_;
    if (t && t->type == JSMN_OBJECT) {
        t_ = t + 1;
        d_ = value.d_;
        n_ = t->size;
    }
}

bool spark::JSONObjectIterator::next() {
    if (!n_ || t_->type != JSMN_STRING) {
        return false;
    }
    k_ = t_++;
    v_ = t_;
    if (--n_) {
        t_ = skipToken(t_);
    }
    return true;
}

// spark::JSONParser
bool spark::JSONParser::parse(const char *data, size_t size) {
    jsmn_parser parser;
    parser.size = sizeof(jsmn_parser);
    jsmn_init(&parser, nullptr);
    const int n = jsmn_parse(&parser, data, size, nullptr, 0, nullptr); // Get number of tokens
    if (n <= 0) {
        return false; // Parsing error
    }
    Array<jsmntok_t> tokens(n);
    if (tokens.isEmpty()) {
        return false; // Memory allocation error
    }
    if (jsmn_parse(&parser, data, size, tokens.data(), tokens.size(), nullptr) <= 0) {
        return false;
    }
    swap(t_, tokens);
    d_ = data;
    return true;
}

// spark::JSONWriter
spark::JSONWriter& spark::JSONWriter::beginArray() {
    writeSeparator();
    write('[');
    state_ = BEGIN;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::endArray() {
    write(']');
    state_ = ELEMENT;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::beginObject() {
    writeSeparator();
    write('{');
    state_ = BEGIN;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::endObject() {
    write('}');
    state_ = ELEMENT;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::name(const char *name, size_t size) {
    writeSeparator();
    writeEscaped(name, size);
    state_ = VALUE;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::value(bool val) {
    writeSeparator();
    if (val) {
        write("true", 4);
    } else {
        write("false", 5);
    }
    state_ = ELEMENT;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::value(int val) {
    writeSeparator();
    printf("%d", val);
    state_ = ELEMENT;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::value(unsigned val) {
    writeSeparator();
    printf("%u", val);
    state_ = ELEMENT;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::value(float val) {
    writeSeparator();
    printf("%f", val);
    state_ = ELEMENT;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::value(const char *val, size_t size) {
    writeSeparator();
    writeEscaped(val, size);
    state_ = ELEMENT;
    return *this;
}

spark::JSONWriter& spark::JSONWriter::nullValue() {
    writeSeparator();
    write("null", 4);
    state_ = ELEMENT;
    return *this;
}

void spark::JSONWriter::printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[16];
    const int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if ((size_t)n >= sizeof(buf)) {
        char buf[n + 1]; // Use larger buffer
        va_start(args, fmt);
        if (vsnprintf(buf, sizeof(buf), fmt, args) > 0) {
            write(buf, n);
        }
        va_end(args);
    } else if (n > 0) {
        write(buf, n);
    }
    va_end(args);
}

void spark::JSONWriter::writeSeparator() {
    switch (state_) {
    case ELEMENT:
        write(',');
        break;
    case VALUE:
        write(':');
        break;
    default:
        break;
    }
}

void spark::JSONWriter::writeEscaped(const char *str, size_t size) {
    write('"');
    const char *s = str;
    const char* const end = str + size;
    while (s != end) {
        const char c = *s;
        if (c == '"' || c == '\\' || (c >= 0 && c <= 0x1f)) { // RFC 7159, 7. Strings
            write(str, s - str); // Write preceeding characters
            write('\\');
            if (c <= 0x1f) {
                printf("u%04x", (unsigned)c); // Control characters are written in hex, e.g. "\u001f"
            } else {
                write(c);
            }
            str = s + 1;
        }
        ++s;
    }
    write(str, s - str); // Write remaining part of the string
    write('"');
}

// spark::JSONBufferWriter
void spark::JSONBufferWriter::write(const char *data, size_t size) {
    if (n_ < bufSize_) {
        memcpy(buf_ + n_, data, std::min(size, bufSize_ - n_));
    }
    n_ += size;
}

void spark::JSONBufferWriter::printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(buf_ + n_, (n_ < bufSize_) ? bufSize_ - n_ : 0, fmt, args);
    va_end(args);
    n_ += n;
}
