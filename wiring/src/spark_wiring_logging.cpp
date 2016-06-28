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

#include <algorithm>
#include <cinttypes>
#include <cstdio>

#include "spark_wiring_logging.h"

namespace {

#if PLATFORM_ID == 3
// GCC on some platforms doesn't provide strchnull() regardless of _GNU_SOURCE feature macro
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

// Iterates over subcategory names
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
// category name specified at module level, so here we specify "app" category name explicitly
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
    // Timestamp
    if (attr.has_time) {
        snprintf(buf, sizeof(buf), "%010u ", (unsigned)attr.time);
        write(buf);
    }
    // Category
    if (category) {
        write("[");
        write(category);
        write("] ");
    }
    // Source file
    if (attr.has_file) {
        // Strip directory path
        const char *s = extractFileName(attr.file);
        write(s); // File name
        if (attr.has_line) {
            write(":");
            snprintf(buf, sizeof(buf), "%d", attr.line); // Line number
            write(buf);
        }
        if (attr.has_function) {
            write(", ");
        } else {
            write(": ");
        }
    }
    // Function name
    if (attr.has_function) {
        // Strip argument and return types for better readability
        size_t n = 0;
        const char *s = extractFuncName(attr.function, &n);
        write(s, n);
        write("(): ");
    }
    // Level
    write(levelName(level));
    write(": ");
    // Message
    if (msg) {
        write(msg);
    }
    // Additional attributes
    if (attr.has_code || attr.has_details) {
        write(" [");
        if (attr.has_code) {
            write("code");
            write(" = ");
            snprintf(buf, sizeof(buf), "%" PRIiPTR, attr.code);
            write(buf);
        }
        if (attr.has_details) {
            if (attr.has_code) {
                write(", ");
            }
            write("details");
            write(" = ");
            write(attr.details);
        }
        write("]");
    }
    write("\r\n");
}

// spark::LogManager
void spark::LogManager::addHandler(LogHandler *handler) {
    const auto it = std::find(handlers_.cbegin(), handlers_.cend(), handler);
    if (it == handlers_.end()) {
        handlers_.push_back(handler);
        if (handlers_.size() == 1) {
            log_set_callbacks(logMessage, logWrite, logEnabled, nullptr); // Set system callbacks
        }
    }
}

void spark::LogManager::removeHandler(LogHandler *handler) {
    const auto it = std::find(handlers_.begin(), handlers_.end(), handler);
    if (it != handlers_.end()) {
        if (handlers_.size() == 1) {
            log_set_callbacks(nullptr, nullptr, nullptr, nullptr); // Reset system callbacks
        }
        handlers_.erase(it);
    }
}

spark::LogManager* spark::LogManager::instance() {
    static LogManager mgr;
    return &mgr;
}

void spark::LogManager::logMessage(const char *msg, int level, const char *category, const LogAttributes *attr, void *reserved) {
    const auto &handlers = instance()->handlers_;
    for (size_t i = 0; i < handlers.size(); ++i) {
        handlers[i]->message(msg, (LogLevel)level, category, *attr);
    }
}

void spark::LogManager::logWrite(const char *data, size_t size, int level, const char *category, void *reserved) {
    const auto &handlers = instance()->handlers_;
    for (size_t i = 0; i < handlers.size(); ++i) {
        handlers[i]->write(data, size, (LogLevel)level, category);
    }
}

int spark::LogManager::logEnabled(int level, const char *category, void *reserved) {
    int minLevel = LOG_LEVEL_NONE;
    const auto &handlers = instance()->handlers_;
    for (size_t i = 0; i < handlers.size(); ++i) {
        const int level = handlers[i]->level(category);
        if (level < minLevel) {
            minLevel = level;
        }
    }
    return (level >= minLevel);
}
