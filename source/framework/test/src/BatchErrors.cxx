#include <TRestProcessRunner.h>
#include <TRestTask.h>
#include <gtest/gtest.h>

namespace {
class RequiredArgumentTask : public TRestTask {
   public:
    RequiredArgumentTask() { fNRequiredArgument = 1; }
};

TEST(BatchErrors, MissingManagerExitsWithFailure) {
    EXPECT_EXIT(
        {
            TRestProcessRunner runner;
            runner.BeginOfInit();
        },
        ::testing::ExitedWithCode(1), "");
}

TEST(BatchErrors, MissingTaskArgumentExitsWithFailure) {
    EXPECT_EXIT(
        {
            RequiredArgumentTask task;
            task.SetArgumentValue({});
        },
        ::testing::ExitedWithCode(1), "");
}
}  // namespace
