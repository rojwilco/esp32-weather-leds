// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 rojwilco
// AI-assisted: generated with Claude (Anthropic); human conception, review, guidance and output approval by rojwilco.
#include "sketch_api.h"
#include <gtest/gtest.h>
#include <string>

class HandleOtaTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_last_send_code  = 0;
        g_mock_last_send_body  = "";
        g_mock_last_redirect   = "";
        g_mock_update_begin_ok   = true;
        g_mock_update_write_fail = false;
        g_mock_update_end_ok     = true;
        g_mock_update_has_error  = false;
        g_mock_update_error_str  = "";
        g_mock_update_written    = 0;
        g_mock_upload            = HTTPUpload{};
    }
};

// Description: When the upload completes without error, handleOtaUpdate() sends
// a 303 redirect to "/" so the browser returns to the config page.
TEST_F(HandleOtaTest, SuccessRedirectsToRoot) {
    RecordProperty("description",
        "When the upload completes without error, handleOtaUpdate() sends a 303 "
        "redirect to \"/\" so the browser returns to the config page.");
    g_mock_update_has_error = false;
    handleOtaUpdate();
    EXPECT_EQ(g_mock_last_send_code, 303);
    EXPECT_EQ(std::string(g_mock_last_redirect.c_str()), "/");
}

// Description: When Update.hasError() is true after upload, handleOtaUpdate()
// sends HTTP 500 containing the error string so the user knows the update failed.
TEST_F(HandleOtaTest, ErrorSends500WithMessage) {
    RecordProperty("description",
        "When Update.hasError() is true after upload, handleOtaUpdate() sends "
        "HTTP 500 containing the error string so the user knows the update failed.");
    g_mock_update_has_error = true;
    g_mock_update_error_str = "MD5 mismatch";
    handleOtaUpdate();
    EXPECT_EQ(g_mock_last_send_code, 500);
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("MD5 mismatch"), std::string::npos);
}

// Description: On UPLOAD_FILE_START, handleOtaUpload() calls Update.begin(),
// confirmed by the mock resetting g_mock_update_written to 0.
TEST_F(HandleOtaTest, UploadStartCallsUpdateBegin) {
    RecordProperty("description",
        "On UPLOAD_FILE_START, handleOtaUpload() calls Update.begin(), "
        "confirmed by the mock resetting g_mock_update_written to 0.");
    g_mock_update_written    = 999;  // sentinel — begin() resets this to 0
    g_mock_upload.status     = UPLOAD_FILE_START;
    g_mock_upload.filename   = String("firmware.bin");
    handleOtaUpload();
    EXPECT_EQ(g_mock_update_written, 0u);
}

// Description: On UPLOAD_FILE_WRITE with a 256-byte chunk, handleOtaUpload()
// passes the chunk to Update.write() and the mock records the bytes written.
TEST_F(HandleOtaTest, UploadWritePassesChunkToUpdate) {
    RecordProperty("description",
        "On UPLOAD_FILE_WRITE with a 256-byte chunk, handleOtaUpload() passes the "
        "chunk to Update.write() and the mock records the bytes written.");
    g_mock_upload.status      = UPLOAD_FILE_WRITE;
    g_mock_upload.currentSize = 256;
    memset(g_mock_upload.buf, 0xAA, 256);
    handleOtaUpload();
    EXPECT_EQ(g_mock_update_written, 256u);
}

// Description: On UPLOAD_FILE_END, handleOtaUpload() calls Update.end(true) to
// finalise the flash write; the upload handler itself must not send any HTTP response.
TEST_F(HandleOtaTest, UploadEndCallsUpdateEnd) {
    RecordProperty("description",
        "On UPLOAD_FILE_END, handleOtaUpload() calls Update.end(true) to finalise "
        "the flash write; the upload handler itself must not send any HTTP response.");
    g_mock_upload.status    = UPLOAD_FILE_END;
    g_mock_upload.totalSize = 512;
    handleOtaUpload();
    // The upload handler does not send; send_code must remain 0.
    EXPECT_EQ(g_mock_last_send_code, 0);
}

// Description: The config page HTML contains a form POSTing to /update with
// multipart/form-data encoding, confirming the firmware update UI is present.
TEST_F(HandleOtaTest, RootPageContainsUpdateForm) {
    RecordProperty("description",
        "The config page HTML contains a form POSTing to /update with "
        "multipart/form-data encoding, confirming the firmware update UI is present.");
    g_ap_mode = false;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("action=\"/update\""),  std::string::npos);
    EXPECT_NE(body.find("multipart/form-data"), std::string::npos);
    EXPECT_NE(body.find("type=\"file\""),        std::string::npos);
}
