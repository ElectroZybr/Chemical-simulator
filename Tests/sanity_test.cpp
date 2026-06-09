#include <gtest/gtest.h>

// Минимальный sanity-тест: проверяет, что GTest harness собирается
// и линкуется. Реальные тесты физики/NL/AtomStorage добавляются в
// Stream A3 в Tests/Engine/.

TEST(Sanity, HarnessRuns) {
    EXPECT_EQ(2 + 2, 4);
}
