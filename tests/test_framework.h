#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <sstream>
#include <type_traits>
#include <any>
#include <memory>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <stdexcept>
#include <filesystem>
#include <queue>
#include <deque>

namespace TestFramework {

template <typename A, typename B>
inline bool assert_eq_values(const A& a, const B& b) {
    if constexpr (std::is_arithmetic_v<A> && std::is_arithmetic_v<B>) {
        using Common = std::common_type_t<A, B>;
        return static_cast<Common>(a) == static_cast<Common>(b);
    }
    return a == b;
}

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

class Test {
public:
    virtual ~Test() = default;
    virtual void SetUp() {}
    virtual void TearDown() {}
};

class TestSuite {
public:
    static TestSuite& instance() {
        static TestSuite s;
        return s;
    }

    void add_test(const std::string& name, std::function<void()> fn) {
        tests_.push_back({name, std::move(fn)});
    }

    int run() {
        int passed = 0, failed = 0;
        std::cout << "\n========================================" << std::endl;
        std::cout << "  AMPLITRON TEST SUITE" << std::endl;
        std::cout << "========================================\n" << std::endl;

        for (auto& [name, fn] : tests_) {
            current_test_ = name;
            current_failed_ = false;
            try {
                fn();
            } catch (const std::exception& e) {
                current_failed_ = true;
                results_.push_back({name, false, std::string("EXCEPTION: ") + e.what()});
            }
            if (!current_failed_) {
                results_.push_back({name, true, ""});
                ++passed;
                std::cout << "  PASS  " << name << std::endl;
            } else {
                ++failed;
            }
        }

        std::cout << "\n----------------------------------------" << std::endl;
        std::cout << "  Results: " << passed << " passed, " << failed << " failed, "
                  << (passed + failed) << " total" << std::endl;
        std::cout << "----------------------------------------\n" << std::endl;

        if (failed > 0) {
            std::cout << "  FAILURES:" << std::endl;
            for (auto& r : results_) {
                if (!r.passed) {
                    std::cout << "    FAIL  " << r.name << std::endl;
                    std::cout << "          " << r.message << std::endl;
                }
            }
            std::cout << std::endl;
        }

        return failed;
    }

    void fail(const std::string& msg) {
        if (!current_failed_) {
            current_failed_ = true;
            results_.push_back({current_test_, false, msg});
            std::cout << "  FAIL  " << current_test_ << std::endl;
            std::cout << "        " << msg << std::endl;
        }
    }

private:
    std::vector<std::pair<std::string, std::function<void()>>> tests_;
    std::vector<TestResult> results_;
    std::string current_test_;
    bool current_failed_ = false;
};

// Macros
#define TEST_SINGLE(name) \
    static void test_##name(); \
    namespace { struct Register_##name { \
        Register_##name() { TestFramework::TestSuite::instance().add_test(#name, test_##name); } \
    } reg_##name; } \
    static void test_##name()

#define TEST_SUITE_CASE(suite, name) \
    static void test_##suite##_##name(); \
    namespace { struct Register_##suite##_##name { \
        Register_##suite##_##name() { TestFramework::TestSuite::instance().add_test(#suite "." #name, test_##suite##_##name); } \
    } reg_##suite##_##name; } \
    static void test_##suite##_##name()

#define TEST_EXPAND(x) x
#define TEST_GET_MACRO(_1, _2, NAME, ...) NAME
#define TEST(...) TEST_EXPAND(TEST_GET_MACRO(__VA_ARGS__, TEST_SUITE_CASE, TEST_SINGLE)(__VA_ARGS__))

#define TEST_F(FixtureName, TestName) \
    class FixtureName##_##TestName : public FixtureName { \
    public: \
        void RunTest(); \
    }; \
    static void run_fixture_test_##FixtureName##_##TestName() { \
        FixtureName##_##TestName t; \
        t.SetUp(); \
        try { \
            t.RunTest(); \
        } catch (...) { \
            t.TearDown(); \
            throw; \
        } \
        t.TearDown(); \
    } \
    namespace { struct Register_##FixtureName##_##TestName { \
        Register_##FixtureName##_##TestName() { \
            TestFramework::TestSuite::instance().add_test(#FixtureName "_" #TestName, run_fixture_test_##FixtureName##_##TestName); \
        } \
    } reg_##FixtureName##_##TestName; } \
    void FixtureName##_##TestName::RunTest()

#define ASSERT_TRUE(expr) \
    do { if (!(expr)) { \
        std::ostringstream ss; ss << "ASSERT_TRUE failed: " #expr " (line " << __LINE__ << ")"; \
        TestFramework::TestSuite::instance().fail(ss.str()); return; \
    }} while(0)

#define ASSERT_FALSE(expr) \
    do { if ((expr)) { \
        std::ostringstream ss; ss << "ASSERT_FALSE failed: " #expr " (line " << __LINE__ << ")"; \
        TestFramework::TestSuite::instance().fail(ss.str()); return; \
    }} while(0)

#define ASSERT_EQ(a, b) \
    do { auto _a = (a); auto _b = (b); if (!TestFramework::assert_eq_values(_a, _b)) { \
        std::ostringstream ss; ss << "ASSERT_EQ failed: " #a " == " #b " (" << _a << " != " << _b << ") (line " << __LINE__ << ")"; \
        TestFramework::TestSuite::instance().fail(ss.str()); return; \
    }} while(0)

#define ASSERT_NE(a, b) \
    do { auto _a = (a); auto _b = (b); if (_a == _b) { \
        std::ostringstream ss; ss << "ASSERT_NE failed: " #a " != " #b " (" << _a << ") (line " << __LINE__ << ")"; \
        TestFramework::TestSuite::instance().fail(ss.str()); return; \
    }} while(0)

#define ASSERT_NEAR(a, b, eps) \
    do { auto _a = (a); auto _b = (b); if (std::fabs(_a - _b) > (eps)) { \
        std::ostringstream ss; ss << "ASSERT_NEAR failed: |" #a " - " #b "| <= " #eps " (" << _a << " vs " << _b << ", diff=" << std::fabs(_a - _b) << ") (line " << __LINE__ << ")"; \
        TestFramework::TestSuite::instance().fail(ss.str()); return; \
    }} while(0)

#define ASSERT_GT(a, b) \
    do { auto _a = (a); auto _b = (b); if (!(_a > _b)) { \
        std::ostringstream ss; ss << "ASSERT_GT failed: " #a " > " #b " (" << _a << " <= " << _b << ") (line " << __LINE__ << ")"; \
        TestFramework::TestSuite::instance().fail(ss.str()); return; \
    }} while(0)

#define ASSERT_LT(a, b) \
    do { auto _a = (a); auto _b = (b); if (!(_a < _b)) { \
        std::ostringstream ss; ss << "ASSERT_LT failed: " #a " < " #b " (" << _a << " >= " << _b << ") (line " << __LINE__ << ")"; \
        TestFramework::TestSuite::instance().fail(ss.str()); return; \
    }} while(0)

#define ASSERT_GE(a, b) \
    do { auto _a = (a); auto _b = (b); if (!(_a >= _b)) { \
        std::ostringstream ss; ss << "ASSERT_GE failed: " #a " >= " #b " (" << _a << " < " << _b << ") (line " << __LINE__ << ")"; \
        TestFramework::TestSuite::instance().fail(ss.str()); return; \
    }} while(0)

#define ASSERT_THROW(expr, ExceptionType) \
    do { \
        bool caught = false; \
        try { \
            (expr); \
        } catch (const ExceptionType&) { \
            caught = true; \
        } catch (...) {} \
        if (!caught) { \
            std::ostringstream ss; ss << "ASSERT_THROW failed: " #expr " did not throw " #ExceptionType " (line " << __LINE__ << ")"; \
            TestFramework::TestSuite::instance().fail(ss.str()); return; \
        } \
    } while(0)

} // namespace TestFramework
