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

#ifndef SPARK_WIRING_ARRAY_H
#define SPARK_WIRING_ARRAY_H

#include <cstring>
#include <cstdlib>
#include <type_traits>
#include <iterator>
#include <utility>
/*
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
*/
// GCC didn't support std::is_trivially_copyable trait until 5.1.0
#if defined(__GNUC__) && (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 < 50100)
#define PARTICLE_ARRAY_TRIVIALLY_COPYABLE_TRAIT std::is_pod
#else
#define PARTICLE_ARRAY_TRIVIALLY_COPYABLE_TRAIT std::is_trivially_copyable
#endif

// Helper macros for SFINAE-based method selection
#define PARTICLE_ARRAY_ENABLE_IF_TRIVIALLY_COPYABLE(T) \
        typename EnableT = T, typename std::enable_if<PARTICLE_ARRAY_TRIVIALLY_COPYABLE_TRAIT<EnableT>::value, int>::type = 0

#define PARTICLE_ARRAY_ENABLE_IF_NOT_TRIVIALLY_COPYABLE(T) \
        typename EnableT = T, typename std::enable_if<!PARTICLE_ARRAY_TRIVIALLY_COPYABLE_TRAIT<EnableT>::value, int>::type = 0

namespace spark {

struct DefaultAllocator {
    static void* malloc(size_t size);
    static void* realloc(void* ptr, size_t size);
    static void free(void* ptr);
};

template<typename T, typename AllocatorT = DefaultAllocator>
class Array {
public:
    Array();
    explicit Array(int n);
    Array(int n, const T& value);
    Array(const T* values, int n);
    Array(std::initializer_list<T> values);
    template<typename IteratorT> Array(IteratorT begin, IteratorT end);
    Array(const Array<T, AllocatorT>& array, int i, int n);
    Array(const Array<T, AllocatorT>& array);
    Array(Array<T, AllocatorT>&& array);
    ~Array();

    bool append(T value);
    bool append(int n, const T& value);
    bool append(const T* values, int n);
    bool append(const Array<T, AllocatorT>& array);
    bool append(const Array<T, AllocatorT>& array, int i, int n);
    template<typename IteratorT> bool append(IteratorT begin, IteratorT end);

    bool prepend(T value);
    bool prepend(int n, const T& value);
    bool prepend(const T* values, int n);
    bool prepend(const Array<T, AllocatorT>& array);
    bool prepend(const Array<T, AllocatorT>& array, int i, int n);
    template<typename IteratorT> bool prepend(IteratorT begin, IteratorT end);

    bool insert(int i, T value);
    bool insert(int i, int n, const T& value);
    bool insert(int i, const T* values, int n);
    bool insert(int i, const Array<T, AllocatorT>& array);
    bool insert(int i, const Array<T, AllocatorT>& array, int i2, int n);
    template<typename IteratorT> bool insert(int i, IteratorT begin, IteratorT end);

    void remove(int i, int n = 1);

    int removeOne(const T& value);
    int removeOne(const T& value, int i, int n);
    int removeAll(const T& value);
    int removeAll(const T& value, int i, int n);

    bool replace(int i, int n, const T& value, int n2);
    bool replace(int i, int n, const T* values, int n2);
    bool replace(int i, int n, const Array<T, AllocatorT>& array);
    bool replace(int i, int n, const Array<T, AllocatorT>& array, int i2, int n2);
    template<typename IteratorT> bool replace(int i, int n, IteratorT begin, IteratorT end);

    int replaceOne(const T& value, T value2);
    int replaceOne(const T& value, T value2, int i, int n);
    int replaceAll(const T& value, const T& value2);
    int replaceAll(const T& value, const T& value2, int i, int n);

    void fill(const T& value);
    void fill(int i, int n, const T& value);

    T takeFirst();
    T takeLast();
    T take(int i);
    Array<T, AllocatorT> take(int i, int n);

    Array<T, AllocatorT> copy(int i, int n) const;

    int indexOf(const T& value) const;
    int indexOf(const T& value, int i, int n) const;
    int lastIndexOf(const T& value) const;
    int lastIndexOf(const T& value, int i, int n) const;

    bool contains(const T& value) const;
    bool contains(const T& value, int i, int n) const;
    int count(const T& value) const;
    int count(const T& value, int i, int n) const;

    bool resize(int n);
    int size() const;
    bool isEmpty() const;

    bool reserve(int n);
    int capacity() const;
    bool trim();

    void clear();

    T* data();
    const T* data() const;

    T* begin();
    const T* begin() const;
    T* end();
    const T* end() const;

    T& first();
    const T& first() const;
    T& last();
    const T& last() const;
    T& at(int i);
    const T& at(int i) const;

    T& operator[](int i);
    const T& operator[](int i) const;

    bool operator==(const Array<T, AllocatorT> &array) const;
    bool operator!=(const Array<T, AllocatorT> &array) const;

    Array<T, AllocatorT>& operator=(Array<T, AllocatorT> array);

private:
    T* data_;
    int size_, capacity_;

    template<PARTICLE_ARRAY_ENABLE_IF_TRIVIALLY_COPYABLE(T)>
    bool realloc(int n) {
        //if (n >= size_) {
            T* d = nullptr;
            if (n > 0) {
                d = (T*)AllocatorT::realloc(data_, n * sizeof(T));
                if (!d) {
                    return false;
                }
            } else {
                AllocatorT::free(data_);
            }
            data_ = d;
            capacity_ = n;
        //}
        return true;
    }

    template<PARTICLE_ARRAY_ENABLE_IF_NOT_TRIVIALLY_COPYABLE(T)>
    bool realloc(int n) {
        //if (n >= size_) {
            T* d = nullptr;
            if (n > 0) {
                d = (T*)AllocatorT::malloc(n * sizeof(T));
                if (!d) {
                    return false;
                }
                move(d, data_, data_ + size_);
            }
            AllocatorT::free(data_);
            data_ = d;
            capacity_ = n;
        //}
        return true;
    }

    template<PARTICLE_ARRAY_ENABLE_IF_TRIVIALLY_COPYABLE(T)>
    static void copy(T* dest, const T* p, const T* end) {
        ::memcpy(dest, p, (end - p) * sizeof(T));
    }

    template<PARTICLE_ARRAY_ENABLE_IF_NOT_TRIVIALLY_COPYABLE(T)>
    static void copy(T* dest, const T* p, const T* end) {
        while (p < end) {
            new(dest) T(*p);
            ++dest;
            ++p;
        }
    }

    template<typename IteratorT>
    static void copy(T* dest, IteratorT it, IteratorT end) {
        while (it != end) {
            new(dest) T(*it);
            ++dest;
            ++it;
        }
    }

    template<PARTICLE_ARRAY_ENABLE_IF_TRIVIALLY_COPYABLE(T)>
    static void move(T* dest, T* p, T* end) {
        ::memmove(dest, p, (end - p) * sizeof(T));
    }

    template<PARTICLE_ARRAY_ENABLE_IF_NOT_TRIVIALLY_COPYABLE(T)>
    static void move(T* dest, T* p, T* end) {
        if (dest > p && dest < end) {
            // Move elements in reverse order
            --p;
            --end;
            dest += end - p - 1;
            while (end > p) {
                new(dest) T(std::move(*end));
                end->~T();
                --dest;
                --end;
            }
        } else if (dest != p) {
            while (p < end) {
                new(dest) T(std::move(*p));
                p->~T();
                ++dest;
                ++p;
            }
        }
    }

    static T* find(T* p, T* end, const T& value) {
        while (p < end) {
            if (*p == value) {
                return p;
            }
            ++p;
        }
        return nullptr;
    }

    // FIXME: template<typename PointerT>
    static const T* find(const T* p, const T* end, const T& value) {
        while (p < end) {
            if (*p == value) {
                return p;
            }
            ++p;
        }
        return nullptr;
    }

    static const T* rfind(const T* p, const T* end, const T& value) {
        --p;
        --end;
        while (end > p) {
            if (*end == value) {
                return end;
            }
            --end;
        }
        return nullptr;
    }

    template<typename... ArgsT>
    static void construct(T* p, T* end, ArgsT... args) {
        while (p < end) {
            new(p) T(args...);
            ++p;
        }
    }

    static void destruct(T* p, T* end) {
        while (p < end) {
            p->~T();
            ++p;
        }
    }

    template<typename V, typename A>
    friend void swap(Array<V, A>& array, Array<V, A>& array2);
};

template<typename T, typename AllocatorT>
void swap(Array<T, AllocatorT>& array, Array<T, AllocatorT>& array2);

} // namespace spark

// spark::DefaultAllocator
inline void* spark::DefaultAllocator::malloc(size_t size) {
    return ::malloc(size);
}

inline void* spark::DefaultAllocator::realloc(void* ptr, size_t size) {
    return ::realloc(ptr, size);
}

inline void spark::DefaultAllocator::free(void* ptr) {
    ::free(ptr);
}

// spark::Array
template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::Array() :
        data_(nullptr),
        size_(0),
        capacity_(0) {
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::Array(int n) : Array() {
    if (realloc(n)) {
        construct(data_, data_ + n);
        size_ = n;
    }
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::Array(int n, const T& value) : Array() {
    if (realloc(n)) {
        construct(data_, data_ + n, value);
        size_ = n;
    }
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::Array(const T* values, int n) : Array() {
    if (realloc(n)) {
        copy(data_, values, values + n);
        size_ = n;
    }
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::Array(std::initializer_list<T> values) : Array() {
    if (realloc(values.size())) {
        copy(data_, values.begin(), values.end());
        size_ = values.size();
    }
}

template<typename T, typename AllocatorT>
template<typename IteratorT>
inline spark::Array<T, AllocatorT>::Array(IteratorT begin, IteratorT end) : Array() {
    const int n = std::distance(begin, end);
    if (realloc(n)) {
        copy(data_, begin, end);
        size_ = n;
    }
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::Array(const Array<T, AllocatorT>& array, int i, int n) : Array() {
    if (n < 0 || i + n > array.size_) {
        n = array.size_ - i;
    }
    if (realloc(n)) {
        const T* const p = array.data_ + i;
        copy(data_, p, p + n);
        size_ = n;
    }
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::Array(const Array<T, AllocatorT>& array) : Array() {
    if (realloc(array.size_)) {
        copy(data_, array.data_, array.data_ + array.size_);
        size_ = array.size_;
    }
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::Array(Array<T, AllocatorT>&& array) : Array() {
    swap(*this, array);
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>::~Array() {
    destruct(data_, data_ + size_);
    AllocatorT::free(data_);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::append(T value) {
    return insert(size_, value);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::append(int n, const T& value) {
    return insert(size_, n, value);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::append(const T* values, int n) {
    return insert(size_, values, n);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::append(const Array<T, AllocatorT> &array) {
    return insert(size_, array);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::append(const Array<T, AllocatorT> &array, int i, int n) {
    return insert(size_, array, i, n);
}

template<typename T, typename AllocatorT>
template<typename IteratorT>
inline bool spark::Array<T, AllocatorT>::append(IteratorT begin, IteratorT end) {
    return insert(size_, begin, end);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::prepend(T value) {
    return insert(0, value);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::prepend(int n, const T& value) {
    return insert(0, n, value);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::prepend(const T* values, int n) {
    return insert(0, values, n);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::prepend(const Array<T, AllocatorT> &array) {
    return insert(0, array);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::prepend(const Array<T, AllocatorT> &array, int i, int n) {
    return insert(0, array, i, n);
}

template<typename T, typename AllocatorT>
template<typename IteratorT>
inline bool spark::Array<T, AllocatorT>::prepend(IteratorT begin, IteratorT end) {
    return insert(0, begin, end);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::insert(int i, T value) {
    if (size_ + 1 > capacity_ && !realloc(size_ + 1)) {
        return false;
    }
    T* const p = data_ + i;
    move(p + 1, p, data_ + size_);
    new(p) T(std::move(value));
    ++size_;
    return true;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::insert(int i, int n, const T& value) {
    if (size_ + n > capacity_ && !realloc(size_ + n)) {
        return false;
    }
    T* const p = data_ + i;
    move(p + n, p, data_ + size_);
    construct(p, p + n, value);
    size_ += n;
    return true;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::insert(int i, const T* values, int n) {
    if (size_ + n > capacity_ && !realloc(size_ + n)) {
        return false;
    }
    T* const p = data_ + i;
    move(p + n, p, data_ + size_);
    copy(p, values, values + n);
    size_ += n;
    return true;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::insert(int i, const Array<T, AllocatorT> &array) {
    return insert(i, array.data_, array.size_);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::insert(int i, const Array<T, AllocatorT> &array, int i2, int n) {
    if (n < 0 || i2 + n > array.size_) {
        n = array.size_ - i2;
    }
    return insert(i, array.data_ + i2, n);
}

template<typename T, typename AllocatorT>
template<typename IteratorT>
inline bool spark::Array<T, AllocatorT>::insert(int i, IteratorT begin, IteratorT end) {
    const int n = std::distance(begin, end);
    if (size_ + n > capacity_ && !realloc(size_ + n)) {
        return false;
    }
    T* const p = data_ + i;
    move(p + n, p, data_ + size_);
    copy(p, begin, end);
    size_ += n;
    return true;
}

template<typename T, typename AllocatorT>
inline void spark::Array<T, AllocatorT>::remove(int i, int n) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    T* const p = data_ + i;
    destruct(p, p + n);
    move(p, p + n, data_ + size_);
    size_ -= n;
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::removeOne(const T &value) {
    return removeOne(value, 0, size_);
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::removeOne(const T &value, int i, int n) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    T* p = data_ + i;
    p = find(p, p + n, value);
    if (!p) {
        return 0;
    }
    p->~T();
    move(p, p + 1, data_ + size_);
    --size_;
    return 1;
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::removeAll(const T &value) {
    return removeAll(value, 0, size_);
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::removeAll(const T &value, int i, int n) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    int size = size_;
    T* p = data_ + i;
    while ((p = find(p, p + n, value))) {
        p->~T();
        move(p, p + 1, data_ + size);
        --size;
        --n;
        ++p;
    }
    n = size_ - size;
    size_ = size;
    return n;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::replace(int i, int n, const T& value, int n2) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    if (n2 > n && !realloc(size_ + n2 - n)) {
        return false;
    }
    T* const p = data_ + i;
    destruct(p, p + n);
    construct(p, p + n2, value);
    move(p + n2, p + n, data_ + size_);
    size_ += n2 - n;
    return true;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::replace(int i, int n, const T* values, int n2) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    if (n2 > n && !realloc(size_ + n2 - n)) {
        return false;
    }
    T* const p = data_ + i;
    destruct(p, p + n);
    copy(p, values, values + n2);
    move(p + n2, p + n, data_ + size_);
    size_ += n2 - n;
    return true;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::replace(int i, int n, const Array<T, AllocatorT>& array) {
    return replace(i, n, array.data_, array.size_);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::replace(int i, int n, const Array<T, AllocatorT>& array, int i2, int n2) {
    if (n2 < 0 || i2 + n2 > array.size_) {
        n2 = array.size_ - i2;
    }
    return replace(i, n, array.data_ + i2, n2);
}

template<typename T, typename AllocatorT>
template<typename IteratorT>
inline bool spark::Array<T, AllocatorT>::replace(int i, int n, IteratorT begin, IteratorT end) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    const int n2 = std::distance(begin, end);
    if (n2 > n && !realloc(size_ + n2 - n)) {
        return false;
    }
    T* const p = data_ + i;
    destruct(p, p + n);
    copy(p, begin, end);
    move(p + n2, p + n, data_ + size_);
    size_ += n2 - n;
    return true;
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::replaceOne(const T& value, T value2) {
    return replaceOne(value, std::move(value2), 0, size_);
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::replaceOne(const T& value, T value2, int i, int n) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    T* p = data_ + i;
    p = find(p, p + n, value);
    if (!p) {
        return 0;
    }
    p->~T();
    p->T(std::move(value2));
    return 1;
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::replaceAll(const T& value, const T& value2) {
    return replaceAll(value, value2, 0, size_);
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::replaceAll(const T& value, const T& value2, int i, int n) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    int count = 0;
    T* p = data_ + i;
    while ((p = find(p, p + n, value))) {
        p->~T();
        p->T(value2);
        ++count;
        ++p;
    }
    return count;
}

template<typename T, typename AllocatorT>
inline void spark::Array<T, AllocatorT>::fill(const T& value) {
    fill(0, size_, value);
}

template<typename T, typename AllocatorT>
inline void spark::Array<T, AllocatorT>::fill(int i, int n, const T& value) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    T* p = data_ + i;
    T* const end = p + n;
    while (p != end) {
        p->~T();
        p->T(value);
        ++p;
    }
}

template<typename T, typename AllocatorT>
inline T spark::Array<T, AllocatorT>::takeFirst() {
    return take(0);
}

template<typename T, typename AllocatorT>
inline T spark::Array<T, AllocatorT>::takeLast() {
    return take(size_ - 1);
}

template<typename T, typename AllocatorT>
inline T spark::Array<T, AllocatorT>::take(int i) {
    T* const p = data_ + i;
    const T val(std::move(*p));
    p->~T();
    move(p, p + 1, data_ + size_);
    --size_;
    return val;
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT> spark::Array<T, AllocatorT>::take(int i, int n) {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    Array<T, AllocatorT> array;
    if (array.realloc(n)) {
        T* const p = data_ + i;
        move(array.data_, p, p + n);
        move(p, p + n, data_ + size_);
        array.size_ = n;
        size_ -= n;
    }
    return array;
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT> spark::Array<T, AllocatorT>::copy(int i, int n) const {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    Array<T, AllocatorT> array;
    if (array.realloc(n)) {
        T* const p = data_ + i;
        copy(array.data_, p, p + n);
        array.size_ = n;
    }
    return array;
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::indexOf(const T &value) const {
    return indexOf(value, 0, size_);
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::indexOf(const T &value, int i, int n) const {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    const T* p = data_ + i;
    p = find(p, p + n, value);
    if (!p) {
        return -1;
    }
    return p - data_;
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::lastIndexOf(const T &value) const {
    return lastIndexOf(value, 0, size_);
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::lastIndexOf(const T &value, int i, int n) const {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    const T* p = data_ + i;
    p = rfind(p, p + n, value);
    if (!p) {
        return -1;
    }
    return p - data_;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::contains(const T &value) const {
    return contains(value, 0, size_);
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::contains(const T &value, int i, int n) const {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    const T* const p = data_ + i;
    return find(p, p + n, value);
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::count(const T &value) const {
    return count(value, 0, size_);
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::count(const T &value, int i, int n) const {
    if (n < 0 || i + n > size_) {
        n = size_ - i;
    }
    int count = 0;
    const T* p = data_ + i;
    const T* const end = p + n;
    while ((p = find(p, end, value))) {
        ++count;
        ++p;
    }
    return count;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::resize(int n) {
    if (n > size_) {
        if (n > capacity_ && !realloc(n)) {
            return false;
        }
        construct(data_ + size_, data_ + n);
    } else {
        destruct(data_ + n, data_ + size_);
    }
    size_ = n;
    return true;
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::size() const {
    return size_;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::isEmpty() const {
    return size_ == 0;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::reserve(int n) {
    if (n > capacity_ && !realloc(n)) {
        return false;
    }
    return true;
}

template<typename T, typename AllocatorT>
inline int spark::Array<T, AllocatorT>::capacity() const {
    return capacity_;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::trim() {
    if (capacity_ > size_ && !realloc(size_)) {
        return false;
    }
    return true;
}

template<typename T, typename AllocatorT>
inline void spark::Array<T, AllocatorT>::clear() {
    destruct(data_, data_ + size_);
    size_ = 0;
}

template<typename T, typename AllocatorT>
inline T* spark::Array<T, AllocatorT>::data() {
    return data_;
}

template<typename T, typename AllocatorT>
inline const T* spark::Array<T, AllocatorT>::data() const {
    return data_;
}

template<typename T, typename AllocatorT>
inline T* spark::Array<T, AllocatorT>::begin() {
    return data_;
}

template<typename T, typename AllocatorT>
const T* spark::Array<T, AllocatorT>::begin() const {
    return data_;
}

template<typename T, typename AllocatorT>
T* spark::Array<T, AllocatorT>::end() {
    return data_ + size_;
}

template<typename T, typename AllocatorT>
const T* spark::Array<T, AllocatorT>::end() const {
    return data_ + size_;
}

template<typename T, typename AllocatorT>
inline T& spark::Array<T, AllocatorT>::first() {
    return data_[0];
}

template<typename T, typename AllocatorT>
inline const T& spark::Array<T, AllocatorT>::first() const {
    return data_[0];
}

template<typename T, typename AllocatorT>
inline T& spark::Array<T, AllocatorT>::last() {
    return data_[size_ - 1];
}

template<typename T, typename AllocatorT>
inline const T& spark::Array<T, AllocatorT>::last() const {
    return data_[size_ - 1];
}

template<typename T, typename AllocatorT>
inline T& spark::Array<T, AllocatorT>::at(int i) {
    return data_[i];
}

template<typename T, typename AllocatorT>
inline const T& spark::Array<T, AllocatorT>::at(int i) const {
    return data_[i];
}

template<typename T, typename AllocatorT>
inline T& spark::Array<T, AllocatorT>::operator[](int i) {
    return data_[i];
}

template<typename T, typename AllocatorT>
inline const T& spark::Array<T, AllocatorT>::operator[](int i) const {
    return data_[i];
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::operator==(const Array<T, AllocatorT> &array) const {
    if (size_ != array.size_) {
        return false;
    }
    const T* p = data_;
    const T* p2 = array.data_;
    const T* const end = p + size_;
    while (p != end) {
        if (*p != *p2) {
            return false;
        }
        ++p2;
        ++p;
    }
    return true;
}

template<typename T, typename AllocatorT>
inline bool spark::Array<T, AllocatorT>::operator!=(const Array<T, AllocatorT> &array) const {
    return !(*this == array);
}

template<typename T, typename AllocatorT>
inline spark::Array<T, AllocatorT>& spark::Array<T, AllocatorT>::operator=(Array<T, AllocatorT> array) {
    swap(*this, array);
    return *this;
}

// spark::*
template<typename T, typename AllocatorT>
inline void spark::swap(Array<T, AllocatorT>& array, Array<T, AllocatorT>& array2) {
    using std::swap;
    swap(array.data_, array2.data_);
    swap(array.size_, array2.size_);
    swap(array.capacity_, array2.capacity_);
}
/*
#pragma GCC diagnostic pop
*/
#endif // SPARK_WIRING_ARRAY_H
