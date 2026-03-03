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

// Description: handleScan() responds with HTTP 200, confirming the endpoint
// is registered and reachable.
TEST_F(HandleScanTest, Returns200) {
    RecordProperty("description",
        "handleScan() responds with HTTP 200, confirming the endpoint "
        "is registered and reachable.");
    handleScan();
    EXPECT_EQ(g_mock_last_send_code, 200);
}

// Description: When no WiFi networks are visible, handleScan() returns a JSON
// empty array '[]' rather than null or an empty string.
TEST_F(HandleScanTest, EmptyScanReturnsEmptyArray) {
    RecordProperty("description",
        "When no WiFi networks are visible, handleScan() returns a JSON empty "
        "array '[]' rather than null or an empty string.");
    handleScan();
    EXPECT_EQ(std::string(g_mock_last_send_body.c_str()), "[]");
}

// Description: Visible networks appear in the JSON response with their SSID
// and RSSI values so the UI can display them to the user.
TEST_F(HandleScanTest, NetworksAppearInJson) {
    RecordProperty("description",
        "Visible networks appear in the JSON response with their SSID and RSSI "
        "values so the UI can display them to the user.");
    g_mock_scan_results.push_back({"HomeNet",   -55});
    g_mock_scan_results.push_back({"OfficeNet", -70});
    handleScan();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("HomeNet"),   std::string::npos);
    EXPECT_NE(body.find("OfficeNet"), std::string::npos);
    EXPECT_NE(body.find("-55"),       std::string::npos);
    EXPECT_NE(body.find("-70"),       std::string::npos);
}

// Description: Networks are sorted by RSSI in descending order (strongest signal
// first) so the best option appears at the top of the UI dropdown.
TEST_F(HandleScanTest, SortedByRssiDescending) {
    RecordProperty("description",
        "Networks are sorted by RSSI in descending order (strongest signal first) "
        "so the best option appears at the top of the UI dropdown.");
    g_mock_scan_results.push_back({"Weak",   -85});
    g_mock_scan_results.push_back({"Strong", -40});
    handleScan();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_LT(body.find("Strong"), body.find("Weak"))
        << "Stronger signal should appear before weaker";
}

// Description: If the same SSID appears multiple times in the scan results
// (e.g., multiple access points), only one entry is included in the response.
TEST_F(HandleScanTest, DuplicateSsidsDeduplicated) {
    RecordProperty("description",
        "If the same SSID appears multiple times in the scan results (e.g., "
        "multiple access points), only one entry is included in the response.");
    g_mock_scan_results.push_back({"MyNet", -40});
    g_mock_scan_results.push_back({"MyNet", -75});
    handleScan();
    std::string body = g_mock_last_send_body.c_str();
    auto first = body.find("\"MyNet\"");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(body.find("\"MyNet\"", first + 1), std::string::npos)
        << "Duplicate SSID should appear only once";
}

// Description: When deduplicating, the entry with the stronger (higher) RSSI
// is kept so the UI reflects the best available signal for that network name.
TEST_F(HandleScanTest, DedupKeepsStrongerRssi) {
    RecordProperty("description",
        "When deduplicating, the entry with the stronger (higher) RSSI is kept "
        "so the UI reflects the best available signal for that network name.");
    g_mock_scan_results.push_back({"MyNet", -40});
    g_mock_scan_results.push_back({"MyNet", -75});
    handleScan();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("-40"), std::string::npos);
    EXPECT_EQ(body.find("-75"), std::string::npos)
        << "Weaker duplicate RSSI should not appear";
}
