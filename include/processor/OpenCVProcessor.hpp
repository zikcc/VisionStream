#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <atomic>
class OpenCVProcessor {
public:
    enum class PixelFormat { MJPEG, YUYV };
    // output_dir: 保存图像的文件夹（末尾不带 '/'）
    OpenCVProcessor(PixelFormat fmt,
                    unsigned width,
                    unsigned height);
    ~OpenCVProcessor() = default;

    // 处理一帧 raw_data
    bool Decode2RGB(const std::vector<uint8_t>& raw_data, cv::Mat& RGBFrame);
    // 返回保存的文件路径，或空字符串表示失败
    std::string process_and_save(const std::string& output_dir, cv::Mat& RGBFrame);
    void apply_algorithm(cv::Mat& frame);

private:
    PixelFormat    pixel_format_;
    unsigned       width_, height_;
    std::atomic<unsigned>      frame_count_ = 0;
};
