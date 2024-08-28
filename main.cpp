#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "render.hpp"
#include "geometry.h"

class MyShader : public jrender::Shader
{
public:
    MyShader() {}
    ~MyShader() {}

    virtual void vs(vec4& p) override {}

    virtual bool fs(const vec3& bar, vec4& color) override
    {
        vec3 vertexColor[3] = { vec3{ 1, 0, 0 }, vec3{ 0, 1, 0 }, vec3{ 0, 0, 1 } };
        vec3 c = vertexColor[0] * bar[0] + vertexColor[1] * bar[1] + vertexColor[2] * bar[2];
        color = embed<4>(c);
        return true;
    }

    vec4 vertex[3];
};

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
    Image frame(screenWidth, screenHeight, Format::RGBA);

    XImage* xImage = XCreateImage(display, DefaultVisual(display, 0), DefaultDepth(display, 0), ZPixmap, 0,
                                  frame.data(), screenWidth, screenHeight, 32, 0);

    ModelPtr vertices = std::make_shared<Model>();
    vertices->setVertices({ { -1, 0.5 }, { -0.3, -1 }, { 1, 0.3 }, { 0.5, 1 } });

    ShaderPtr shader = std::make_shared<MyShader>();
    Render    render(vertices, shader);
    render.setViewport(0, 0, screenWidth, screenHeight);

    render.drawArray(frame, PrimitiveMode::Point, 0, 2);
    render.drawArray(frame, PrimitiveMode::Point, 2, 2);
    render.drawArray(frame, PrimitiveMode::Line, 0, 2);
    render.drawArray(frame, PrimitiveMode::Line, 2, 2);
    render.drawArray(frame, PrimitiveMode::Triangle, 0, 4);

    ModelPtr model = std::make_shared<Model>();
    model->loadModel("diablo3_pose.obj");
    render.setModel(model);
    render.drawIndex(frame, PrimitiveMode::Triangle, 0, model->faces() * 3);

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
