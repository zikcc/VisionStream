#pragma once
#include <string>
#include <vector>
#include <linux/videodev2.h>

// 对底层 Linux 视频接口进行抽象，方便上层逻辑调用
class V4L2Capture {
public:
    explicit V4L2Capture(const std::string& device = "/dev/video0");
    ~V4L2Capture();
    // 用于执行：
    // 设置格式（调用 set_format()）；
    // 设置缓冲区并映射（调用 init_mmap()）；
    // 开启视频采集流（一般需 VIDIOC_STREAMON）
    bool initialize(unsigned width = 1280, unsigned height = 720);
    // 从摄像头中读取一帧数据，并保存到 buffer 中
    bool capture_frame(std::vector<uint8_t>& buffer);
    unsigned get_width() const { return width_; }
    unsigned get_height() const { return height_; }
    
private:
    // 摄像头设备文件描述符
    int fd_ = -1;
    unsigned width_ = 0;
    unsigned height_ = 0;
    // 存储映射缓冲区地址列表，通常使用 mmap() 映射 V4L2 的缓冲区
    // 用于存取图像数据，而不必每次都 read()，性能更高
    std::vector<void*> mapped_buffers_;
    // 添加每个 mmap buffer 的长度
    std::vector<size_t> buffer_lengths_;  
    // 配置缓存方式为 mmap（即使用 VIDIOC_REQBUFS 请求缓冲区，VIDIOC_QUERYBUF 查询每个缓冲区信息，然后用 mmap 映射）
    bool init_mmap();
    bool set_format(unsigned width, unsigned height);
};