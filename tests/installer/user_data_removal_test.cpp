#include "user_data_removal.h"

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace gazeora::setup {
namespace {

class FakeSession final : public InstallerSession {
public:
    std::wstring Property(const std::wstring& name) const override {
        auto found = properties.find(name);
        return found == properties.end() ? L"" : found->second;
    }
    bool AskYesNo(const std::wstring&) const override {
        ++questions;
        return answer;
    }
    void Log(const std::wstring&) const override { ++logged; }

    std::map<std::wstring, std::wstring> properties;
    bool answer = false;
    mutable int questions = 0;
    mutable int logged = 0;
};

class FakeFolder final : public UserDataFolder {
public:
    std::wstring Path() const override { return L"C:\\Users\\Someone\\AppData\\Roaming\\Gazeora"; }
    bool Exists() const override { return exists; }
    bool Remove() const override {
        ++removals;
        return removable;
    }

    bool exists = true;
    bool removable = true;
    mutable int removals = 0;
};

TEST(UserDataRemoval, RemovesWhenTheUserSaysYes) {
    FakeSession session;
    session.answer = true;
    FakeFolder folder;
    UserDataRemoval(session, folder).Run();
    EXPECT_EQ(session.questions, 1);
    EXPECT_EQ(folder.removals, 1);
}

TEST(UserDataRemoval, KeepsWhenTheUserSaysNo) {
    FakeSession session;
    FakeFolder folder;
    UserDataRemoval(session, folder).Run();
    EXPECT_EQ(session.questions, 1);
    EXPECT_EQ(folder.removals, 0);
}

TEST(UserDataRemoval, AsksNothingWithoutAFolder) {
    FakeSession session;
    FakeFolder folder;
    folder.exists = false;
    UserDataRemoval(session, folder).Run();
    EXPECT_EQ(session.questions, 0);
    EXPECT_EQ(folder.removals, 0);
}

TEST(UserDataRemoval, PropertyDecidesWithoutAsking) {
    FakeSession remove;
    remove.properties[L"REMOVEUSERDATA"] = L"1";
    FakeFolder removed;
    UserDataRemoval(remove, removed).Run();
    EXPECT_EQ(remove.questions, 0);
    EXPECT_EQ(removed.removals, 1);

    FakeSession keep;
    keep.properties[L"REMOVEUSERDATA"] = L"0";
    keep.answer = true;
    FakeFolder kept;
    UserDataRemoval(keep, kept).Run();
    EXPECT_EQ(keep.questions, 0);
    EXPECT_EQ(kept.removals, 0);
}

TEST(UserDataRemoval, LogsWhatItCouldNotDelete) {
    FakeSession session;
    session.answer = true;
    FakeFolder folder;
    folder.removable = false;
    UserDataRemoval(session, folder).Run();
    EXPECT_EQ(session.logged, 1);
}

}  // namespace
}  // namespace gazeora::setup
