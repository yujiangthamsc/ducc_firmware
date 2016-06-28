#define CATCH_CONFIG_PREFIX_ALL
#include "catch.hpp"

#include <unordered_map>
#include <vector>
#include <random>

#include "spark_wiring_array.h"

#define CHECK(...) \
        CATCH_CHECK(__VA_ARGS__)

#define REQUIRE(...) \
        CATCH_REQUIRE(__VA_ARGS__)

namespace {

class TestAllocator {
public:
    class Checker {
    public:
        Checker() {
            TestAllocator::instance()->reset();
        }

        void check() {
            TestAllocator::instance()->check();
        }
    };

    void* malloc(size_t size) {
        const Buffer b = newBuffer(size);
        void* const ptr = &b->front() + PADDING_SIZE;
        alloc_.insert(std::make_pair(ptr, b));
        return ptr;
    }

    void* calloc(size_t count, size_t size) {
        size *= count;
        void* const ptr = malloc(size);
        if (ptr) {
            ::memset(ptr, 0, size);
        }
        return ptr;
    }

    void* realloc(void* ptr, size_t size) {
        try {
            Buffer b; // Original buffer
            if (ptr) {
                auto it = alloc_.find(ptr);
                if (it == alloc_.end()) {
                    throw std::runtime_error("Invalid realloc() detected");
                }
                b = it->second;
            }
            void* const p = malloc(size);
            if (p) {
                free(ptr);
                if (b) {
                    // Copy user data from the original buffer
                    ::memcpy(p, b->data() + PADDING_SIZE, std::min(size, b->size() - PADDING_SIZE * 2));
                }
            }
            return p;
        } catch (const std::exception& e) {
            // realloc() and free() can be called in destructor, so we can't use CATCH_FAIL() here,
            // as it throws exceptions
            CATCH_WARN("TestAllocator: " << e.what());
            failed_ = true;
            return nullptr;
        }
    }

    void free(void* ptr) {
        try {
            if (!ptr) {
                return;
            }
            if (free_.count(ptr)) {
                throw std::runtime_error("Double free() detected");
            }
            auto it = alloc_.find(ptr);
            if (it == alloc_.end()) {
                throw std::runtime_error("Invalid free() detected");
            }
            const Buffer b = it->second; // Original buffer
            alloc_.erase(it);
            FreeBuffer f;
            f.buffer = b;
            f.data = b->substr(PADDING_SIZE, b->size() - PADDING_SIZE * 2); // Current user data
            free_.insert(std::make_pair(ptr, f));
            if (!checkPadding(b)) {
                throw std::runtime_error("Buffer overflow detected");
            }
        } catch (const std::exception& e) {
            CATCH_WARN("TestAllocator: " << e.what());
            failed_ = true;
        }
    }

    void reset() {
        alloc_.clear();
        free_.clear();
        randomFill(&padding_.front(), PADDING_SIZE); // Generate new padding data
        failed_ = false;
    }

    void check() {
        // Check all previously allocated buffers
        if (!alloc_.empty()) {
            CATCH_FAIL("TestAllocator: Memory leak detected");
        }
        for (auto it = free_.begin(); it != free_.end(); ++it) {
            const FreeBuffer& f = it->second;
            if (!checkData(f.buffer, f.data) || !checkPadding(f.buffer)) {
                CATCH_FAIL("TestAllocator: Memory corruption detected");
            }
        }
        if (failed_) {
            CATCH_FAIL("TestAllocator: Memory check failed");
        }
    }

    static TestAllocator* instance() {
        static TestAllocator alloc;
        return &alloc;
    }

    // This class is non-copyable
    TestAllocator(const TestAllocator&) = delete;
    TestAllocator& operator=(const TestAllocator&) = delete;

private:
    typedef std::shared_ptr<std::string> Buffer;

    struct FreeBuffer {
        Buffer buffer;
        std::string data; // Original data
    };

    std::unordered_map<void*, Buffer> alloc_;
    std::unordered_map<void*, FreeBuffer> free_;
    std::string padding_;
    bool failed_;

    static const size_t PADDING_SIZE = 128;

    TestAllocator() :
            padding_(PADDING_SIZE, 0),
            failed_(false) {
        reset();
    }

    Buffer newBuffer(size_t size) const {
        const Buffer b(new std::string(size + PADDING_SIZE * 2, 0));
        // Initialize padding
        ::memcpy(&b->front(), padding_.data(), PADDING_SIZE);
        ::memcpy(&b->front() + PADDING_SIZE + size, padding_.data(), PADDING_SIZE);
        // Fill user data with random bytes
        randomFill(&b->front() + PADDING_SIZE, size);
        return b;
    }

    bool checkData(const Buffer& b, const std::string& data) {
        const size_t size = b->size() - PADDING_SIZE * 2;
        return ::memcmp(b->data() + PADDING_SIZE, data.data(), size) == 0;
    }

    bool checkPadding(const Buffer& b) const {
        const size_t size = b->size() - PADDING_SIZE * 2;
        return ::memcmp(b->data(), padding_.data(), PADDING_SIZE) == 0 &&
                ::memcmp(b->data() + PADDING_SIZE + size, padding_.data(), PADDING_SIZE) == 0;
    }

    static void randomFill(char* data, size_t size) {
        static std::random_device rnd;
        static std::uniform_int_distribution<unsigned> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            data[i] = (char)dist(rnd);
        }
    }
};

struct TestArrayAllocator {
    static void* malloc(size_t size) {
        return TestAllocator::instance()->malloc(size);
    }

    static void* realloc(void* ptr, size_t size) {
        return TestAllocator::instance()->realloc(ptr, size);
    }

    static void free(void* ptr) {
        TestAllocator::instance()->free(ptr);
    }
};

template<typename T>
class Array: public spark::Array<T, TestArrayAllocator> {
public:
    using spark::Array<T, TestArrayAllocator>::Array;

    template<typename... ArgsT>
    void checkValues(ArgsT... args) const {
        checkValues(0, *this, args...);
    }

    void checkSize(int size, int capacity = -1) {
        checkSize(*this, size, capacity);
    }

private:
    template<typename... ArgsT>
    static void checkValues(int index, const Array<T>& array, const T& value, ArgsT... args) {
        checkValues(index, array, value);
        checkValues(index + 1, array, args...);
    }

    static void checkValues(int index, const Array<T>& array, const T& value) {
        REQUIRE(index < array.size());
        REQUIRE(array.at(index) == value);
    }

    static void checkValues(int size, const Array<T>& array) {
        REQUIRE(array.size() == size);
    }

    static void checkSize(const Array<T>& array, int size, int capacity = -1) {
        REQUIRE(array.size() == size);
        if (size > 0) {
            REQUIRE(!array.isEmpty());
        } else {
            REQUIRE(array.isEmpty());
        }
        if (capacity < 0) {
            capacity = size;
        }
        REQUIRE(array.capacity() == capacity);
        REQUIRE(array.capacity() >= array.size());
        if (capacity > 0) {
            REQUIRE(array.data() != nullptr);
        } else {
            REQUIRE(array.data() == nullptr);
        }
    }
};

} // namespace

CATCH_TEST_CASE("Array<int>") {
    typedef ::Array<int> Array;

    TestAllocator::Checker alloc;

    CATCH_SECTION("construct") {
        CATCH_SECTION("Array()") {
            Array a;
            a.checkSize(0);
        }
        CATCH_SECTION("Array(int n)") {
            Array a(3); // n = 3
            a.checkSize(3);
            a.checkValues(0, 0, 0);
            Array a2(0); // n = 0
            a2.checkSize(0);
        }
        CATCH_SECTION("Array(int n, const T& value)") {
            Array a(3, 1); // n = 3
            a.checkSize(3);
            a.checkValues(1, 1, 1);
            Array a2(0, 1); // n = 0
            a2.checkSize(0);
        }
        CATCH_SECTION("Array(const T* values, int n)") {
            int v[] = { 1, 2, 3 };
            Array a(v, 3); // n = 3
            a.checkSize(3);
            a.checkValues(1, 2, 3);
            Array a2(v, 0); // n = 0
            a2.checkSize(0);
        }
        CATCH_SECTION("Array(std::initializer_list<T> values)") {
            Array a({ 1, 2, 3 });
            a.checkSize(3);
            a.checkValues(1, 2, 3);
            Array a2({}); // n = 0
            a2.checkSize(0);
        }
        CATCH_SECTION("Array(IteratorT begin, IteratorT end)") {
            std::vector<int> v = { 1, 2, 3 };
            Array a(v.begin(), v.end());
            a.checkSize(3);
            a.checkValues(1, 2, 3);
            Array a2(v.begin(), v.begin()); // n = 0
            a2.checkSize(0);
        }
        CATCH_SECTION("Array(const Array<T>& array, int i, int n)") {
            CATCH_SECTION("i = 0") {
                Array a({ 1, 2, 3 });
                Array a2(a, 0, 0); // n = 0
                a2.checkSize(0);
                Array a3(a, 0, 1); // n = 1
                a3.checkSize(1);
                a3.checkValues(1);
                Array a4(a, 0, 2); // n = 2
                a4.checkSize(2);
                a4.checkValues(1, 2);
            }
            CATCH_SECTION("i = array.size() / 2") {
                Array a({ 1, 2, 3 });
                Array a2(a, 1, 0); // n = 0
                a2.checkSize(0);
                Array a3(a, 1, 1); // n = 1
                a3.checkSize(1);
                a3.checkValues(2);
                Array a4(a, 1, 2); // n = 2
                a4.checkSize(2);
                a4.checkValues(2, 3);
            }
            CATCH_SECTION("i = array.size()") {
                Array a({ 1, 2, 3 });
                Array a2(a, 3, 0); // n = 0
                a2.checkSize(0);
                Array a3(a, 3, 1); // n = 1
                a3.checkSize(0);
                Array a4(a, 3, 2); // n = 2
                a4.checkSize(0);
            }
            CATCH_SECTION("n = -1") {
                Array a({ 1, 2, 3 });
                Array a2(a, 0, -1); // i = 0
                a2.checkSize(3);
                a2.checkValues(1, 2, 3);
                Array a3(a, 1, -1); // i = array.size() / 2
                a3.checkSize(2);
                a3.checkValues(2, 3);
                Array a4(a, 3, -1); // i = array.size()
                a4.checkSize(0);
            }
            CATCH_SECTION("array.size() == 0") {
                Array a;
                Array a2(a, 0, 0); // i = 0, n = 0
                a2.checkSize(0);
                Array a3(a, 0, 1); // ..., n = 1
                a3.checkSize(0);
                Array a4(a, 0, 2); // ..., n = 2
                a4.checkSize(0);
                Array a5(a, 0, -1); // ..., n = -1
                a5.checkSize(0);
            }
        }
        CATCH_SECTION("Array(const Array<T>& array)") {
            Array a({ 1, 2, 3 });
            Array a2(a);
            a2.checkSize(3);
            a2.checkValues(1, 2, 3);
            a.checkSize(3); // source data is not affected
            a.checkValues(1, 2, 3);
            Array a3;
            Array a4(a3); // copy empty array
            a4.checkSize(0);
            a3.checkSize(0);
        }
        CATCH_SECTION("Array(Array<T>&& array)") {
            Array a({ 1, 2, 3 });
            Array a2(std::move(a));
            a2.checkSize(3);
            a2.checkValues(1, 2, 3);
            a.checkSize(0); // source data has been moved
            Array a3;
            Array a4(std::move(a3)); // move empty array
            a4.checkSize(0);
            a3.checkSize(0);
        }
    }

    CATCH_SECTION("append()") {
        CATCH_SECTION("append(T value)") {
            Array a({ 1, 2, 3 });
            REQUIRE(a.append(4));
            a.checkSize(4);
            a.checkValues(1, 2, 3, 4);
            Array a2;
            REQUIRE(a2.append(1)); // append to empty array
            a2.checkSize(1);
            a2.checkValues(1);
        }
        CATCH_SECTION("append(int n, const T& value)") {
            Array a({ 1, 2, 3 });
            REQUIRE(a.append(0, 4)); // n = 0
            a.checkSize(3);
            a.checkValues(1, 2, 3);
            REQUIRE(a.append(1, 4)); // n = 1
            a.checkSize(4);
            a.checkValues(1, 2, 3, 4);
            REQUIRE(a.append(2, 5)); // n = 2
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 5);
            Array a2;
            REQUIRE(a2.append(3, 1)); // append to empty array
            a2.checkSize(3);
            a2.checkValues(1, 1, 1);
        }
        CATCH_SECTION("append(const T* values, int n)") {
            int v[] = { 4 };
            int v2[] = { 5, 6 };
            Array a({ 1, 2, 3 });
            REQUIRE(a.append(v, 0)); // n = 0
            a.checkSize(3);
            a.checkValues(1, 2, 3);
            REQUIRE(a.append(v, 1)); // n = 1
            a.checkSize(4);
            a.checkValues(1, 2, 3, 4);
            REQUIRE(a.append(v2, 2)); // n = 2
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            Array a2;
            REQUIRE(a2.append(v2, 2)); // append to empty array
            a2.checkSize(2);
            a2.checkValues(5, 6);
        }
        CATCH_SECTION("append(const Array<T>& array)") {
            Array a({ 1, 2, 3 });
            Array a2({ 4, 5, 6 });
            Array a3;
            REQUIRE(a.append(a2));
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            REQUIRE(a.append(a3)); // append empty array
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            REQUIRE(a3.append(a)); // append to empty array
            a3.checkSize(6);
            a3.checkValues(1, 2, 3, 4, 5, 6);
        }
        CATCH_SECTION("append(IteratorT begin, IteratorT end)") {
            std::vector<int> v = { 4 };
            std::vector<int> v2 = { 5, 6 };
            Array a({ 1, 2, 3 });
            REQUIRE(a.append(v.begin(), v.begin())); // n = 0
            a.checkSize(3);
            a.checkValues(1, 2, 3);
            REQUIRE(a.append(v.begin(), v.end())); // n = 1
            a.checkSize(4);
            a.checkValues(1, 2, 3, 4);
            REQUIRE(a.append(v2.begin(), v2.end())); // n = 2
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            Array a2;
            REQUIRE(a2.append(v2.begin(), v2.end())); // append to empty array
            a2.checkSize(2);
            a2.checkValues(5, 6);
        }
    }

    CATCH_SECTION("prepend()") {
        CATCH_SECTION("prepend(T value)") {
            Array a({ 2, 3, 4 });
            REQUIRE(a.prepend(1));
            a.checkSize(4);
            a.checkValues(1, 2, 3, 4);
            Array a2;
            REQUIRE(a2.prepend(1)); // prepend to empty array
            a2.checkSize(1);
            a2.checkValues(1);
        }
        CATCH_SECTION("prepend(int n, const T& value)") {
            Array a({ 3, 4, 5 });
            REQUIRE(a.prepend(0, 2)); // n = 0
            a.checkSize(3);
            a.checkValues(3, 4, 5);
            REQUIRE(a.prepend(1, 2)); // n = 1
            a.checkSize(4);
            a.checkValues(2, 3, 4, 5);
            REQUIRE(a.prepend(2, 1)); // n = 2
            a.checkSize(6);
            a.checkValues(1, 1, 2, 3, 4, 5);
            Array a2;
            REQUIRE(a2.prepend(3, 1)); // prepend to empty array
            a2.checkSize(3);
            a2.checkValues(1, 1, 1);
        }
        CATCH_SECTION("prepend(const T* values, int n)") {
            int v[] = { 1, 2 };
            int v2[] = { 3 };
            Array a({ 4, 5, 6 });
            REQUIRE(a.prepend(v2, 0)); // n = 0
            a.checkSize(3);
            a.checkValues(4, 5, 6);
            REQUIRE(a.prepend(v2, 1)); // n = 1
            a.checkSize(4);
            a.checkValues(3, 4, 5, 6);
            REQUIRE(a.prepend(v, 2)); // n = 2
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            Array a2;
            REQUIRE(a2.prepend(v, 2)); // prepend to empty array
            a2.checkSize(2);
            a2.checkValues(1, 2);
        }
        CATCH_SECTION("prepend(IteratorT begin, IteratorT end)") {
            std::vector<int> v = { 1, 2 };
            std::vector<int> v2 = { 3 };
            Array a({ 4, 5, 6 });
            REQUIRE(a.prepend(v2.begin(), v2.begin())); // n = 0
            a.checkSize(3);
            a.checkValues(4, 5, 6);
            REQUIRE(a.prepend(v2.begin(), v2.end())); // n = 1
            a.checkSize(4);
            a.checkValues(3, 4, 5, 6);
            REQUIRE(a.prepend(v.begin(), v.end())); // n = 2
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            Array a2;
            REQUIRE(a2.prepend(v.begin(), v.end())); // prepend to empty array
            a2.checkSize(2);
            a2.checkValues(1, 2);
        }
        CATCH_SECTION("prepend(const Array<T>& array)") {
            Array a({ 4, 5, 6 });
            Array a2({ 1, 2, 3 });
            Array a3;
            REQUIRE(a.prepend(a2));
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            REQUIRE(a.prepend(a3)); // prepend empty array
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            REQUIRE(a3.prepend(a)); // prepend to empty array
            a3.checkSize(6);
            a3.checkValues(1, 2, 3, 4, 5, 6);
        }
    }

    CATCH_SECTION("insert()") {
        CATCH_SECTION("insert(int i, T value)") {
            Array a({ 2, 4, 5 });
            REQUIRE(a.insert(0, 1)); // i = 0
            a.checkSize(4);
            a.checkValues(1, 2, 4, 5);
            REQUIRE(a.insert(4, 6)); // i = size()
            a.checkSize(5);
            a.checkValues(1, 2, 4, 5, 6);
            REQUIRE(a.insert(2, 3)); // i = size() / 2
            a.checkSize(6);
            a.checkValues(1, 2, 3, 4, 5, 6);
            Array a2;
            REQUIRE(a2.insert(0, 1)); // insert to empty array
            a2.checkSize(1);
            a2.checkValues(1);
        }
        CATCH_SECTION("insert(int i, int n, const T& value)") {
            CATCH_SECTION("i = 0") {
                Array a({ 3, 4, 5 });
                REQUIRE(a.insert(0, 0, 2)); // n = 0
                a.checkSize(3);
                a.checkValues(3, 4, 5);
                REQUIRE(a.insert(0, 1, 2)); // n = 1
                a.checkSize(4);
                a.checkValues(2, 3, 4, 5);
                REQUIRE(a.insert(0, 2, 1)); // n = 2
                a.checkSize(6);
                a.checkValues(1, 1, 2, 3, 4, 5);
            }
            CATCH_SECTION("i = size()") {
                Array a({ 1, 2, 3 });
                REQUIRE(a.insert(3, 0, 4)); // n = 0
                a.checkSize(3);
                a.checkValues(1, 2, 3);
                REQUIRE(a.insert(3, 1, 4)); // n = 1
                a.checkSize(4);
                a.checkValues(1, 2, 3, 4);
                REQUIRE(a.insert(4, 2, 5)); // n = 2
                a.checkSize(6);
                a.checkValues(1, 2, 3, 4, 5, 5);
            }
            CATCH_SECTION("i = size() / 2") {
                Array a({ 1, 4, 5 });
                REQUIRE(a.insert(1, 0, 2)); // n = 0
                a.checkSize(3);
                a.checkValues(1, 4, 5);
                REQUIRE(a.insert(1, 1, 2)); // n = 1
                a.checkSize(4);
                a.checkValues(1, 2, 4, 5);
                REQUIRE(a.insert(2, 2, 3)); // n = 2
                a.checkSize(6);
                a.checkValues(1, 2, 3, 3, 4, 5);
            }
            CATCH_SECTION("misc") {
                Array a;
                REQUIRE(a.insert(0, 3, 1)); // insert to empty array
                a.checkSize(3);
                a.checkValues(1, 1, 1);
            }
        }
        CATCH_SECTION("insert(int i, const T* value, int n)") {
            CATCH_SECTION("i = 0") {
                int v[] = { 1, 2 };
                int v2[] = { 3 };
                Array a({ 4, 5, 6 });
                REQUIRE(a.insert(0, v2, 0)); // n = 0
                a.checkSize(3);
                a.checkValues(4, 5, 6);
                REQUIRE(a.insert(0, v2, 1)); // n = 1
                a.checkSize(4);
                a.checkValues(3, 4, 5, 6);
                REQUIRE(a.insert(0, v, 2)); // n = 2
                a.checkSize(6);
                a.checkValues(1, 2, 3, 4, 5, 6);
            }
            CATCH_SECTION("i = size()") {
                int v[] = { 4 };
                int v2[] = { 5, 6 };
                Array a({ 1, 2, 3 });
                REQUIRE(a.insert(3, v, 0)); // n = 0
                a.checkSize(3);
                a.checkValues(1, 2, 3);
                REQUIRE(a.insert(3, v, 1)); // n = 1
                a.checkSize(4);
                a.checkValues(1, 2, 3, 4);
                REQUIRE(a.insert(4, v2, 2)); // n = 2
                a.checkSize(6);
                a.checkValues(1, 2, 3, 4, 5, 6);
            }
            CATCH_SECTION("i = size() / 2") {
                int v[] = { 2 };
                int v2[] = { 3, 4 };
                Array a({ 1, 5, 6 });
                REQUIRE(a.insert(1, v, 0)); // n = 0
                a.checkSize(3);
                a.checkValues(1, 5, 6);
                REQUIRE(a.insert(1, v, 1)); // n = 1
                a.checkSize(4);
                a.checkValues(1, 2, 5, 6);
                REQUIRE(a.insert(2, v2, 2)); // n = 2
                a.checkSize(6);
                a.checkValues(1, 2, 3, 4, 5, 6);
            }
            CATCH_SECTION("size() == 0") {
                int v[] = { 1, 2, 3 };
                Array a;
                REQUIRE(a.insert(0, v, 3)); // insert to empty array
                a.checkSize(3);
                a.checkValues(1, 2, 3);
            }
        }
        CATCH_SECTION("insert(int i, IteratorT begin, IteratorT end)") {
            CATCH_SECTION("i = 0") {
                std::vector<int> v = { 1, 2 };
                std::vector<int> v2 = { 3 };
                Array a({ 4, 5, 6 });
                REQUIRE(a.insert(0, v2.begin(), v2.begin())); // n = 0
                a.checkSize(3);
                a.checkValues(4, 5, 6);
                REQUIRE(a.insert(0, v2.begin(), v2.end())); // n = 1
                a.checkSize(4);
                a.checkValues(3, 4, 5, 6);
                REQUIRE(a.insert(0, v.begin(), v.end())); // n = 2
                a.checkSize(6);
                a.checkValues(1, 2, 3, 4, 5, 6);
            }
            CATCH_SECTION("i = size()") {
                std::vector<int> v = { 4 };
                std::vector<int> v2 = { 5, 6 };
                Array a({ 1, 2, 3 });
                REQUIRE(a.insert(3, v.begin(), v.begin())); // n = 0
                a.checkSize(3);
                a.checkValues(1, 2, 3);
                REQUIRE(a.insert(3, v.begin(), v.end())); // n = 1
                a.checkSize(4);
                a.checkValues(1, 2, 3, 4);
                REQUIRE(a.insert(4, v2.begin(), v2.end())); // n = 2
                a.checkSize(6);
                a.checkValues(1, 2, 3, 4, 5, 6);
            }
            CATCH_SECTION("i = size() / 2") {
                std::vector<int> v = { 2 };
                std::vector<int> v2 = { 3, 4 };
                Array a({ 1, 5, 6 });
                REQUIRE(a.insert(1, v.begin(), v.begin())); // n = 0
                a.checkSize(3);
                a.checkValues(1, 5, 6);
                REQUIRE(a.insert(1, v.begin(), v.end())); // n = 1
                a.checkSize(4);
                a.checkValues(1, 2, 5, 6);
                REQUIRE(a.insert(2, v2.begin(), v2.end())); // n = 2
                a.checkSize(6);
                a.checkValues(1, 2, 3, 4, 5, 6);
            }
            CATCH_SECTION("size() == 0") {
                std::vector<int> v = { 1, 2, 3 };
                Array a;
                REQUIRE(a.insert(0, v.begin(), v.end())); // insert to empty array
                a.checkSize(3);
                a.checkValues(1, 2, 3);
            }
        }
    }

    alloc.check(); // Check memory allocations
}
