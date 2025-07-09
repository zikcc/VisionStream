// live_display.cpp
#include <SDL2/SDL.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include "capture/V4L2Capture.hpp"
#include "processor/OpenCVProcessor.hpp"

int main() {
    const std::string VIDEO_DEVICE = "/dev/video0";
    const OpenCVProcessor::PixelFormat FMT = OpenCVProcessor::PixelFormat::YUYV;
    V4L2Capture capture = V4L2Capture(VIDEO_DEVICE);
    if (!capture.initialize()) {
        std::cerr << "摄像头初始化失败! 请检查设备权限和格式支持" << std::endl;
        return -1;
    }
    const int width_ = capture.get_width();
    const int height_ = capture.get_height();
    OpenCVProcessor processor = OpenCVProcessor(FMT, width_, height_);


    // 初始化 SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL 初始化失败: " << SDL_GetError() << std::endl;
        return -1;
    }
    // 创建双窗口
    const int window_number = 2;
    SDL_Window* windows[window_number];
    SDL_Renderer* renderers[window_number];
    SDL_Texture* textures[window_number];
    std::vector<std::string> title = {"原画面", "处理画面"};
    for(int i = 0; i < window_number; ++i) {
        windows[i] = SDL_CreateWindow(title[i].c_str(), 
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            width_, height_, SDL_WINDOW_SHOWN);
        if (!windows[i]) {
            std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return -1;
        }
        renderers[i] = SDL_CreateRenderer(windows[i], -1, SDL_RENDERER_ACCELERATED);
        if (!renderers[i]) {
            std::cerr << "渲染器创建失败: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(windows[i]);
            SDL_Quit();
            return -1;
        }
        textures[i] = SDL_CreateTexture(renderers[i],
            SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
            width_, height_);
        if (!textures[i]) {
            std::cerr << "纹理创建失败: " << SDL_GetError() << std::endl;
            SDL_DestroyRenderer(renderers[i]);
            SDL_DestroyWindow(windows[i]);
            SDL_Quit();
            return -1;
        } 
   }
    // 主循环
    bool quit = false;
    SDL_Event event;
    std::vector<uint8_t> frameBuffer;
    cv::Mat RGBFrame;
    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } 
            // 添加 ESC 按键检测
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;  // 按下ESC键退出
                }
            }
        }

        // 捕获帧
        if (!capture.capture_frame(frameBuffer)) {
            std::cerr << "帧捕获失败" << std::endl;
            continue;
        }

        // 处理帧
        if (!processor.Decode2RGB(frameBuffer, RGBFrame)) {
            std::cerr << "解码失败" << std::endl;
            continue;
        }
    
        // 更新原始窗口
        SDL_UpdateTexture(textures[0], NULL, RGBFrame.data, RGBFrame.step);
        SDL_RenderClear(renderers[0]);
        SDL_RenderCopy(renderers[0], textures[0], NULL, NULL);
        SDL_RenderPresent(renderers[0]);

        // 处理并更新处理窗口
        processor.apply_algorithm(RGBFrame);
        if (processor.Decode2RGB(frameBuffer, RGBFrame)) {
            processor.apply_algorithm(RGBFrame);
            SDL_UpdateTexture(textures[1], NULL, RGBFrame.data, RGBFrame.step);
            SDL_RenderClear(renderers[1]);
            SDL_RenderCopy(renderers[1], textures[1], NULL, NULL);
            SDL_RenderPresent(renderers[1]);
        }
    }

    // 清理资源
    for (int i = 0; i < window_number; ++i) {
        SDL_DestroyTexture(textures[i]);
        SDL_DestroyRenderer(renderers[i]);
        SDL_DestroyWindow(windows[i]);
    }
    SDL_Quit();
    return 0;
}