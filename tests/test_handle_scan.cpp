#include "sketch_api.h"
#include <gtest/gtest.h>
#include <string>

class HandleScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_scan_results.clear();
        g_mock_last_send_code = 0;
        g_mock_last_send_body = "";
    }
};

TEST_F(HandleScanTest, Returns200) {
    handleScan();
    EXPECT_EQ(g_mock_last_send_code, 200);
}

TEST_F(HandleScanTest, EmptyScanReturnsEmptyArray) {
    handleScan();
    EXPECT_EQ(std::string(g_mock_last_send_body.c_str()), "[]");
}

TEST_F(HandleScanTest, NetworksAppearInJson) {
    g_mock_scan_results.push_back({"HomeNet",   -55});
    g_mock_scan_results.push_back({"OfficeNet", -70});
    handleScan();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("HomeNet"),   std::string::npos);
    EXPECT_NE(body.find("OfficeNet"), std::string::npos);
    EXPECT_NE(body.find("-55"),       std::string::npos);
    EXPECT_NE(body.find("-70"),       std::string::npos);
}

TEST_F(HandleScanTest, SortedByRssiDescending) {
    g_mock_scan_results.push_back({"Weak",   -85});
    g_mock_scan_results.push_back({"Strong", -40});
    handleScan();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_LT(body.find("Strong"), body.find("Weak"))
        << "Stronger signal should appear before weaker";
}

TEST_F(HandleScanTest, DuplicateSsidsDeduplicated) {
    g_mock_scan_results.push_back({"MyNet", -40});
    g_mock_scan_results.push_back({"MyNet", -75});
    handleScan();
    std::string body = g_mock_last_send_body.c_str();
    auto first = body.find("\"MyNet\"");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(body.find("\"MyNet\"", first + 1), std::string::npos)
        << "Duplicate SSID should appear only once";
}

TEST_F(HandleScanTest, DedupKeepsStrongerRssi) {
    g_mock_scan_results.push_back({"MyNet", -40});
    g_mock_scan_results.push_back({"MyNet", -75});
    handleScan();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("-40"), std::string::npos);
    EXPECT_EQ(body.find("-75"), std::string::npos)
        << "Weaker duplicate RSSI should not appear";
}
