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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for strchrnul()
#endif
#include <cstring>

#include "spark_wiring_logging.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>

#include "spark_wiring_usbserial.h"
#include "spark_wiring_usartserial.h"

namespace {

using namespace spark;

/*
    This class performs processing of configuration requests in JSON format.

    Adding log handler:

    {
      "cmd": "add_handler", // Command name
      "id": "handler_1", // Handler ID
      "hnd": { // Handler settings
        "type": "JSONLogHandler", // Handler type
        "param": { // Type-specific parameters
          ...
        }
      },
      "strm": { // Stream settings
        "type": "Serial1", // Stream type
        "param": { // Type-specific parameters
          ...
        }
      }
      "filt": [ // Category filters
        {
          "cat": "app", // Category name
          "lvl": "all" // Logging level
        }
      ],
      "lvl": "warn" // Default logging level
    }

    Removing log handler:

    {
      "cmd": "remove_handler", // Command name
      "id": "handler_1" // Handler ID
    }

    Enumerating active log handlers:

    {
      "cmd": "enum_handlers" // Command name
    }
*/
class JSONRequestHandler {
public:
    static bool process(const char *reqData, size_t reqSize, char *repData, size_t *repSize) {
        JSONParser parser(reqData, reqSize);
        if (!parser.isValid()) {
            return false; // Parsing error
        }
        Request req;
        if (!parseRequest(parser.value(), &req)) {
            return false;
        }
        JSONBufferWriter writer(repData, repSize ? *repSize : 0);
        if (!processRequest(req, writer)) {
            return false;
        }
        if (repSize) {
            *repSize = writer.size();
        }
        if (writer.size() > writer.bufferSize()) {
            return false;
        }
        return true;
    }

private:
    struct Object {
        JSONString type;
        JSONValue params;
    };

    struct Request {
        Object handler, stream;
        LogCategoryFilters filters;
        JSONString cmd, id;
        LogLevel level;

        Request() :
                level(LOG_LEVEL_NONE) {
        }
    };

    static bool parseRequest(const JSONValue &value, Request *req) {
        JSONObjectIterator it(value);
        while (it.next()) {
            if (it.name() == "cmd") { // Command name
                req->cmd = it.value().toString();
            } else if (it.name() == "id") { // Handler ID
                req->id = it.value().toString();
            } else if (it.name() == "hnd") { // Handler settings
                if (!parseObject(it.value(), &req->handler)) {
                    return false;
                }
            } else if (it.name() == "strm") { // Stream settings
                if (!parseObject(it.value(), &req->stream)) {
                    return false;
                }
            } else if (it.name() == "filt") { // Category filters
                if (!parseFilters(it.value(), &req->filters)) {
                    return false;
                }
            } else if (it.name() == "lvl") { // Default level
                if (!parseLevel(it.value(), &req->level)) {
                    return false;
                }
            }
        }
        return true;
    }

    static bool parseObject(const JSONValue &value, Object *object) {
        JSONObjectIterator it(value);
        while (it.next()) {
            if (it.name() == "type") { // Object type
                object->type = it.value().toString();
            } else if (it.name() == "params") { // Additional parameters
                object->params = it.value();
            }
        }
        return true;
    }

    static bool parseFilters(const JSONValue &value, LogCategoryFilters *filters) {
        JSONArrayIterator it(value);
        if (!filters->reserve(it.count())) {
            return false; // Memory allocation error
        }
        while (it.next()) {
            JSONString cat;
            LogLevel level = LOG_LEVEL_NONE;
            JSONObjectIterator it2(it.value());
            while (it2.next()) {
                if (it2.name() == "cat") { // Category
                    cat = it2.value().toString();
                } else if (it2.name() == "lvl") { // Level
                    if (!parseLevel(it2.value(), &level)) {
                        return false;
                    }
                }
            }
            filters->append(LogCategoryFilter(cat, level));
        }
        return true;
    }

    static bool parseLevel(const JSONValue &value, LogLevel *level) {
        const JSONString name = value.toString();
        static struct {
            const char *name;
            LogLevel level;
        } levels[] = {
                { "none", LOG_LEVEL_NONE },
                { "trace", LOG_LEVEL_TRACE },
                { "info", LOG_LEVEL_INFO },
                { "warn", LOG_LEVEL_WARN },
                { "error", LOG_LEVEL_ERROR },
                { "panic", LOG_LEVEL_PANIC },
                { "all", LOG_LEVEL_ALL }
            };
        static size_t n = sizeof(levels) / sizeof(levels[0]);
        size_t i = 0;
        for (; i < n; ++i) {
            if (name == levels[i].name) {
                break;
            }
        }
        if (i == n) {
            return false; // Unknown level name
        }
        *level = levels[i].level;
        return true;
    }

    static bool processRequest(Request &req, JSONWriter &writer) {
        if (req.cmd == "add_handler") {
            return addHandler(req, writer);
        } else if (req.cmd == "remove_handler") {
            return removeHandler(req, writer);
        } else if (req.cmd == "enum_handlers") {
            return enumHandlers(req, writer);
        } else {
            return false; // Unsupported request
        }
    }

    static bool addHandler(Request &req, JSONWriter&) {
        return logManager()->addNamedHandler(req.id, req.handler.type, req.handler.params, req.stream.type,
                req.stream.params, req.level, req.filters);
    }

    static bool removeHandler(Request &req, JSONWriter&) {
        logManager()->removeNamedHandler(req.id);
        return true;
    }

    static bool enumHandlers(Request &req, JSONWriter &writer) {
        writer.beginArray();
        logManager()->enumNamedHandlers(enumHandlersCallback, &writer);
        writer.endArray();
        return true;
    }

    static void enumHandlersCallback(const char *id, void *data) {
        JSONWriter *writer = static_cast<JSONWriter*>(data);
        writer->value(id);
    }

    static LogManager* logManager() {
        return LogManager::instance();
    }
};

class DefaultLogHandlerFactory: public LogHandlerFactory {
public:
    LogHandler* createHandler(const JSONString &type, const JSONValue &params, Print *stream, LogLevel level,
            const LogCategoryFilters &filters) override {
        if (type == "JSONLogHandler") {
            return createStreamHandler<JSONLogHandler>(stream, level, filters);
        } else if (type == "StreamLogHandler") {
            return createStreamHandler<StreamLogHandler>(stream, level, filters);
        }
        return nullptr; // Unknown handler type
    }

    static DefaultLogHandlerFactory* instance() {
        static DefaultLogHandlerFactory factory;
        return &factory;
    }

private:
    template<typename T>
    static T* createStreamHandler(Print *stream, LogLevel level, const LogCategoryFilters &filters) {
        if (!stream) {
            return nullptr; // Output stream is not specified
        }
        return new T(*stream, level, filters);
    }
};

class DefaultOutputStreamFactory: public OutputStreamFactory {
public:
    Print* createStream(const JSONString &type, const JSONValue &params) override {
        Print *stream = nullptr;
#if PLATFORM_ID != 3
        if (type == "Serial") {
            Serial.begin();
            stream = &Serial;
        }
#if Wiring_USBSerial1
        else if (type == "USBSerial1") {
            USBSerial1.begin();
            stream = &USBSerial1;
        }
#endif
        else if (type == "Serial1") {
            int baud = 9600;
            getParams(params, &baud);
            Serial1.begin(baud);
            stream = &Serial1;
        }
#endif // PLATFORM_ID != 3
        return stream;
    }

    void destroyStream(Print *stream) override {
#if PLATFORM_ID != 3
        if (stream == &Serial) {
            Serial.end();
        }
#if Wiring_USBSerial1
        else if (stream == &USBSerial1) {
            USBSerial1.end();
        }
#endif
        else if (stream == &Serial1) {
            Serial1.end();
        }
#endif // PLATFORM_ID != 3
    }

    static DefaultOutputStreamFactory* instance() {
        static DefaultOutputStreamFactory factory;
        return &factory;
    }

private:
    static void getParams(const JSONValue &params, int *baudRate) {
        JSONObjectIterator it(params);
        while (it.next()) {
            if (it.name() == "baud" && baudRate) {
                *baudRate = it.value().toInt();
            }
        }
    }
};

#if PLATFORM_ID == 3
// GCC on some platforms doesn't provide strchrnul()
inline const char* strchrnul(const char *s, char c) {
    while (*s && *s != c) {
        ++s;
    }
    return s;
}
#endif

// Slightly tweaked version of std::lower_bound() taking strcmp-alike comparator function
template<typename T, typename CompareT, typename... ArgsT>
int lowerBound(const spark::Array<T> &array, CompareT compare, bool &found, ArgsT... args) {
    found = false;
    int index = 0;
    int n = array.size();
    while (n > 0) {
        const int half = n >> 1;
        const int i = index + half;
        const int cmp = compare(array.at(i), args...);
        if (cmp < 0) {
            index = i + 1;
            n -= half + 1;
        } else {
            if (cmp == 0) {
                found = true;
            }
            n = half;
        }
    }
    return index;
}

// Iterates over subcategory names separated by '.' character
const char* nextSubcategoryName(const char* &category, size_t &size) {
    const char *s = strchrnul(category, '.');
    size = s - category;
    if (size) {
        if (*s) {
            ++s;
        }
        std::swap(s, category);
        return s;
    }
    return nullptr;
}

const char* extractFileName(const char *s) {
    const char *s1 = strrchr(s, '/');
    if (s1) {
        return s1 + 1;
    }
    return s;
}

const char* extractFuncName(const char *s, size_t *size) {
    const char *s1 = s;
    for (; *s; ++s) {
        if (*s == ' ') {
            s1 = s + 1; // Skip return type
        } else if (*s == '(') {
            break; // Skip argument types
        }
    }
    *size = s - s1;
    return s1;
}

} // namespace

// Default logger instance. This code is compiled as part of the wiring library which has its own
// category name specified at module level, so here we use "app" category name explicitly
const spark::Logger spark::Log("app");

/*
    LogFilter instance maintains a prefix tree based on a list of category filter strings. Every
    node of the tree contains a subcategory name and, optionally, a logging level - if node matches
    complete filter string. For example, given the following filters:

    a (error)
    a.b.c (trace)
    a.b.x (trace)
    aa (error)
    aa.b (warn)

    LogFilter builds the following prefix tree:

    |
    |- a (error) -- b - c (trace)
    |               |
    |               `-- x (trace)
    |
    `- aa (error) - b (warn)
*/

// spark::LogFilter
struct spark::LogFilter::Node {
    const char *name; // Subcategory name
    uint16_t size; // Name length
    int16_t level; // Logging level (-1 if not specified for this node)
    Array<Node> nodes; // Children nodes

    Node(const char *name, uint16_t size) :
            name(name),
            size(size),
            level(-1) {
    }
};

spark::LogFilter::LogFilter(LogLevel level) :
        level_(level) {
}

spark::LogFilter::LogFilter(LogLevel level, LogCategoryFilters filters) :
        level_(LOG_LEVEL_NONE) { // Fallback level that will be used in case of construction errors
    // Store category names
    Array<String> cats;
    if (!cats.reserve(filters.size())) {
        return;
    }
    for (LogCategoryFilter &filter: filters) {
        cats.append(std::move(filter.cat_));
    }
    // Process category filters
    Array<Node> nodes;
    for (int i = 0; i < cats.size(); ++i) {
        const char *category = cats.at(i).c_str();
        if (!category) {
            continue; // Invalid usage or string allocation error
        }
        Array<Node> *pNodes = &nodes; // Root nodes
        const char *name = nullptr; // Subcategory name
        size_t size = 0; // Name length
        while ((name = nextSubcategoryName(category, size))) {
            bool found = false;
            const int index = nodeIndex(*pNodes, name, size, found);
            if (!found && !pNodes->insert(index, Node(name, size))) { // Add node
                return;
            }
            Node &node = pNodes->at(index);
            if (!*category) { // Check if it's last subcategory
                node.level = filters.at(i).level_;
            }
            pNodes = &node.nodes;
        }
    }
    using std::swap;
    swap(cats_, cats);
    swap(nodes_, nodes);
    level_ = level;
}

spark::LogFilter::~LogFilter() {
}

LogLevel spark::LogFilter::level(const char *category) const {
    LogLevel level = level_; // Default level
    if (!nodes_.isEmpty() && category) {
        const Array<Node> *pNodes = &nodes_; // Root nodes
        const char *name = nullptr; // Subcategory name
        size_t size = 0; // Name length
        while ((name = nextSubcategoryName(category, size))) {
            bool found = false;
            const int index = nodeIndex(*pNodes, name, size, found);
            if (!found) {
                break;
            }
            const Node &node = pNodes->at(index);
            if (node.level >= 0) {
                level = (LogLevel)node.level;
            }
            pNodes = &node.nodes;
        }
    }
    return level;
}

int spark::LogFilter::nodeIndex(const Array<Node> &nodes, const char *name, size_t size, bool &found) {
    return lowerBound(nodes, [](const Node &node, const char *name, uint16_t size) {
            const int cmp = strncmp(node.name, name, std::min(node.size, size));
            if (cmp == 0) {
                return (int)node.size - (int)size;
            }
            return cmp;
        }, found, name, (uint16_t)size);
}

// spark::StreamLogHandler
void spark::StreamLogHandler::logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) {
    char buf[16];
    const char *s;
    size_t n;
    // Timestamp
    if (attr.has_time) {
        n = snprintf(buf, sizeof(buf), "%010u ", (unsigned)attr.time);
        write(buf, std::min(n, sizeof(buf) - 1));
    }
    // Category
    if (category) {
        write("[", 1);
        write(category);
        write("] ", 2);
    }
    // Source file
    if (attr.has_file) {
        s = extractFileName(attr.file); // Strip directory path
        write(s); // File name
        if (attr.has_line) {
            write(":", 1);
            n = snprintf(buf, sizeof(buf), "%d", (int)attr.line); // Line number
            write(buf, std::min(n, sizeof(buf) - 1));
        }
        if (attr.has_function) {
            write(", ", 2);
        } else {
            write(": ", 2);
        }
    }
    // Function name
    if (attr.has_function) {
        s = extractFuncName(attr.function, &n); // Strip argument and return types
        write(s, n);
        write("(): ", 4);
    }
    // Level
    s = levelName(level);
    write(s);
    write(": ", 2);
    // Message
    if (msg) {
        write(msg);
    }
    // Additional attributes
    if (attr.has_code || attr.has_details) {
        write(" [", 2);
        if (attr.has_code) {
            write("code = ", 7);
            n = snprintf(buf, sizeof(buf), "%" PRIiPTR, (intptr_t)attr.code);
            write(buf, std::min(n, sizeof(buf) - 1));
        }
        if (attr.has_details) {
            if (attr.has_code) {
                write(", ", 2);
            }
            write("details = ", 10);
            write(attr.details);
        }
        write("]", 1);
    }
    write("\r\n", 2);
}

// spark::JSONLogHandler
void spark::JSONLogHandler::logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) {
    writer_.beginObject();
    // Level
    const char *s = levelName(level);
    writer_.name("level", 5).value(s);
    // Message
    if (msg) {
        writer_.name("message", 7).value(msg);
    }
    // Category
    if (category) {
        writer_.name("category", 8).value(category);
    }
    // File name
    if (attr.has_file) {
        s = extractFileName(attr.file); // Strip directory path
        writer_.name("file", 4).value(s);
    }
    // Line number
    if (attr.has_line) {
        writer_.name("line", 4).value(attr.line);
    }
    // Function name
    if (attr.has_function) {
        size_t n = 0;
        s = extractFuncName(attr.function, &n); // Strip argument and return types
        writer_.name("function", 8).value(s, n);
    }
    // Timestamp
    if (attr.has_time) {
        writer_.name("time", 4).value((unsigned)attr.time);
    }
    // Code
    if (attr.has_code) {
        writer_.name("code", 4).value(attr.code);
    }
    // Details
    if (attr.has_details) {
        writer_.name("details", 7).value(attr.details);
    }
    writer_.endObject();
    write("\r\n", 2);
}

// spark::LogManager
struct spark::LogManager::NamedHandler {
    String id;
    LogHandler *handler;
    LogHandlerFactory *handlerFactory;
    Print *stream;
    OutputStreamFactory *streamFactory;

    explicit NamedHandler(const JSONString &id) :
            id(id),
            handler(nullptr),
            handlerFactory(nullptr),
            stream(nullptr),
            streamFactory(nullptr) {
    }
};

spark::LogManager::LogManager() {
    // Register default factory instances
    handlerFactories_.append(DefaultLogHandlerFactory::instance());
    streamFactories_.append(DefaultOutputStreamFactory::instance());
}

spark::LogManager::~LogManager() {
}

bool spark::LogManager::addHandler(LogHandler *handler) {
    WITH_LOCK(mutex_) {
        if (activeHandlers_.contains(handler) || !activeHandlers_.append(handler)) {
            return false;
        }
        if (activeHandlers_.size() == 1) {
            log_set_callbacks(logMessage, logWrite, logEnabled, nullptr); // Set system callbacks
        }
    }
    return true;
}

void spark::LogManager::removeHandler(LogHandler *handler) {
    WITH_LOCK(mutex_) {
        if (activeHandlers_.removeOne(handler) && activeHandlers_.isEmpty()) {
            log_set_callbacks(nullptr, nullptr, nullptr, nullptr); // Reset system callbacks
        }
    }
}

bool spark::LogManager::addHandlerFactory(LogHandlerFactory *factory) {
    WITH_LOCK(mutex_) {
        if (handlerFactories_.contains(factory) || !handlerFactories_.append(factory)) {
            return false;
        }
    }
    return true;
}

void spark::LogManager::removeHandlerFactory(LogHandlerFactory *factory) {
    WITH_LOCK(mutex_) {
        if (handlerFactories_.removeOne(factory)) {
            // Destroying all handlers created via the factory that is about to be unregistered
            int i = 0;
            while (i < namedHandlers_.size()) {
                NamedHandler &h = namedHandlers_[i];
                if (h.handlerFactory == factory) {
                    destroyNamedHandler(h);
                    namedHandlers_.remove(i);
                } else {
                    ++i;
                }
            }
        }
    }
}

bool spark::LogManager::addStreamFactory(OutputStreamFactory *factory) {
    WITH_LOCK(mutex_) {
        if (streamFactories_.contains(factory) || !streamFactories_.append(factory)) {
            return false;
        }
    }
    return true;
}

void spark::LogManager::removeStreamFactory(OutputStreamFactory *factory) {
    WITH_LOCK(mutex_) {
        if (streamFactories_.removeOne(factory)) {
            // Destroying all handlers that use streams created via the factory that is about to be unregistered
            int i = 0;
            while (i < namedHandlers_.size()) {
                NamedHandler &h = namedHandlers_[i];
                if (h.streamFactory == factory) {
                    destroyNamedHandler(h);
                    namedHandlers_.remove(i);
                } else {
                    ++i;
                }
            }
        }
    }
}

bool spark::LogManager::addNamedHandler(const JSONString &id, const JSONString &handlerType, const JSONValue &handlerParams,
        const JSONString &streamType, const JSONValue &streamParams, LogLevel level, LogCategoryFilters filters) {
    WITH_LOCK(mutex_) {
        const int i = namedHandlerIndex(id);
        if (i != -1) {
            destroyNamedHandler(namedHandlers_[i]); // Destroy existent handler with the same ID
            namedHandlers_.remove(i);
        }
        NamedHandler h(id);
        if (!h.id.length()) {
            return false; // Memory allocation error
        }
        // Create output stream (if necessary)
        if (!streamType.isEmpty()) {
            for (OutputStreamFactory *factory: streamFactories_) {
                h.stream = factory->createStream(streamType, streamParams);
                if (h.stream) {
                    h.streamFactory = factory; // Keep factory instance
                    break;
                }
            }
            if (!h.stream) {
                return false; // Unsupported stream type
            }
        }
        // Create log handler
        for (LogHandlerFactory *factory: handlerFactories_) {
            h.handler = factory->createHandler(handlerType, handlerParams, h.stream, level, filters);
            if (h.handler) {
                h.handlerFactory = factory; // Keep factory instance
                break;
            }
        }
        if (!h.handler) {
            return false; // Unsupported handler type
        }
        if (!namedHandlers_.append(h) || !activeHandlers_.append(h.handler)) {
            return false;
        }
        // FIXME: Destroy stream in case of error
    }
    return true;
}

void spark::LogManager::removeNamedHandler(const JSONString &id) {
    WITH_LOCK(mutex_) {
        const int i = namedHandlerIndex(id);
        if (i != -1) {
            destroyNamedHandler(namedHandlers_[i]);
            namedHandlers_.remove(i);
        }
    }
}

void spark::LogManager::enumNamedHandlers(void(*callback)(const char *id, void *data), void *data) {
    SPARK_ASSERT(callback);
    WITH_LOCK(mutex_) {
        for (const NamedHandler &handler: namedHandlers_) {
            callback(handler.id.c_str(), data);
        }
    }
}

int spark::LogManager::namedHandlerIndex(const JSONString &id) const {
    for (int i = 0; i < namedHandlers_.size(); ++i) {
        if (namedHandlers_.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

void spark::LogManager::destroyNamedHandler(const NamedHandler &h) {
    activeHandlers_.removeOne(h.handler);
    SPARK_ASSERT(h.handlerFactory);
    h.handlerFactory->destroyHandler(h.handler);
    if (h.stream) {
        SPARK_ASSERT(h.streamFactory);
        h.streamFactory->destroyStream(h.stream);
    }
}

spark::LogManager* spark::LogManager::instance() {
    static LogManager mgr;
    return &mgr;
}

void spark::LogManager::logMessage(const char *msg, int level, const char *category, const LogAttributes *attr, void *reserved) {
    LogManager *that = instance();
    WITH_LOCK(that->mutex_) {
        for (LogHandler *handler: that->activeHandlers_) {
            handler->message(msg, (LogLevel)level, category, *attr);
        }
    }
}

void spark::LogManager::logWrite(const char *data, size_t size, int level, const char *category, void *reserved) {
    LogManager *that = instance();
    WITH_LOCK(that->mutex_) {
        for (LogHandler *handler: that->activeHandlers_) {
            handler->write(data, size, (LogLevel)level, category);
        }
    }
}

int spark::LogManager::logEnabled(int level, const char *category, void *reserved) {
    LogManager *that = instance();
    int minLevel = LOG_LEVEL_NONE;
    WITH_LOCK(that->mutex_) {
        for (LogHandler *handler: that->activeHandlers_) {
            const int level = handler->level(category);
            if (level < minLevel) {
                minLevel = level;
            }
        }
    }
    return (level >= minLevel);
}

// spark::*
bool spark::logProcessConfigRequest(const char *reqData, size_t reqSize, char *repData, size_t *repSize, DataFormat fmt) {
    if (fmt == DATA_FORMAT_JSON) {
        return JSONRequestHandler::process(reqData, reqSize, repData, repSize);
    }
    return false; // Unsupported request format
}
