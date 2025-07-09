#include <vector>
#include "streamer/RTMPStreamer.hpp"

RTMPStreamer::RTMPStreamer(int w, int h, int f, const char* rtmp_url)
    : width(w), height(h), fps(f), pts(0),
      output_ctx(nullptr), codec_ctx(nullptr), frame(nullptr), sws_ctx(nullptr)
       {
    avformat_network_init();
    InitEncoder(rtmp_url);
}

void RTMPStreamer::InitEncoder(const char* rtmp_url) {
    // ——————————————————————————————————————————————————————————————
    // 1. 初始化网络模块（必须）
    //    在使用网络相关的 AVIO 操作前，需调用此函数。
    // ——————————————————————————————————————————————————————————————
    avformat_network_init();

    // ——————————————————————————————————————————————————————————————
    // 2. 创建输出上下文（AVFormatContext）
    //    这里指定容器格式为 "flv"，URL 为 RTMP 推流地址。
    //    成功后 output_ctx 会指向一个新的 AVFormatContext。
    // ——————————————————————————————————————————————————————————————
    if (avformat_alloc_output_context2(&output_ctx, nullptr, "flv", rtmp_url) < 0) {
        throw std::runtime_error("创建输出上下文失败");
    }

    // ——————————————————————————————————————————————————————————————
    // 3. 查找 H.264 编码器（libx264）
    //    AVCodecContext 将用来编码视频帧。
    // ——————————————————————————————————————————————————————————————
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        throw std::runtime_error("未找到H264编码器");
    }

    // ——————————————————————————————————————————————————————————————
    // 4. 分配并配置编码器上下文
    //    - width/height: 分辨率
    //    - time_base/framerate: 时间基准（帧率 1/fps）
    //    - pix_fmt: 像素格式（输出 YUV420P）
    //    - bit_rate/rc_max_rate/rc_buffer_size: 码率控制
    // ——————————————————————————————————————————————————————————————
    codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = {1, fps};
    codec_ctx->framerate = {fps, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = 2000000;
    codec_ctx->rc_max_rate = 2000000;
    codec_ctx->rc_buffer_size = 4000000;

    // ——————————————————————————————————————————————————————————————
    // 5. 设置 x264 参数字典
    //    - preset=ultrafast: 尽可能快的编码
    //    - tune=zerolatency: 零延迟编码，适合实时推流
    //    - profile/level: 保证兼容性
    //    - keyint/mbtree/bframes: GOP 长度与帧类型控制
    // ——————————————————————————————————————————————————————————————
    AVDictionary* codec_options = nullptr;
    av_dict_set(&codec_options, "preset", "ultrafast", 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);
    av_dict_set(&codec_options, "profile", "baseline", 0);
    av_dict_set(&codec_options, "level", "3.1", 0);
    av_dict_set(&codec_options, "x264-params",
                "keyint=60:min-keyint=30:no-scenecut=1"
                ":no-mbtree=1:bframes=0", 0);

    // 在 InitEncoder() 中，打开编码器前，添加：
    // 一些 RTMP 接收端（包括 Nginx-RTMP）要求所有的 codec extradata（SPS/PPS）
    // 在流的开始以“Global Header”形式发送。FFmpeg CLI 在封装到 FLV 时会自动处理这一步，
    // 但用 SDK 必须手动打开 AV_CODEC_FLAG_GLOBAL_HEADER
    if (output_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 打开编码器，并将参数应用到 codec_ctx
    if (avcodec_open2(codec_ctx, codec, &codec_options) < 0) {
        av_dict_free(&codec_options);
        throw std::runtime_error("打开编码器失败");
    }
    av_dict_free(&codec_options);

    // ——————————————————————————————————————————————————————————————
    // 6. 创建输出流（AVStream）并将 codec_ctx 信息拷贝进去
    //    output_ctx->streams[0] 即为我们的视频流，用于后续打包推送。
    // ——————————————————————————————————————————————————————————————
    AVStream* stream = avformat_new_stream(output_ctx, nullptr);
    if (!stream) {
        throw std::runtime_error("创建输出流失败");
    }
    video_stream = stream;  // 保存到成员变量，后面推帧时使用
    stream->time_base = codec_ctx->time_base;

    if (avcodec_parameters_from_context(stream->codecpar, codec_ctx) < 0) {
        throw std::runtime_error("从 codec context 设置 stream parameters 失败");
    }

    // ——————————————————————————————————————————————————————————————
    // 7. 打开网络 IO（AVIOContext）
    //    建立到 RTMP 服务器的 TCP 连接。
    // ——————————————————————————————————————————————————————————————
    if (avio_open(&output_ctx->pb, rtmp_url, AVIO_FLAG_WRITE) < 0) {
        throw std::runtime_error("无法打开输出流");
    }

    // ——————————————————————————————————————————————————————————————
    // 8. 写入流媒体头部（metadata）
    //    RTMP 握手及元数据在此阶段完成，必须成功写入否则服务器不会接收数据。
    // ——————————————————————————————————————————————————————————————
    if (avformat_write_header(output_ctx, nullptr) < 0) {
        throw std::runtime_error("写入头部失败");
    }

    // ——————————————————————————————————————————————————————————————
    // 9. 初始化颜色/像素格式转换器（SwsContext）
    //    OpenCV 传入的是 RGB24，需要转换到 YUV420P 供编码器编码。
    // ——————————————————————————————————————————————————————————————
    sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_RGB24,   // 输入格式
        width, height, AV_PIX_FMT_YUV420P, // 输出格式
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!sws_ctx) {
        throw std::runtime_error("初始化颜色转换器失败");
    }

    // ——————————————————————————————————————————————————————————————
    // 10. 分配 AVFrame 供后续填充 YUV 数据
    //     设置 frame 的格式、宽高，并分配缓冲区。
    // ——————————————————————————————————————————————————————————————
    frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width  = width;
    frame->height = height;
    if (av_frame_get_buffer(frame, 32) < 0) {
        throw std::runtime_error("分配帧缓冲区失败");
    }
}


// 推送一帧到 RTMP 服务器
void RTMPStreamer::PushFrame(const cv::Mat& rgbFrame) {
    // ——————————————————————————————————————————————————————————————
    // 1. 基本校验：确保所有上下文已正确初始化
    // ——————————————————————————————————————————————————————————————
    if (!frame || !sws_ctx || !codec_ctx || !output_ctx) {
        std::cerr << "[RTMPStreamer] 推流前检查失败: 初始化未完成" << std::endl;
        return;
    }

    // std::cout << "[RTMPStreamer] 开始推送帧..." << std::endl;

    // ——————————————————————————————————————————————————————————————
    // 2. 颜色空间转换：BGR24 (OpenCV) → YUV420P (编码器)
    //    sws_scale 会将 rgbFrame.data 转换并写入到 frame->data 中
    // ——————————————————————————————————————————————————————————————
    const uint8_t* srcData[1] = { rgbFrame.data };
    const int srcLinesize[1] = { static_cast<int>(rgbFrame.step) };
    sws_scale(sws_ctx, srcData, srcLinesize, 0,
              height, frame->data, frame->linesize);

    // ——————————————————————————————————————————————————————————————
    // 3. 设置 PTS（Presentation Timestamp），用于同步
    // ——————————————————————————————————————————————————————————————
    frame->pts = pts++;

    // ——————————————————————————————————————————————————————————————
    // 4. 发送帧到编码器（非阻塞或阻塞，取决实现）
    // ——————————————————————————————————————————————————————————————
    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "[RTMPStreamer] avcodec_send_frame 错误: " << errbuf << std::endl;
        return;
    }

    // ——————————————————————————————————————————————————————————————
    // 5. 从编码器接收 packet 并写到 RTMP 服务器
    //    一帧可能对应多个 packet，需要循环发送
    // ——————————————————————————————————————————————————————————————
    while (ret >= 0) {
        AVPacket* pkt = av_packet_alloc();

        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "[RTMPStreamer] avcodec_receive_packet 错误: " << errbuf << std::endl;
            av_packet_free(&pkt);
            break;
        }

        // ———— 设置 packet 属于哪个流 —————————————————
        pkt->stream_index = video_stream->index;
        // ———— 设置时间戳 ————————————————————————
        pkt->pts = frame->pts;
        pkt->dts = pkt->pts;
        // 可选：pkt->duration = 1;

        // ———— 实际写包到 RTMP —————————————————————
        ret = av_interleaved_write_frame(output_ctx, pkt);
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "[RTMPStreamer] 推送帧失败 av_interleaved_write_frame: "
                      << errbuf << std::endl;
        }

        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    // std::cout << "[RTMPStreamer] 推送完成一帧" << std::endl;
}

RTMPStreamer::~RTMPStreamer() {
    if (output_ctx) {
        av_write_trailer(output_ctx);
        if (output_ctx && !(output_ctx->oformat->flags & AVFMT_NOFILE) && output_ctx->pb) {
            avio_closep(&output_ctx->pb);
        }
        avformat_free_context(output_ctx);
    }
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (frame) av_frame_free(&frame);
    if (sws_ctx) sws_freeContext(sws_ctx);
    avformat_network_deinit();
}

