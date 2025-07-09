#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include "capture/V4L2Capture.hpp"
#include "processor/OpenCVProcessor.hpp"
#include "streamer/RTMPStreamer.hpp"
#include "queue/ThreadSafeQueue.hpp"  // 假设你之前的线程安全队列文件叫这个

int main() {
    const std::string VIDEO_DEVICE = "/dev/video0";
    const OpenCVProcessor::PixelFormat FMT = OpenCVProcessor::PixelFormat::YUYV;

    V4L2Capture capture(VIDEO_DEVICE);
    if (!capture.initialize()) {
        std::cerr << "摄像头初始化失败! 请检查设备权限和格式支持" << std::endl;
        return -1;
    }

    const int width  = capture.get_width();
    const int height = capture.get_height();
    OpenCVProcessor processor(FMT, width, height);

    // 推流器初始化
    char output_url[] = "rtmp://192.168.217.130/live/stream";
    RTMPStreamer streamer(width, height, 30, output_url);
    std::cout << "[RTMPStreamer] 初始化完成，开始推流到: " << output_url << std::endl;

    // 线程安全帧队列
    ThreadSafeQueue<cv::Mat> frameQueue;

    // --- 推流线程 ---
    std::thread streaming_thread([&]() {
        while (true) {
            cv::Mat frame;
            if (frameQueue.pop(frame)) {  // 阻塞直到取出一帧
                streamer.PushFrame(frame);
            }
        }
    });
    std::cout << "[RTMPStreamer] 推流线程创建成功！"<< std::endl;

    std::vector<uint8_t> frameBuffer;
    cv::Mat RGBFrame;

    const auto frame_interval = std::chrono::milliseconds(1000 / 30);
    while (true) {
        auto t0 = std::chrono::steady_clock::now();

        if (!capture.capture_frame(frameBuffer)) {
            std::cerr << "帧捕获失败\n";
            continue;
        }
        // std::cout << "[V4L2Capture] 捕获到图像数据！"<< std::endl;
        if (!processor.Decode2RGB(frameBuffer, RGBFrame)) {
            std::cerr << "解码失败\n";
            continue;
        }
        processor.apply_algorithm(RGBFrame);
        // std::cout << "[OpenCVProcessor] 图像处理完成!"<< std::endl;
        // 将RGB帧放入队列
        frameQueue.push(RGBFrame.clone());  // clone防止异步数据冲突

        auto dt = std::chrono::steady_clock::now() - t0;
        if (dt < frame_interval) {
            std::this_thread::sleep_for(frame_interval - dt);
        }
    }

    streaming_thread.join();
    return 0;
}
