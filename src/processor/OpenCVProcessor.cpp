#include "processor/OpenCVProcessor.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <opencv2/core/mat.hpp>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

OpenCVProcessor::OpenCVProcessor(PixelFormat fmt, unsigned width,
                                 unsigned height)
    : pixel_format_(fmt), width_(width), height_(height) {}

bool OpenCVProcessor::Decode2RGB(const std::vector<uint8_t>& raw_data,
                                 cv::Mat& RGBFrame) {
    if (raw_data.empty()) {
        std::cerr << "接收到的数据为空! " << std::endl;
        return false;
    }
    cv::Mat frame;
    if (pixel_format_ == PixelFormat::MJPEG) {
        // MJPEG 解码（BGR 格式）
        cv::Mat raw_mat(1, static_cast<int>(raw_data.size()), CV_8UC1,
                        const_cast<uint8_t*>(raw_data.data()));
        frame = cv::imdecode(raw_mat, cv::IMREAD_COLOR);
        if (frame.empty()) {
            std::cerr << "MJPEG 解码失败! " << std::endl;
            return false;
        }
        // BGR → RGB 转换
        cv::cvtColor(frame, RGBFrame, cv::COLOR_BGR2RGB);
    } else if (pixel_format_ == PixelFormat::YUYV) {
        // 验证 YUYV 数据大小
        size_t expected_size = width_ * height_ * 2;
        if (raw_data.size() != expected_size) {
            throw std::runtime_error(
                "YUYV 数据大小错误: 期望 " + std::to_string(expected_size) +
                " 字节，实际 " + std::to_string(raw_data.size()));
            return false;
        }
        // YUYV → BGR
        // 创建 YUYV Mat 对象
        cv::Mat yuyv_frame(height_, width_, CV_8UC2, (void*)raw_data.data());
        if (yuyv_frame.empty()) {
            throw std::runtime_error("YUYV 帧数据为空");
            return false;
        }
        cv::cvtColor(yuyv_frame, RGBFrame, cv::COLOR_YUV2RGB_YUYV);
    }
    return true;
}

std::string OpenCVProcessor::process_and_save(const std::string& output_dir,
                                              cv::Mat& RGBFrame) {
    // 确保输出目录存在
    if (!fs::exists(output_dir)) {
        if (!fs::create_directories(output_dir)) {
            throw std::runtime_error("Failed to create output directory: " +
                                     output_dir);
        }
    }
    apply_algorithm(RGBFrame);

    // 生成文件名：例如 output_dir_/frame_0001.png
    std::ostringstream oss;
    oss << output_dir << "/frame_" << std::setfill('0') << std::setw(4)
        << frame_count_++ << ".png";
    std::string file_path = oss.str();

    // 保存
    if (!cv::imwrite(file_path, RGBFrame)) {
        std::cerr << "图片保存失败! " << std::endl;
        return "";
    }

    return file_path;
}

void OpenCVProcessor::apply_algorithm(cv::Mat& frame) {
    // 示例：Canny 边缘检测
    cv::Mat gray, edges;
    cv::cvtColor(frame, gray, cv::COLOR_RGB2GRAY);
    cv::Canny(gray, edges, 100, 200);
    cv::cvtColor(edges, frame, cv::COLOR_GRAY2RGB);
}
