#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <format>

#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace jrender {

using glm::vec2;
using glm::vec3;
using glm::vec4;

enum class PrimitiveType { Point, Line, Triangle };
enum class Format { GRAYSCALE = 1, RGB = 3, RGBA = 4, BGRA = 5 };

struct Color
{
    union {
        uint8_t color[4];
        struct
        {
            uint8_t r, g, b, a;
        };
    };
};

std::vector<vec2> linePoints(vec2&& p0, vec2&& p1)
{
    std::vector<vec2> pts;

    bool steep = false;
    if (std::abs(p0.x - p1.x) < std::abs(p0.y - p1.y)) {
        std::swap(p0.x, p0.y);
        std::swap(p1.x, p1.y);
        steep = true;
    }
    if (p0.x > p1.x) {
        std::swap(p0.x, p1.x);
        std::swap(p0.y, p1.y);
    }
    int dx = p1.x - p0.x;
    int dy = p1.y - p0.y;
    int derror2 = std::abs(dy) * 2;
    int error2 = 0;
    int y = p0.y;
    for (int x = p0.x; x <= p1.x; x++) {
        if (steep) {
            pts.emplace_back(vec2{ (double)y, (double)x });
        }
        else {
            pts.emplace_back(vec2{ (double)x, (double)y });
        }
        error2 += derror2;
        if (error2 > dx) {
            y += (p1.y > p0.y ? 1 : -1);
            error2 -= dx * 2;
        }
    }

    return pts;
}

constexpr int PrimVertexCount(PrimitiveType prim)
{
    switch (prim) {
    case PrimitiveType::Point:
        return 1;
    case PrimitiveType::Line:
        return 2;
    case PrimitiveType::Triangle:
        return 3;
    default:
        return 3;
    }
}

constexpr int FormatSize(Format format)
{
    switch (format) {
    case Format::GRAYSCALE:
        return 1;
    case Format::RGB:
        return 3;
    case Format::BGRA:
    case Format::RGBA:
        return 4;
    default:
        return 4;
    }
}

class Image
{
public:
    Image() = default;

    Image(const char* imgPath) { loadImage(imgPath); }

    Image(int w, int h, Format format) : _format(format), _width(w), _height(h)
    {
        _pixels.resize(w * h * FormatSize(_format));
    }

    ~Image() {}

    void setFlipVertical(bool flip) { _flipVertical = flip; }

    void loadImage(const char* filePath)
    {
        int channels;
        stbi_set_flip_vertically_on_load(_flipVertical);
        u_int8_t* data = stbi_load(filePath, &_width, &_height, &channels, 0);
        if (data == nullptr) {
            std::printf("load %s failed!\n", filePath);
            return;
        }

        _format = (Format)channels;

        int len = _width * _height * FormatSize(_format);
        _pixels.assign(data, data + len);

        stbi_image_free(data);
    }

    void setPixel(int x, int y, const Color& c)
    {
        y = _flipVertical ? (_height - 1 - y) : y;
        int index = (y * _width + x) * FormatSize(_format);
        switch (_format) {
        case Format::BGRA: {
            _pixels[index] = c.b;
            _pixels[index + 1] = c.g;
            _pixels[index + 2] = c.r;
            _pixels[index + 3] = c.a;
        } break;
        case Format::RGBA: {
            _pixels[index] = c.r;
            _pixels[index + 1] = c.g;
            _pixels[index + 2] = c.b;
            _pixels[index + 3] = c.a;
        } break;
        default:
            break;
        }
    }

    Color pixel(int x, int y) const
    {
        if (!_pixels.size() || x < 0 || y < 0 || x >= _width || y >= _height) return {};

        Color ret{ 0 };

        int pSize = FormatSize(_format);
        const uint8_t* p = _pixels.data() + (x + y * _width) * pSize;
        for (int i = pSize; i--; ret.color[i] = p[i])
            ;
        return ret;
    }

    int width() const { return _width; }
    int height() const { return _height; }
    int size() const { return _pixels.size(); }

    char* data() { return (char*)_pixels.data(); }

    void clear() { std::fill(_pixels.begin(), _pixels.end(), 0); }

private:
    bool _flipVertical{ false };
    Format _format;
    int _width;
    int _height;

    std::vector<uint8_t> _pixels;
};
using ImagePtr = std::shared_ptr<Image>;

class Shader
{
public:
    virtual ~Shader() {}

    static vec4 sample2D(const Image& img, vec2& uvf)
    {
        Color c = img.pixel(uvf[0] * img.width(), uvf[1] * img.height());
        return vec4(c.color[0] / 255.f, c.color[1] / 255.f, c.color[2] / 255.f, c.color[3] / 255.f);
    }
    virtual vec4 vs(vec3&& pos) = 0;
    virtual bool fs(const vec3& bary, vec4& fragColor) = 0;

    PrimitiveType _primType;
    uint8_t _vertexID;
    uint32_t _primID;
};
using ShaderPtr = std::shared_ptr<Shader>;

class Model
{
public:
    Model() {}
    ~Model() {}

    void loadModel(const std::string& filename)
    {
        std::ifstream in;
        in.open(filename, std::ifstream::in);
        if (in.fail()) return;
        std::string line;
        while (!in.eof()) {
            std::getline(in, line);
            std::istringstream iss(line.c_str());
            char trash;
            if (!line.compare(0, 2, "v ")) {
                iss >> trash;
                vec3 v;
                for (int i = 0; i < 3; i++)
                    iss >> v[i];

                _vertices.push_back(v);
            }
            else if (!line.compare(0, 3, "vn ")) {
                iss >> trash >> trash;
                vec3 n;
                for (int i = 0; i < 3; i++)
                    iss >> n[i];
                _norms.push_back(glm::normalize(n));
            }
            else if (!line.compare(0, 3, "vt ")) {
                iss >> trash >> trash;
                vec2 uv;
                for (int i = 0; i < 2; i++)
                    iss >> uv[i];
                _texCoords.push_back({ uv.x, 1 - uv.y });
            }
            else if (!line.compare(0, 2, "f ")) {
                int f, t, n;
                iss >> trash;
                int cnt = 0;
                while (iss >> f >> trash >> t >> trash >> n) {
                    _vertIndices.push_back(--f);
                    _texIndices.push_back(--t);
                    _normIndices.push_back(--n);
                    cnt++;
                }
                if (3 != cnt) {
                    std::cerr << "Error: the obj file is supposed to be triangulated" << std::endl;
                    return;
                }
            }
        }

        size_t dot = filename.find_last_of(".");
        if (dot == std::string::npos) return;
        std::string baseName = filename.substr(0, dot);

        _normalMap.loadImage(std::format("{}_nm_tangent.tga", baseName).c_str());
        _diffuseMap.loadImage(std::format("{}_diffuse.tga", baseName).c_str());
        _specularMap.loadImage(std::format("{}_spec.tga", baseName).c_str());
    }

    void setVertices(std::vector<vec3>&& vertices) { _vertices = std::move(vertices); }
    void setIndices(std::vector<int>&& indices) { _vertIndices = std::move(indices); }
    void setTexCoords(std::vector<vec2>&& texCoords) { _texCoords = std::move(texCoords); }

    int faces() const { return _vertIndices.size() / 3; }

    vec3 vertex(uint i) const
    {
        if (i < _vertices.size()) {
            return _vertices[i];
        }
        return {};
    }

    int vertexIndex(uint i) const
    {
        if (i < _vertIndices.size()) {

            return _vertIndices[i];
        }
        return -1;
    }

    vec2 texcoord(uint i) const
    {
        if (i < _texCoords.size()) {
            return _texCoords[i];
        }
        return {};
    }

    int texcoordIndex(uint i) const
    {
        if (i < _texIndices.size()) {
            return _texIndices[i];
        }
        return -1;
    }

    vec3 normal(uint i) const
    {
        if (i < _norms.size()) {
            return _norms[i];
        }
        return {};
    }

    vec3 normal(const vec2& uvf) const
    {
        Color c = _normalMap.pixel(uvf[0] * _normalMap.width(), uvf[1] * _normalMap.height());
        return vec3((double)c.color[0], (double)c.color[1], (double)c.color[2]) * 2.f / 255.f - vec3(1, 1, 1);
    }

    int normalIndex(uint i) const
    {
        if (i < _normIndices.size()) {
            return _normIndices[i];
        }
        return -1;
    }

    void setTexture(uint index, ImagePtr img)
    {
        if (index < _textures.size()) {
            _textures[index] = std::move(img);
        }
    }

    const ImagePtr texture(uint index) const
    {
        if (index < _textures.size()) {
            return _textures[index];
        }
        return nullptr;
    }

    const Image& diffuse() const { return _diffuseMap; }
    const Image& specular() const { return _specularMap; }

private:
    std::vector<vec3> _vertices;
    std::vector<vec2> _texCoords;
    std::vector<vec3> _norms;
    std::vector<int> _vertIndices;
    std::vector<int> _texIndices;
    std::vector<int> _normIndices;

    Image _diffuseMap;   // diffuse color texture
    Image _specularMap;  // specular map texture
    Image _normalMap;    // normal map texture

    std::array<ImagePtr, 10> _textures;
};
using ModelPtr = std::shared_ptr<Model>;

vec3 barycentricLine(const vec2 tri[2], const vec2 p)
{
    double a = glm::distance(p, tri[0]) / glm::distance(tri[0], tri[1]);
    return vec3{ 1 - a, a, 0 };
}

vec3 barycentric(const vec2 tri[3], const vec2& P)
{
    glm::mat3 ABC = { vec3(tri[0], 1.0), vec3(tri[1], 1.0), vec3(tri[2], 1.0) };

    // for a degenerate triangle generate negative coordinates, it will be thrown away by the rasterizator
    if (glm::determinant(ABC) < 1e-3) return { -1, 1, 1 };

    return glm::inverse(ABC) * vec3(P, 1.0);
}

class Render
{
public:
    Render(ImagePtr frame, ModelPtr model, ShaderPtr shader)
      : _frame(std::move(frame))
      , _model(std::move(model))
      , _shader(std::move(shader))
      , _zbuffer(_frame->width() * _frame->height(), std::numeric_limits<double>::max())
    {}

    ~Render() {}

    void setViewport(int x, int y, int w, int h)
    {
        _viewport = glm::mat4(1.0f);

        // 缩放 NDC 到窗口坐标的比例
        _viewport[0][0] = w / 2.0f;
        _viewport[1][1] = h / 2.0f;
        _viewport[2][2] = 1;

        // 平移到窗口坐标的偏移量
        _viewport[3][0] = x + w / 2.0f;
        _viewport[3][1] = y + h / 2.0f;
        _viewport[3][2] = 0;
    }

    void setModel(ModelPtr model) { _model = std::move(model); }
    void setShader(ShaderPtr shader) { _shader = std::move(shader); }

    const std::vector<double>& zbuffer() const { return _zbuffer; }

    void drawArray(PrimitiveType mode, int start, int vertexCount)
    {
        if (mode == PrimitiveType::Triangle) {
            int priCount = vertexCount / 3;
            for (int i = 0; i < priCount; i++) {
                int vert[3] = { start + i * 3, start + i * 3 + 1, start + i * 3 + 2 };
                drawTriangle(i, vert);
            }
        }
        else if (mode == PrimitiveType::Line) {
            int priCount = vertexCount / 2;
            for (int i = 0; i < priCount; i++) {
                int vert[2] = { start + i * 2, start + i * 2 + 1 };
                drawLine(i, vert);
            }
        }
        else if (mode == PrimitiveType::Point) {
            for (int i = 0; i < vertexCount; i++) {
                int vert = start + i;
                drawPoint(i, vert);
            }
        }
    }

    void drawIndex(PrimitiveType mode, int start, int indexCount)
    {
        if (mode == PrimitiveType::Triangle) {
            int priCount = indexCount / 3;
            for (int i = 0; i < priCount; i++) {
                int vert[3] = { _model->vertexIndex(start + i * 3), _model->vertexIndex(start + i * 3 + 1),
                                _model->vertexIndex(start + i * 3 + 2) };
                drawTriangle(i, vert);
            }
        }
        else if (mode == PrimitiveType::Line) {
            int priCount = indexCount / 2;
            for (int i = 0; i < priCount; i++) {
                int vert[2] = { _model->vertexIndex(start + i * 2), _model->vertexIndex(start + i * 2 + 1) };
                drawLine(i, vert);
            }
        }
        else if (mode == PrimitiveType::Point) {
            for (int i = 0; i < indexCount; i++) {
                int vert = _model->vertexIndex(start + i);
                drawPoint(i, vert);
            }
        }
    }

    void clear()
    {
        std::fill(_zbuffer.begin(), _zbuffer.end(), std::numeric_limits<double>::max());
        _frame->clear();
    }

private:
    void drawPoint(int primID, int vert)
    {
        _shader->_primType = PrimitiveType::Point;
        _shader->_primID = primID;

        _shader->_vertexID = 0;
        vec4 pV = _viewport * _shader->vs(_model->vertex(vert));
        vec2 pt{ pV[0] / pV[3], pV[1] / pV[3] };

        vec4 fsColor;
        if (!_shader->fs(vec3{ 1.0, 0.0, 0.0 }, fsColor)) {
            fsColor = fsColor * 255.0f;
            jrender::Color color{ (uint8_t)fsColor[0], (uint8_t)fsColor[1], (uint8_t)fsColor[2], (uint8_t)fsColor[3] };
            _frame->setPixel((int)pt.x, (int)pt.y, color);
        }
    }

    void drawLine(int primID, int vert[2])
    {
        _shader->_primType = PrimitiveType::Line;
        _shader->_primID = primID;

        _shader->_vertexID = 0;
        vec4 pV0 = _viewport * _shader->vs(_model->vertex(vert[0]));
        _shader->_vertexID = 1;
        vec4 pV1 = _viewport * _shader->vs(_model->vertex(vert[1]));

        vec2 pts[2] = {
            { pV0[0] / pV0[3], pV0[1] / pV0[3] },
            { pV1[0] / pV1[3], pV1[1] / pV1[3] },
        };

#pragma omp parallel for
        for (const auto& p : linePoints(vec2{ pts[0].x, pts[0].y }, vec2{ pts[1].x, pts[1].y })) {
            vec4 fsColor;
            if (!_shader->fs(barycentricLine(pts, p), fsColor)) {
                fsColor = fsColor * 255.0f;
                jrender::Color color{ (uint8_t)fsColor[0], (uint8_t)fsColor[1], (uint8_t)fsColor[2],
                                      (uint8_t)fsColor[3] };
                _frame->setPixel(p.x, p.y, color);
            }
        }
    }

    void drawTriangle(int primID, int vert[3])
    {
        _shader->_primType = PrimitiveType::Triangle;
        _shader->_primID = primID;

        _shader->_vertexID = 0;
        vec4 pV0 = _viewport * _shader->vs(_model->vertex(vert[0]));

        _shader->_vertexID = 1;
        vec4 pV1 = _viewport * _shader->vs(_model->vertex(vert[1]));

        _shader->_vertexID = 2;
        vec4 pV2 = _viewport * _shader->vs(_model->vertex(vert[2]));

        vec2 pts[3] = { vec2(pV0 / pV0[3]), vec2(pV1 / pV1[3]), vec2(pV2 / pV2[3]) };

        int minX = std::min({ pts[0].x, pts[1].x, pts[2].x });
        int maxX = std::max({ pts[0].x, pts[1].x, pts[2].x });
        int minY = std::min({ pts[0].y, pts[1].y, pts[2].y });
        int maxY = std::max({ pts[0].y, pts[1].y, pts[2].y });

#pragma omp parallel for
        for (int x = std::max(minX, 0); x <= std::min(maxX, _frame->width() - 1); x++) {
            for (int y = std::max(minY, 0); y <= std::min(maxY, _frame->height() - 1); y++) {
                vec3 bc_screen = barycentric(pts, vec2{ (double)x, (double)y });
                double depth = glm::dot(vec3(pV0.z, pV1.z, pV2.z), bc_screen);
                if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0
                    || depth > _zbuffer[y * _frame->width() + x]) {
                    continue;
                }

                vec4 fsColor;
                if (!_shader->fs(bc_screen, fsColor)) {
                    _zbuffer[y * _frame->width() + x] = depth;
                    fsColor = fsColor * 255.0f;
                    Color color{ (uint8_t)fsColor[0], (uint8_t)fsColor[1], (uint8_t)fsColor[2], (uint8_t)fsColor[3] };
                    _frame->setPixel(x, y, color);
                }
            }
        }
    }

private:
    ImagePtr _frame;
    ModelPtr _model;
    ShaderPtr _shader;
    glm::mat4 _viewport;

    std::vector<double> _zbuffer;
};

}  // namespace jrender