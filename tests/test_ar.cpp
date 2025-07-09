#include <gtest/gtest.h>

#include <iostream>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <string>

#include "vision/OpenCVProcessor.hpp"
#include "vision/V4L2Capture.hpp"

class ARTest : public ::testing::Test {
   protected:
    const std::string TEST_DEVICE = "/dev/video0";

    const std::string TEST_OUTPUT_PATH =
        "/home/lzk/Desktop/MiniCommStack/output";
    const OpenCVProcessor::PixelFormat TEST_FMT =
        OpenCVProcessor::PixelFormat::YUYV;

    void SetUp() override {
        capture = new V4L2Capture(TEST_DEVICE);
        ASSERT_TRUE(capture->initialize());
        processor = new OpenCVProcessor(TEST_FMT, capture->get_width(),
                                        capture->get_height());
    }

    void TearDown() override {
        delete capture;
        delete processor;
    }
    V4L2Capture* capture;
    OpenCVProcessor* processor;
};
TEST_F(ARTest, ProcessFrame) {
    std::vector<uint8_t> frameBuffer;
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(capture->capture_frame(frameBuffer))
            << "第 " << i << " 帧捕获失败";
        EXPECT_FALSE(frameBuffer.empty()) << "第 " << i << " 帧数据为空";
        // Step 3: 处理帧
        try {
            cv::Mat RGBFrame;
            processor->Decode2RGB(frameBuffer, RGBFrame);
            processor->apply_algorithm(RGBFrame);
            std::string path =
                processor->process_and_save(TEST_OUTPUT_PATH, RGBFrame);
            // std::cout << "第" << i << "个图片保存的路径为:" << path <<
            // std::endl;
        } catch (const cv::Exception& e) {
            FAIL() << "OpenCV 异常: " << e.what();
        } catch (const std::exception& e) {
            FAIL() << "标准异常: " << e.what();
        }
    }
}