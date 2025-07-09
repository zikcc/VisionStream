#include "capture/V4L2Capture.hpp"
#include <iostream>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdexcept>

V4L2Capture::V4L2Capture(const std::string& device) {
    // 读写 非阻塞
    fd_ = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open V4L2 device");
    }
}

V4L2Capture::~V4L2Capture() {
    // 释放资源
    if (fd_ >= 0) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd_, VIDIOC_STREAMOFF, &type);
        for (size_t i = 0; i < mapped_buffers_.size(); ++i) {
            if (mapped_buffers_[i]) {
                munmap(mapped_buffers_[i], buffer_lengths_[i]);
            }
        }
        close(fd_);
    }
}

bool V4L2Capture::initialize(unsigned width, unsigned height) {
    if (!set_format(width, height)) return false;
    return init_mmap();
}



bool V4L2Capture::set_format(unsigned width, unsigned height) {
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // 硬件压缩格式
    // 将配置发送给驱动
    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) return false;

    // 驱动可能会调整至它支持的最接近的分辨率，读回 fmt.fmt.pix.
    // 驱动会返回实际生效的格式
    width_ = fmt.fmt.pix.width;
    height_ = fmt.fmt.pix.height;
    return true;
}

// 请求内核分配缓冲区
// 把这些缓冲区映射到用户空间
bool V4L2Capture::init_mmap() {
    // 1) 请求 4 个缓冲区，类型：视频捕捉、内存映射
    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    // 对这个设备申请缓存区
    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0) return false;

    // 2) 分配存储映射地址和长度的容器
    mapped_buffers_.resize(req.count);
    buffer_lengths_.resize(req.count);  // 添加这行
    // 3) 对每个缓冲区：
    for (unsigned i = 0; i < req.count; ++i) {
        // 3.1) 查询缓冲区信息（包括它在内核中的长度和偏移）
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        // 查询第 i 个缓存区的大小和在内核空间的偏移量
        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) return false;

        buffer_lengths_[i] = buf.length;  // 保存长度
        // 3.2) mmap()：把内核里的 buf.length 大小的那块内存映射到用户空间，
        //      并把指针保存到 mapped_buffers_[i]
        // // 让系统自己选地址 // 大小 // 可读可写  // 共享映射 //
        // 哪个文件（设备） // 内核里缓冲区的偏移
        mapped_buffers_[i] = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd_, buf.m.offset);
        if (mapped_buffers_[i] == MAP_FAILED) return false;
    }
    // 4) 把所有缓冲区 “入队” 到采集队列：driver 会往这些队列里不断填充新帧
    for (unsigned i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        // 把编号为 i 的空缓冲区放入内核的采集队列  把空的缓冲区交给驱动去填数据
        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) return false;
    }
    // 5) 启动数据流：驱动收到这个命令后开始不停地往队列缓冲区写入新帧
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return ioctl(fd_, VIDIOC_STREAMON, &type) >= 0;
}

bool V4L2Capture::capture_frame(std::vector<uint8_t>& buffer) {
    // 1) 等待 fd_ “可读” —— 意味着至少一个缓冲区里已有完整一帧
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    timeval tv{2, 0}; // 最长等 2 秒
    int ret = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (ret <= 0) {
        // ret == 0 表示超时；ret < 0 表示出错
        return false;
    }

    // 2) 循环尝试 DQBUF，直到成功或遇到非 EAGAIN 错误
    v4l2_buffer buf{};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    for (int attempts = 0; attempts < 3; ++attempts) {
        if (ioctl(fd_, VIDIOC_DQBUF, &buf) == 0) {
            // 成功取到一帧
            const auto* src = static_cast<uint8_t*>(mapped_buffers_[buf.index]);
            buffer.assign(src, src + buf.bytesused);
            if (buffer.empty()) {
                throw std::runtime_error("摄像头返回空数据");
            }
            // std::cout << "采集数据大小: " << buffer.size() << " 字节" << std::endl;
            // 重新入队
            ioctl(fd_, VIDIOC_QBUF, &buf);
            return !buffer.empty();
        }
        if (errno != EAGAIN) {
            // 真正的错误
            return false;
        }
        // 如果是 EAGAIN，短暂休息再重试
        usleep(1000); // 1 ms
    }

    // 多次 EAGAIN 后仍然失败
    return false;
}