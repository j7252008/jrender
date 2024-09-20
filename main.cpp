#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <glm/gtc/matrix_transform.hpp>
#include "render.hpp"

class ColorShader : public jrender::Shader
{
public:
    ColorShader(jrender::ModelPtr model) : _model(model) {}
    ~ColorShader() override {}

    virtual glm::vec4 vs(uint32_t primID, uint8_t vertexID, glm::vec3&& pos) override { return glm::vec4(pos, 1.0); }

    bool fs(const glm::vec3& bar, glm::vec4& fragColor) override
    {
        using namespace glm;

        constexpr glm::mat3 vertexColor{ glm::vec3{ 1, 0, 0 }, glm::vec3{ 0, 1, 0 }, glm::vec3{ 0, 0, 1 } };
        fragColor = glm::vec4(vertexColor * bar, 1.0);

        return false;  // not discarded
    }

    jrender::ModelPtr _model;
};

class TextureShader : public jrender::Shader
{
public:
    TextureShader(jrender::ModelPtr model) : _model(model) {}
    ~TextureShader() override {}

    virtual glm::vec4 vs(uint32_t primID, uint8_t vertexID, glm::vec3&& pos) override
    {
        _pos[vertexID] = std::move(pos);
        _uv[vertexID] = _model->texcoord(_model->texcoordIndex(primID * 3 + vertexID));

        return glm::vec4(_pos[vertexID], 1.0);
    }

    bool fs(const glm::vec3& bar, glm::vec4& fragColor) override
    {
        using namespace glm;

        vec2 uv = _uv * bar;
        fragColor = sample2D(*_model->texture(0), uv);

        return false;  // not discarded
    }

    glm::mat3x2 _uv;
    glm::mat3   _pos;

    jrender::ModelPtr _model;
};

glm::mat4 mvp;

class MyShader : public jrender::Shader
{
public:
    MyShader(jrender::ModelPtr model) : _model(model) {}
    ~MyShader() override {}

    virtual glm::vec4 vs(uint32_t primID, uint8_t vertexID, glm::vec3&& pos) override
    {

        glm::vec4 gPos = mvp * glm::vec4(pos, 1.f);

        _uv[vertexID] = _model->texcoord(_model->texcoordIndex(primID * 3 + vertexID));
        _norm[vertexID] = mvp * glm::vec4(_model->normal(_model->normalIndex(primID * 3 + vertexID)), 1.0);
        _pos[vertexID] = glm::vec3(gPos);
        return gPos;
    }

    bool fs(const glm::vec3& bar, glm::vec4& fragColor) override
    {
        using namespace glm;
        constexpr glm::vec3 lightColor{ 1, 1, 1 };  // light source
        constexpr glm::vec3 lightPos{ 0, 1, 5 };    // light source

        vec3 fragPos = _pos * bar;
        vec2 uv = _uv * bar;
        vec3 normal = _norm * bar;  //_model->normal(uv);

        // ambient
        float ambient = 0.1;
        vec3  ambientColor = ambient * lightColor;

        // diffuse
        vec3  norm = glm::normalize(normal);
        vec3  lightDir = glm::normalize(lightPos - fragPos);
        float diff = std::max(glm::dot(norm, lightDir), 0.0f);
        vec3  diffuseColor = diff * lightColor;

        fragColor = vec4((ambientColor + diffuseColor) * vec3(sample2D(_model->diffuse(), uv)), 1.0);

        return false;  // not discarded
    }

    glm::mat3x2 _uv;
    glm::mat3   _norm;
    glm::mat3   _pos;

    jrender::ModelPtr _model;
};

int main()
{
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 600;

    Display* display = XOpenDisplay(nullptr);
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
    GC gc = XCreateGC(display, window, 0, nullptr);

    using namespace jrender;
    ImagePtr frame = std::make_shared<Image>(screenWidth, screenHeight, Format::BGRA);

    XImage* xImage = XCreateImage(display, DefaultVisual(display, 0), DefaultDepth(display, 0), ZPixmap, 0,
                                  frame->data(), screenWidth, screenHeight, 32, 0);

    // color
    ModelPtr vertices = std::make_shared<Model>();
    vertices->setVertices(
      { glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.4, 0.4, 0.0), glm::vec3(0.4, 0.8, 0.0), glm::vec3(0.8, 0.8, 0.0) });

    ShaderPtr shader = std::make_shared<ColorShader>(vertices);

    Render render(frame, vertices, shader);
    render.setViewport(screenWidth / 8, screenHeight / 8, screenWidth * 3 / 4, screenHeight * 3 / 4);

    render.drawArray(PrimitiveMode::Point, 0, 2);
    render.drawArray(PrimitiveMode::Point, 2, 2);
    render.drawArray(PrimitiveMode::Line, 0, 2);
    render.drawArray(PrimitiveMode::Line, 2, 2);
    render.drawArray(PrimitiveMode::Triangle, 0, 4);

    // texture
    ModelPtr texModel = std::make_shared<Model>();
    vertices->setTexture(0, std::make_shared<Image>("awesomeface.png"));
    vertices->setTexCoords({ vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0) });
    vertices->setVertices(
      { glm::vec3(-1.0, 1.0, 0.0), glm::vec3(-1.0, -1.0, 0.0), glm::vec3(1.0, -1.0, 0.0), glm::vec3(1.0, 1.0, 0.0) });

    ShaderPtr texShader = std::make_shared<TextureShader>(vertices);
    render.setShader(texShader);
    render.setModel(texModel);

    render.drawArray(PrimitiveMode::Triangle, 0, 4);

    // model
    ModelPtr model = std::make_shared<Model>();
    model->loadModel("diablo3_pose/diablo3_pose.obj");
    ShaderPtr shaderD = std::make_shared<MyShader>(model);

    render.setShader(shaderD);
    render.setModel(model);

    // 事件循环
    auto   startT = std::chrono::high_resolution_clock::now();
    XEvent event;
    while (true) {
        XNextEvent(display, &event);
        if (event.type == KeyPress) {
            frame->clear();
            render.clear();

            glm::mat4 modelMat(1.f);
            glm::mat4 view(1.f);
            glm::mat4 proj(1.f);

            std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - startT;
            modelMat = glm::rotate(modelMat, (float)elapsed.count(), glm::vec3(0, 1, 0));
            view = glm::translate(view, glm::vec3(0, 0, -2));
            proj = glm::perspective(glm::radians(45.f), (float)800.f / 600.f, 0.1f, 100.f);

            mvp = proj * view * modelMat;

            // 将图像绘制到窗口
            render.drawIndex(PrimitiveMode::Triangle, 0, model->faces() * 3);
            std::copy(frame->data(), frame->data() + frame->size(), xImage->data);
            XPutImage(display, window, gc, xImage, 0, 0, 0, 0, screenWidth, screenHeight);
        }
    }

    // 清理
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
