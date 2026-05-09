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

struct EdgeFunction
{
    double stepX;
    double stepY;
    double constant;

    // 2D 边函数可以写成 E(x, y) = A*x + B*y + C。
    // 对三角形来说，它既能判断点在边的哪一侧，也能作为重心坐标分子的有向面积。
    EdgeFunction(const vec2& a, const vec2& b)
      : stepX(a.y - b.y)
      , stepY(b.x - a.x)
      , constant(a.x * b.y - a.y * b.x)
    {}

    double eval(double x, double y) const { return stepX * x + stepY * y + constant; }
};

// Top-left rule keeps shared triangle edges from being rasterized twice.
bool isTopLeftEdge(const vec2& a, const vec2& b)
{
    const vec2 edge = b - a;
    return edge.y > 0 || (edge.y == 0 && edge.x < 0);
}

struct EdgeCoverage
{
    EdgeFunction edge;
    bool topLeft;

    // 三角形绕序可能是顺时针也可能是逆时针，top-left 规则需要跟绕序一起解释。
    EdgeCoverage(const vec2& a, const vec2& b, bool positiveArea)
      : edge(a, b)
      , topLeft(positiveArea ? isTopLeftEdge(a, b) : isTopLeftEdge(b, a))
    {}

    double eval(double x, double y) const { return edge.eval(x, y); }
    double stepX() const { return edge.stepX; }
    double stepY() const { return edge.stepY; }
    // The signed edge value flips with winding, so the caller normalizes the sign first.
    bool contains(double signedValue) const { return signedValue > 0 || (signedValue == 0 && topLeft); }
};

struct PerspectivePlane
{
    double value;
    double stepX;
    double stepY;
};

PerspectivePlane makePerspectivePlane(const vec3& vertexValues, const vec3& screenBaryStepX, const vec3& screenBaryStepY,
                                      const vec3& sampleStartBary)
{
    // 透视除法之后，真正保持屏幕空间线性的不是属性本身，而是像 1/w、z/w 这类量。
    // 因此我们先在起始采样点求值，再记录沿 x/y 每移动一个像素时的固定增量。
    return {
        glm::dot(vertexValues, sampleStartBary),
        glm::dot(vertexValues, screenBaryStepX),
        glm::dot(vertexValues, screenBaryStepY),
    };
}

double edgeArea(const vec2 pts[3])
{
    return EdgeFunction(pts[1], pts[2]).eval(pts[0].x, pts[0].y);
}

struct ClipVertex
{
    // clipPos 仍然处在齐次裁剪空间；bary 记录这个顶点相对“原始三角形”的重心坐标。
    // 裁剪生成的新顶点也会沿边插值出新的 bary，这样 fragment shader 接口不用改。
    vec4 clipPos;
    vec3 bary;
};

struct ClipLineVertex
{
    // 线段只需要一个一维参数 bary：0 表示起点，1 表示终点。
    vec4 clipPos;
    double bary;
};

vec3 toOriginalTriangleBary(const ClipVertex tri[3], const vec3& clippedTriangleBary)
{
    const glm::mat3 clippedVertexBary{ tri[0].bary, tri[1].bary, tri[2].bary };
    return clippedVertexBary * clippedTriangleBary;
}

struct ClipPlane
{
    // 对 OpenGL 风格的 clip space，视锥可以写成 6 个 f(v) >= 0 的平面约束。
    double (*distance)(const vec4&);
};

double clipLeft(const vec4& v) { return v.x + v.w; }
double clipRight(const vec4& v) { return v.w - v.x; }
double clipBottom(const vec4& v) { return v.y + v.w; }
double clipTop(const vec4& v) { return v.w - v.y; }
double clipNear(const vec4& v) { return v.z + v.w; }
double clipFar(const vec4& v) { return v.w - v.z; }

bool insideClipPlane(const ClipVertex& v, const ClipPlane& plane)
{
    return plane.distance(v.clipPos) >= 0.0;
}

bool insideClipPlane(const ClipLineVertex& v, const ClipPlane& plane)
{
    return plane.distance(v.clipPos) >= 0.0;
}

ClipVertex intersectClipPlane(const ClipVertex& a, const ClipVertex& b, const ClipPlane& plane)
{
    // clip space 中平面函数和边插值都是线性的，所以交点直接按参数 t 线性求解即可。
    const double da = plane.distance(a.clipPos);
    const double db = plane.distance(b.clipPos);
    const double t = da / (da - db);
    return {
        a.clipPos + (b.clipPos - a.clipPos) * (float)t,
        a.bary + (b.bary - a.bary) * (float)t,
    };
}

ClipLineVertex intersectClipPlane(const ClipLineVertex& a, const ClipLineVertex& b, const ClipPlane& plane)
{
    const double da = plane.distance(a.clipPos);
    const double db = plane.distance(b.clipPos);
    const double t = da / (da - db);
    return {
        a.clipPos + (b.clipPos - a.clipPos) * (float)t,
        a.bary + (b.bary - a.bary) * t,
    };
}

std::vector<ClipVertex> clipPolygonAgainstPlane(const std::vector<ClipVertex>& polygon, const ClipPlane& plane)
{
    std::vector<ClipVertex> output;
    output.reserve(4);

    if (polygon.empty()) {
        return output;
    }

    ClipVertex previous = polygon.back();
    bool previousInside = insideClipPlane(previous, plane);
    for (const ClipVertex& current : polygon) {
        const bool currentInside = insideClipPlane(current, plane);

        // 这里是标准的 Sutherland-Hodgman 多边形裁剪：
        // 1. 一条边跨越平面时，补一个交点
        // 2. 当前点在平面内时，保留当前点
        if (currentInside != previousInside) {
            output.push_back(intersectClipPlane(previous, current, plane));
        }
        if (currentInside) {
            output.push_back(current);
        }

        previous = current;
        previousInside = currentInside;
    }

    return output;
}

bool clipLineAgainstPlane(ClipLineVertex& a, ClipLineVertex& b, const ClipPlane& plane)
{
    // 线段裁剪比多边形简单：两端都在内则保留，都在外则丢弃，一内一外就把外侧端点推进到交点。
    const bool aInside = insideClipPlane(a, plane);
    const bool bInside = insideClipPlane(b, plane);
    if (aInside && bInside) {
        return true;
    }
    if (!aInside && !bInside) {
        return false;
    }

    const ClipLineVertex clipped = intersectClipPlane(a, b, plane);
    if (!aInside) {
        a = clipped;
    }
    else {
        b = clipped;
    }
    return true;
}

std::vector<ClipVertex> clipPolygonAgainstFrustum(const ClipVertex tri[3])
{
    // 依次对 left/right/bottom/top/near/far 六个平面裁剪。
    // 每裁完一个平面，输出多边形再作为下一个平面的输入。
    std::vector<ClipVertex> polygon{ tri, tri + 3 };
    constexpr ClipPlane planes[] = {
        { clipLeft },
        { clipRight },
        { clipBottom },
        { clipTop },
        { clipNear },
        { clipFar },
    };

    for (const ClipPlane& plane : planes) {
        polygon = clipPolygonAgainstPlane(polygon, plane);
        if (polygon.size() < 3) {
            break;
        }
    }

    return polygon;
}

bool clipLineAgainstFrustum(ClipLineVertex line[2])
{
    // 线段路径和三角形路径保持同一套视锥定义，这样边界行为更一致。
    constexpr ClipPlane planes[] = {
        { clipLeft },
        { clipRight },
        { clipBottom },
        { clipTop },
        { clipNear },
        { clipFar },
    };

    for (const ClipPlane& plane : planes) {
        if (!clipLineAgainstPlane(line[0], line[1], plane)) {
            return false;
        }
    }
    return true;
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
        vec4 clipV0 = _shader->vs(_model->vertex(vert[0]));
        _shader->_vertexID = 1;
        vec4 clipV1 = _shader->vs(_model->vertex(vert[1]));

        ClipLineVertex lineVertices[2] = {
            { clipV0, 0.0 },
            { clipV1, 1.0 },
        };
        if (!clipLineAgainstFrustum(lineVertices)) {
            return;
        }

        // Clip before perspective divide so the screen-space line segment stays finite around the frustum edges.
        const vec4 pV0 = _viewport * lineVertices[0].clipPos;
        const vec4 pV1 = _viewport * lineVertices[1].clipPos;

        vec2 pts[2] = {
            { pV0[0] / pV0[3], pV0[1] / pV0[3] },
            { pV1[0] / pV1[3], pV1[1] / pV1[3] },
        };
        const vec2 line = pts[1] - pts[0];
        const double lengthSquared = glm::dot(line, line);
        if (lengthSquared < 1e-6) {
            return;
        }

        const EdgeFunction edge{ pts[0], pts[1] };
        const double lineLength = std::sqrt(lengthSquared);
        const int minX = std::max((int)std::floor(std::min(pts[0].x, pts[1].x) - 0.5), 0);
        const int maxX = std::min((int)std::ceil(std::max(pts[0].x, pts[1].x) + 0.5), _frame->width() - 1);
        const int minY = std::max((int)std::floor(std::min(pts[0].y, pts[1].y) - 0.5), 0);
        const int maxY = std::min((int)std::ceil(std::max(pts[0].y, pts[1].y) + 0.5), _frame->height() - 1);

        if (minX > maxX || minY > maxY) {
            return;
        }

        // 这里把线段看成“围绕隐式直线的一条 1 像素宽条带”：
        // 1. 用 edgeValue 判断像素中心到直线的带符号距离
        // 2. 用 t 判断像素投影是否落在线段两个端点之间
        // 3. 两个条件都满足时再执行 fragment shader
#pragma omp parallel for
        for (int y = minY; y <= maxY; y++) {
            double edgeValue = edge.eval(minX + 0.5, y + 0.5);
            for (int x = minX; x <= maxX; x++) {
                const vec2 sample{ x + 0.5, y + 0.5 };
                const double t = glm::dot(sample - pts[0], line) / lengthSquared;
                if (t >= 0.0 && t <= 1.0 && std::abs(edgeValue) <= 0.5 * lineLength) {
                    const double bary = lineVertices[0].bary + (lineVertices[1].bary - lineVertices[0].bary) * t;
                    vec4 fsColor;
                    if (!_shader->fs(vec3{ 1.0 - bary, bary, 0.0 }, fsColor)) {
                        fsColor = fsColor * 255.0f;
                        jrender::Color color{ (uint8_t)fsColor[0], (uint8_t)fsColor[1], (uint8_t)fsColor[2],
                                              (uint8_t)fsColor[3] };
                        _frame->setPixel(x, y, color);
                    }
                }
                edgeValue += edge.stepX;
            }
        }
    }

    void drawTriangle(int primID, int vert[3])
    {
        _shader->_primType = PrimitiveType::Triangle;
        _shader->_primID = primID;

        _shader->_vertexID = 0;
        vec4 clipV0 = _shader->vs(_model->vertex(vert[0]));

        _shader->_vertexID = 1;
        vec4 clipV1 = _shader->vs(_model->vertex(vert[1]));

        _shader->_vertexID = 2;
        vec4 clipV2 = _shader->vs(_model->vertex(vert[2]));

        // 先在齐次裁剪空间里做完整视锥裁剪，再做透视除法。
        // 如果反过来先做 x/w、y/w，跨近平面的三角形会出现极大的数值问题和错误包围盒。
        const ClipVertex clipTriangle[3] = {
            { clipV0, vec3{ 1.0, 0.0, 0.0 } },
            { clipV1, vec3{ 0.0, 1.0, 0.0 } },
            { clipV2, vec3{ 0.0, 0.0, 1.0 } },
        };
        const std::vector<ClipVertex> clippedPolygon = clipPolygonAgainstFrustum(clipTriangle);
        if (clippedPolygon.size() < 3) {
            return;
        }

        for (size_t i = 1; i + 1 < clippedPolygon.size(); i++) {
            // 裁剪后的多边形始终保持边界顶点顺序，因此可以用扇形拆分重新生成三角形。
            ClipVertex clippedTri[3] = {
                clippedPolygon[0],
                clippedPolygon[i],
                clippedPolygon[i + 1],
            };
            rasterizeTriangle(clippedTri);
        }
    }

    void rasterizeTriangle(const ClipVertex tri[3])
    {
        const vec4 pV0 = _viewport * tri[0].clipPos;
        const vec4 pV1 = _viewport * tri[1].clipPos;
        const vec4 pV2 = _viewport * tri[2].clipPos;

        vec2 pts[3] = { vec2(pV0 / pV0[3]), vec2(pV1 / pV1[3]), vec2(pV2 / pV2[3]) };
        const double triangleArea = edgeArea(pts);
        if (std::abs(triangleArea) < 1e-3) {
            return;
        }

        // areaSign 统一了顺时针/逆时针两种顶点顺序，
        // 后面 inside test 只需要比较“归一化后的符号”即可。
        const bool positiveArea = triangleArea > 0;
        const double areaSign = positiveArea ? 1.0 : -1.0;
        const double invTriangleArea = 1.0 / triangleArea;
        const EdgeCoverage w0Edge{ pts[1], pts[2], positiveArea };
        const EdgeCoverage w1Edge{ pts[2], pts[0], positiveArea };
        const EdgeCoverage w2Edge{ pts[0], pts[1], positiveArea };
        // 透视正确插值的关键不是直接插值属性，而是插值 1/w 和 z/w。
        // 最终像素上的 bary 需要再乘回 invWAtPixel 才能恢复成真正的透视矫正重心坐标。
        const vec3 invClipW{ 1.0 / tri[0].clipPos.w, 1.0 / tri[1].clipPos.w, 1.0 / tri[2].clipPos.w };
        const vec3 depthOverW{ tri[0].clipPos.z / tri[0].clipPos.w, tri[1].clipPos.z / tri[1].clipPos.w,
                               tri[2].clipPos.z / tri[2].clipPos.w };
        const vec3 screenBaryStepX{ w0Edge.stepX() * invTriangleArea, w1Edge.stepX() * invTriangleArea,
                                    w2Edge.stepX() * invTriangleArea };
        const vec3 screenBaryStepY{ w0Edge.stepY() * invTriangleArea, w1Edge.stepY() * invTriangleArea,
                                    w2Edge.stepY() * invTriangleArea };

        int minX = std::min({ pts[0].x, pts[1].x, pts[2].x });
        int maxX = std::max({ pts[0].x, pts[1].x, pts[2].x });
        int minY = std::min({ pts[0].y, pts[1].y, pts[2].y });
        int maxY = std::max({ pts[0].y, pts[1].y, pts[2].y });
        const int startX = std::max(minX, 0);
        const int endX = std::min(maxX, _frame->width() - 1);
        const int startY = std::max(minY, 0);
        const int endY = std::min(maxY, _frame->height() - 1);

        if (startX > endX || startY > endY) {
            return;
        }

        // 使用像素中心采样，边界规则更稳定，也更接近常见图形 API 的定义。
        const double sampleStartX = startX + 0.5;
        const double sampleStartY = startY + 0.5;
        const vec3 sampleStartBary{ w0Edge.eval(sampleStartX, sampleStartY) * invTriangleArea,
                                    w1Edge.eval(sampleStartX, sampleStartY) * invTriangleArea,
                                    w2Edge.eval(sampleStartX, sampleStartY) * invTriangleArea };
        const PerspectivePlane invWPlane =
          makePerspectivePlane(invClipW, screenBaryStepX, screenBaryStepY, sampleStartBary);
        const PerspectivePlane depthOverWPlane =
          makePerspectivePlane(depthOverW, screenBaryStepX, screenBaryStepY, sampleStartBary);

#pragma omp parallel for
        for (int y = startY; y <= endY; y++) {
            double w0 = w0Edge.eval(sampleStartX, y + 0.5);
            double w1 = w1Edge.eval(sampleStartX, y + 0.5);
            double w2 = w2Edge.eval(sampleStartX, y + 0.5);
            double invWAtPixel = invWPlane.value + (y - startY) * invWPlane.stepY;
            double depthOverWAtPixel = depthOverWPlane.value + (y - startY) * depthOverWPlane.stepY;

            for (int x = startX; x <= endX; x++) {
                const double signedW0 = w0 * areaSign;
                const double signedW1 = w1 * areaSign;
                const double signedW2 = w2 * areaSign;
                // 三条边同时满足覆盖规则，像素才处在三角形内部。
                if (!w0Edge.contains(signedW0) || !w1Edge.contains(signedW1) || !w2Edge.contains(signedW2)) {
                    w0 += w0Edge.stepX();
                    w1 += w1Edge.stepX();
                    w2 += w2Edge.stepX();
                    invWAtPixel += invWPlane.stepX;
                    depthOverWAtPixel += depthOverWPlane.stepX;
                    continue;
                }

                const vec3 bc_screen{ w0 * invTriangleArea, w1 * invTriangleArea, w2 * invTriangleArea };
                const double depth = depthOverWAtPixel / invWAtPixel;
                if (depth > _zbuffer[y * _frame->width() + x]) {
                    w0 += w0Edge.stepX();
                    w1 += w1Edge.stepX();
                    w2 += w2Edge.stepX();
                    invWAtPixel += invWPlane.stepX;
                    depthOverWAtPixel += depthOverWPlane.stepX;
                    continue;
                }

                // Convert screen-space barycentrics back into perspective-correct barycentrics before shading.
                const vec3 bc_perspective = (bc_screen * invClipW) / (float)invWAtPixel;
                // tri[i].bary 记录的是“裁剪后三角形顶点在原始三角形里的位置”，
                // 所以这里再做一次组合，就能把像素重新映射回原始三角形的 bary。
                const vec3 originalTriangleBary = toOriginalTriangleBary(tri, bc_perspective);
                vec4 fsColor;
                if (!_shader->fs(originalTriangleBary, fsColor)) {
                    _zbuffer[y * _frame->width() + x] = depth;
                    fsColor = fsColor * 255.0f;
                    Color color{ (uint8_t)fsColor[0], (uint8_t)fsColor[1], (uint8_t)fsColor[2], (uint8_t)fsColor[3] };
                    _frame->setPixel(x, y, color);
                }

                w0 += w0Edge.stepX();
                w1 += w1Edge.stepX();
                w2 += w2Edge.stepX();
                invWAtPixel += invWPlane.stepX;
                depthOverWAtPixel += depthOverWPlane.stepX;
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
