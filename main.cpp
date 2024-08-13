#include <unordered_map>
#include <algorithm>

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "render.hpp"

namespace jrender {

void DrawLine(Display* display, Window window, GC gc, const Point& p0, const Point& p1)
{
    for (const auto& p : linePoints(Point { p0 }, Point { p1 })) {
        XDrawPoint(display, window, gc, p.x, p.y); // 使用图形上下文绘制点
    }
}

void DrawTriangle(Display* display, Window window, GC gc, const Point& p0, const Point& p1, const Point& p2)
{
    std::unordered_map<int, Point[2]> pts_map;
    for (auto&& p : linePoints(Point { p0 }, Point { p1 })) {
        if (pts_map.find(p.y) == pts_map.end()) {
            pts_map[p.y][0] = std::move(p);
        } else {
            pts_map[p.y][1] = std::move(p);
        }
    }
    for (auto& p : linePoints(Point { p1 }, Point { p2 })) {
        if (pts_map.find(p.y) == pts_map.end()) {
            pts_map[p.y][0] = std::move(p);
        } else {
            pts_map[p.y][1] = std::move(p);
        }
    }
    for (auto& p : linePoints(Point { p2 }, Point { p0 })) {
        if (pts_map.find(p.y) == pts_map.end()) {
            pts_map[p.y][0] = std::move(p);
        } else {
            pts_map[p.y][1] = std::move(p);
        }
    }

    int minY = std::min({ p0.y, p1.y, p2.y });
    int maxY = std::max({ p0.y, p1.y, p2.y });

    for (int i = minY; i <= maxY; i++) {
        auto it = pts_map.find(i);
        if (it == pts_map.end()) {
            std::printf("error y\n");
            return;
        }

        if (it->second[1].x == 0) {
            XDrawPoint(display, window, gc, it->second[0].x, it->second[0].y); // 使用图形上下文绘制点
        } else {
            DrawLine(display, window, gc, it->second[0], it->second[1]);
        }
    }

    // DrawLine(display, window, gc, p0, p1);
    // DrawLine(display, window, gc, p1, p2);
    // DrawLine(display, window, gc, p2, p0);
}

} // namespace jrender

int main(int argc, char const* argv[])
{
    Display* display;
    Window window;
    XEvent event;
    int screen;

    // 打开与 X 服务器的连接
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "无法打开显示\n");
        exit(1);
    }

    // 获取默认屏幕
    screen = DefaultScreen(display);

    // 创建一个窗口
    window = XCreateSimpleWindow(display,
        RootWindow(display, screen),
        10,
        10,
        400,
        400,
        1,
        BlackPixel(display, screen),
        WhitePixel(display, screen));

    // 选择事件类型
    XSelectInput(display, window, ExposureMask | KeyPressMask);

    // 显示窗口
    XMapWindow(display, window);

    // 创建图形上下文
    GC gc = XCreateGC(display, window, 0, NULL);
    if (gc == NULL) {
        fprintf(stderr, "无法创建图形上下文\n");
        XCloseDisplay(display);
        exit(1);
    }

    // 设置前景色（例如，红色）
    XSetForeground(display, gc, 0xFF0000); // 设置为红色

    using namespace jrender;

    // 进入事件循环
    while (1) {
        XNextEvent(display, &event); // 等待并获取下一个事件
        if (event.type == Expose) {
            // 窗口需要重绘时处理
            XClearWindow(display, window);

            // 在窗口中心绘制一个点
            DrawLine(display, window, gc, { 100, 100 }, { 200, 0 });
            DrawTriangle(display, window, gc, { 150, 150 }, { 200, 200 }, { 160, 180 });
        }
        if (event.type == KeyPress) {
            break; // 按下任意键退出循环
        }
    }

    // 清理资源
    XFreeGC(display, gc); // 释放图形上下文
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
