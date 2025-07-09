#include "vision/V4L2Capture.hpp"
#include <gtest/gtest.h>

class V4L2Test : public ::testing::Test {
protected:
    const std::string TEST_DEVICE = "/dev/video0";
    
    void SetUp() override {
        capture = new V4L2Capture(TEST_DEVICE);
    }
    
    void TearDown() override {
        delete capture;
    }
    
    V4L2Capture* capture;
};

TEST_F(V4L2Test, InitializesWithValidDevice) {
    EXPECT_TRUE(capture->initialize(640, 480)) 
        << "初始化 V4L2 设备失败";
}

TEST_F(V4L2Test, CapturesFramesConsistently) {
    ASSERT_TRUE(capture->initialize(640, 480));
    
    std::vector<uint8_t> frameBuffer;
    for(int i = 0; i < 5; ++i) {
        EXPECT_TRUE(capture->capture_frame(frameBuffer)) 
            << "Failed to capture frame " << i;
        EXPECT_FALSE(frameBuffer.empty())
            << "Frame " << i << " is empty";
    }
}