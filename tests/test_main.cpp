#include <string>

#include "test_framework.h"

// Test source files are compiled separately and register themselves
// via static initialization in the TEST() macro.

int main(int argc, char* argv[]) {
    std::string junit_path = "";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--junitxml=", 0) == 0) {
            junit_path = arg.substr(11);
        }
    }
    return TestFramework::TestSuite::instance().run(junit_path);
}
