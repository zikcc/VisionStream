#pragma once
#include <opencv2/opencv.hpp>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <string>
#include <stdexcept>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

class RTMPStreamer {
    public:
        RTMPStreamer(int w, int h, int f, const char* rtmp_url);
        ~RTMPStreamer();
    
        void PushFrame(const cv::Mat& rgbFrame);  // 由外部线程定时调用
    private:
        void InitEncoder(const char* rtmp_url);
    
        int width, height, fps;
        int64_t pts;
    
        AVFormatContext* output_ctx;
        AVCodecContext* codec_ctx;
        AVFrame* frame;
        SwsContext* sws_ctx;
        AVStream* video_stream; // 你应在类中添加 AVStream* video_stream

    };
    
