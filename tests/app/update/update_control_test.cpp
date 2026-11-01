// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "update/update_control.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include "gazeora/update/platform/bcrypt_file_hasher.h"
#include "gazeora/update/repositories/web_release_feed.h"
#include "update/fake_http_client.h"
#include "update/feed_json.h"

namespace gazeora::app {
namespace {

constexpr char kMsiBody[] = "abcd";
constexpr char kMsiSha256[] = "88d4266fd4e6338d13b845fcf289579d209c897823b9217da3e161936f031589";
constexpr char kMsiUrl[] = "https://example.test/Gazeora-v1.1.0-x64.msi";
constexpr auto kPatience = std::chrono::seconds(10);

class FakeRunner final : public IInstallerRunner {
public:
    void Run(const std::wstring& path) override { started.push_back(path); }
    std::vector<std::wstring> started;
};

// UpdateControl at v1.0.0 x64, over a fake website offering v1.1.0.
class UpdateControlTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::remove_all(folder);
        std::filesystem::create_directories(folder);
        const std::string latest = ReleaseJson("v1.1.0", {"x64"}, kMsiSha256);
        http.Serve(std::string(kFeedFolder) + "index.json", IndexJson(latest, 1));
        http.Serve(kMsiUrl, kMsiBody);
    }
    void TearDown() override { std::filesystem::remove_all(folder); }

    // Ticks until the background step is done.
    void Settle(UpdateControl& control) {
        const auto deadline = std::chrono::steady_clock::now() + kPatience;
        while (control.Busy() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            control.Tick();
        }
        ASSERT_FALSE(control.Busy());
    }

    UpdateEvents Events() {
        UpdateEvents events;
        events.found = [this](const Release& release) {
            found.push_back(release.Version().Label());
        };
        events.installerStarted = [this] { ++installersStarted; };
        return events;
    }

    std::filesystem::path folder =
        std::filesystem::temp_directory_path() /
        ("gazeora_update_control_" + std::to_string(GetCurrentProcessId()));
    FakeHttpClient http;
    BcryptFileHasher hasher;
    FakeRunner runner;
    WebReleaseFeed feed{http, kFeedFolder};
    UpdateChecker checker{feed, *ReleaseVersion::Parse("v1.0.0"), "x64"};
    InstallerDownloader downloader{http, hasher, (folder / "downloads").wstring()};
    UpdateSettings settings{(folder / "updates.ini").wstring()};
    std::vector<std::string> found;
    int installersStarted = 0;
};

TEST_F(UpdateControlTest, AnnouncesAnUpdateFoundAtStart) {
    UpdateControl control(checker, downloader, runner, settings, Events());
    EXPECT_TRUE(control.CheckOnStart());
    control.Start();
    Settle(control);
    EXPECT_EQ(control.State(), UpdateState::Available);
    EXPECT_EQ(control.Update()->Version().Label(), "v1.1.0");
    EXPECT_EQ(found, std::vector<std::string>{"v1.1.0"});
}

TEST_F(UpdateControlTest, ChecksNothingAtStartOnceSwitchedOff) {
    UpdateControl(checker, downloader, runner, settings, Events()).SetCheckOnStart(false);
    UpdateControl control(checker, downloader, runner, settings, Events());
    EXPECT_FALSE(control.CheckOnStart());
    control.Start();
    EXPECT_EQ(control.State(), UpdateState::Idle);
    EXPECT_TRUE(http.Requests().empty());
}

TEST_F(UpdateControlTest, ShowsAnUpdateFoundByHandWithoutAnnouncingIt) {
    UpdateControl control(checker, downloader, runner, settings, Events());
    control.Check();
    Settle(control);
    EXPECT_EQ(control.State(), UpdateState::Available);
    EXPECT_TRUE(found.empty());
}

TEST_F(UpdateControlTest, SaysWhenItIsUpToDate) {
    http.Serve(std::string(kFeedFolder) + "index.json",
               IndexJson(ReleaseJson("v1.0.0", {"x64"}), 0));
    UpdateControl control(checker, downloader, runner, settings, Events());
    control.Check();
    Settle(control);
    EXPECT_EQ(control.State(), UpdateState::UpToDate);
}

TEST_F(UpdateControlTest, DownloadsChecksAndStartsTheInstaller) {
    UpdateControl control(checker, downloader, runner, settings, Events());
    control.Check();
    Settle(control);
    control.Install();
    EXPECT_EQ(control.State(), UpdateState::Downloading);
    Settle(control);
    EXPECT_EQ(control.State(), UpdateState::Installing);
    ASSERT_EQ(runner.started.size(), 1u);
    EXPECT_EQ(std::filesystem::path(runner.started[0]),
              folder / "downloads" / "Gazeora-v1.1.0-x64.msi");
    EXPECT_EQ(installersStarted, 1);
}

TEST_F(UpdateControlTest, NeverStartsAnInstallerThatFailsItsChecksum) {
    http.Serve(kMsiUrl, "abce");
    UpdateControl control(checker, downloader, runner, settings, Events());
    control.Check();
    Settle(control);
    control.Install();
    Settle(control);
    EXPECT_EQ(control.State(), UpdateState::Failed);
    EXPECT_FALSE(control.Error().empty());
    EXPECT_TRUE(runner.started.empty());

    http.Serve(kMsiUrl, kMsiBody);
    control.Retry();
    Settle(control);
    EXPECT_EQ(control.State(), UpdateState::Installing);
}

TEST_F(UpdateControlTest, ReportsAFailedCheckAndTriesAgain) {
    http.Serve(std::string(kFeedFolder) + "index.json", "not json");
    UpdateControl control(checker, downloader, runner, settings, Events());
    control.Start();
    Settle(control);
    EXPECT_EQ(control.State(), UpdateState::Failed);
    EXPECT_TRUE(found.empty());

    http.Serve(std::string(kFeedFolder) + "index.json",
               IndexJson(ReleaseJson("v1.1.0", {"x64"}, kMsiSha256), 1));
    control.Retry();
    Settle(control);
    EXPECT_EQ(control.State(), UpdateState::Available);
}

}  // namespace
}  // namespace gazeora::app
