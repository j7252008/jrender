#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "render.hpp"
#include "geometry.h"

int main()
{
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 600;

    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    Window root = DefaultRootWindow(display);
    Window window = XCreateSimpleWindow(display, root, 10, 10, screenWidth, screenHeight, 1, BlackPixel(display, 0),
                                        WhitePixel(display, 0));

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    XFlush(display);

    // 创建图形上下文
    GC gc = XCreateGC(display, window, 0, NULL);

    using namespace jrender;
    Image image(screenWidth, screenHeight, Format::RGBA);

    XImage* xImage = XCreateImage(display, DefaultVisual(display, 0), DefaultDepth(display, 0), ZPixmap, 0,
                                  image.data(), screenWidth, screenHeight, 32, 0);

    DrawPoint(image, vec2{ -0.8, 0.8}, Color{ 255, 0, 0, 255 });
    DrawLine(image, vec2{ -0.7, 0.7 }, vec2{ -0.6, 0.8 }, Color{ 0, 255, 0, 255 });
    DrawTriangle(image, vec3{ -0.5, 0.5 }, vec3{ -0.7, 0.3 }, vec3{ -0.3, 0.3 }, Color{ 255, 255, 0, 255 });

    Model model;
    model.loadModel("diablo3_pose.obj");
    for (int i = 0; i < model.faces(); i++) {  // for every triangle
        vec3 v0 = model.vertex(i, 0);
        vec3 v1 = model.vertex(i, 1);
        vec3 v2 = model.vertex(i, 2);
        DrawTriangle(image, v0, v1, v2, Color{ 255, 255, 255, 255 });
    }

    // 事件循环
    XEvent event;
    while (1) {
        XNextEvent(display, &event);
        if (event.type == Expose) {
            // 将图像绘制到窗口
            XPutImage(display, window, gc, xImage, 0, 0, 0, 0, screenWidth, screenHeight);
        }
        if (event.type == KeyPress) {
            break;  // 按任意键退出
        }
    }

    // 清理
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
