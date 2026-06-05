#include "test_framework.h"

// Test source files are compiled separately and register themselves
// via static initialization in the TEST() macro.

int main() { return TestFramework::TestSuite::instance().run(); }
