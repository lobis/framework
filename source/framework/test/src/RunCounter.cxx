#include <TRestDataBase.h>
#include <TUUID.h>
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <set>

namespace {
namespace fs = std::filesystem;
class RunCounterTest : public ::testing::Test {
   protected:
    fs::path directory;
    std::string previousPath;
    void SetUp() override {
        previousPath = REST_USER_PATH;
        directory = fs::temp_directory_path() / (std::string("rest-counter-") + TUUID().AsString());
        fs::create_directories(directory);
        REST_USER_PATH = directory.string();
    }
    void TearDown() override {
        REST_USER_PATH = previousPath;
        fs::remove_all(directory);
    }
};

TEST_F(RunCounterTest, ExplicitShorterCounterTruncatesOldDigits) {
    std::ofstream(directory / "runNumber") << "123456\n";
    TRestDataBase database;
    EXPECT_EQ(database.set_run(DBEntry(8)), 8);
    EXPECT_EQ(fs::file_size(directory / "runNumber"), 2U);
    EXPECT_EQ(database.get_lastrun(), 8);
    EXPECT_EQ(database.set_run(DBEntry()), 9);
}

TEST_F(RunCounterTest, InvalidCounterAndMissingDirectoryRefuseAllocation) {
    std::ofstream(directory / "runNumber") << "invalid\n";
    TRestDataBase database;
    EXPECT_EQ(database.set_run(DBEntry()), -1);
    EXPECT_EQ(database.get_lastrun(), -1);
    REST_USER_PATH = (directory / "missing").string();
    EXPECT_EQ(database.set_run(DBEntry()), -1);
}

TEST_F(RunCounterTest, ConcurrentProcessesAllocateUniqueConsecutiveRuns) {
    constexpr int processes = 8;
    constexpr int allocations = 40;
    std::vector<pid_t> children;
    for (int process = 0; process < processes; ++process) {
        const auto child = fork();
        ASSERT_GE(child, 0);
        if (child == 0) {
            TRestDataBase database;
            std::ofstream output(directory / ("child-" + std::to_string(process)));
            for (int index = 0; index < allocations; ++index) {
                const int run = database.set_run(DBEntry());
                if (run < 1) _exit(1);
                output << run << '\n';
            }
            output.close();
            _exit(output ? 0 : 1);
        }
        children.push_back(child);
    }
    for (const auto child : children) {
        int status = 0;
        ASSERT_EQ(waitpid(child, &status, 0), child);
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }
    std::set<int> runs;
    for (int process = 0; process < processes; ++process) {
        std::ifstream input(directory / ("child-" + std::to_string(process)));
        int run;
        while (input >> run) EXPECT_TRUE(runs.insert(run).second);
    }
    ASSERT_EQ(runs.size(), static_cast<size_t>(processes * allocations));
    EXPECT_EQ(*runs.begin(), 1);
    EXPECT_EQ(*runs.rbegin(), processes * allocations);
    TRestDataBase database;
    EXPECT_EQ(database.get_lastrun(), processes * allocations);
}
}  // namespace
