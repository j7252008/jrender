#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdint>
#include <memory>
#include <algorithm>

#include <glm/glm.hpp>

namespace jrender {

using glm::vec2;
using glm::vec3;
using glm::vec4;

struct Color
{
    union {
        uint8_t bgra[4];
        struct
        {
            uint8_t b, g, r, a;
        };
    };
};

enum class PrimitiveMode { Point, Line, Triangle };
enum class Format { BGRA, RGBA };

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

constexpr int FormatSize(Format format)
{
    switch (format) {
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
    Image(int w, int h, Format format) : _width(w), _height(h), _format(format)
    {
        _pixels.reserve(w * h * FormatSize(_format));
    }

    ~Image() {}

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

    int width() const { return _width; }
    int height() const { return _height; }

    char* data() { return (char*)_pixels.data(); }

private:
    bool   _flipVertical{ true };
    Format _format;
    int    _width;
    int    _height;

    std::vector<uint8_t> _pixels;
};

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
            char               trash;
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
                norms.push_back(glm::normalize(n));
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
                    _indices.push_back(--f);
                    facet_tex.push_back(--t);
                    facet_nrm.push_back(--n);
                    cnt++;
                }
                if (3 != cnt) {
                    std::cerr << "Error: the obj file is supposed to be triangulated" << std::endl;
                    return;
                }
            }
        }
        std::cout << "# v# " << _vertices.size() << " f# " << _indices.size() / 3 << " vt# " << _texCoords.size()
                  << " vn# " << norms.size() << std::endl;
    }

    void setVertices(std::vector<vec3>&& vertices) { _vertices = std::move(vertices); }
    void setIndices(std::vector<int>&& indices) { _indices = std::move(indices); }
    void setTexCoords(std::vector<vec2>&& texCoords) { _texCoords = std::move(texCoords); }

    int faces() const { return _indices.size() / 3; }

    vec3 vertex(int i) const { return _vertices[i]; }
    vec3 indexVertex(int index) const { return _vertices[_indices[index]]; }

private:
    std::vector<vec3> _vertices;   // array of vertices
    std::vector<vec2> _texCoords;  // per-vertex array of tex coords
    std::vector<vec3> norms;       // per-vertex array of normal vectors
    std::vector<int>  _indices;    // per-triangle indices in the above arrays
    std::vector<int>  facet_tex;
    std::vector<int>  facet_nrm;
};
using ModelPtr = std::shared_ptr<Model>;

class Shader
{
public:
    virtual ~Shader() {}
    virtual void vs(vec4& p) = 0;
    virtual bool fs(const vec3& bar, vec4& color) = 0;
};
using ShaderPtr = std::shared_ptr<Shader>;

vec3 barycentricLine(const vec2 tri[2], const vec2 p)
{
    double a = glm::distance(p, tri[0]) / glm::distance(tri[0], tri[1]);
    return vec3{ 1 - a, a, 0 };
}

vec3 barycentric(const vec2 tri[3], const vec2& P)
{
    glm::mat3 ABC = { vec3(tri[0], 1.0), vec3(tri[1], 1.0), vec3(tri[2], 1.0) };

    // for a degenerate triangle generate negative coordinates, it will be thrown away by the rasterizator
    // if (ABC.det() < 1e-3) return { -1, 1, 1 };

    return glm::inverse(ABC) * vec3(P, 1.0);
}

class Render
{
public:
    Render(ModelPtr model, ShaderPtr shader) : _model(std::move(model)), _shader(std::move(shader)) {}
    ~Render() {}

    void setViewport(int x, int y, int w, int h)
    {
        _viewport = glm::mat4(1.0f);

        // 缩放 NDC 到窗口坐标的比例
        _viewport[0][0] = (w - 1) / 2.0f;
        _viewport[1][1] = (h - 1) / 2.0f;
        _viewport[2][2] = 1;

        // 平移到窗口坐标的偏移量
        _viewport[3][0] = x + (w - 1) / 2.0f;
        _viewport[3][1] = y + (h - 1) / 2.0f;
        _viewport[3][2] = 0;
    }

    void setModel(ModelPtr model) { _model = std::move(model); }

    void drawArray(Image& frame, PrimitiveMode mode, int start, int vertexCount)
    {
        if (mode == PrimitiveMode::Triangle) {
            int priCount = vertexCount / 3;
            for (int i = 0; i < priCount; i++) {
                vec3 tri[3] = { _model->vertex(start + i * 3), _model->vertex(start + i * 3 + 1),
                                _model->vertex(start + i * 3 + 2) };

                drawTriangle(frame, tri);
            }
        }
        else if (mode == PrimitiveMode::Line) {
            int priCount = vertexCount / 2;
            for (int i = 0; i < priCount; i++) {
                vec3 line[2] = { _model->vertex(start + i * 2), _model->vertex(start + i * 2 + 1) };
                drawLine(frame, line);
            }
        }
        else if (mode == PrimitiveMode::Point) {
            for (int i = start; i < vertexCount; i++) {
                drawPoint(frame, _model->vertex(i));
            }
        }
    }

    void drawIndex(Image& frame, PrimitiveMode mode, int start, int indexCount)
    {
        if (mode == PrimitiveMode::Triangle) {
            int priCount = indexCount / 3;
            for (int i = 0; i < priCount; i++) {
                vec3 tri[3] = { _model->indexVertex(start + i * 3), _model->indexVertex(start + i * 3 + 1),
                                _model->indexVertex(start + i * 3 + 2) };

                drawTriangle(frame, tri);
            }
        }
        else if (mode == PrimitiveMode::Line) {
            int priCount = indexCount / 2;
            for (int i = 0; i < priCount; i++) {
                vec3 line[2] = { _model->indexVertex(start + i * 2), _model->indexVertex(start + i * 2 + 1) };
                drawLine(frame, line);
            }
        }
        else if (mode == PrimitiveMode::Point) {
            for (int i = start; i < indexCount; i++) {
                drawPoint(frame, _model->indexVertex(i));
            }
        }
    }

private:
    void drawPoint(Image& frame, vec3 p)
    {
        vec4 v = vec4(p, 1.0);
        _shader->vs(v);
        vec4 pV = _viewport * v;
        vec2 pt{ pV[0] / pV[3], pV[1] / pV[3] };

        vec4 fsColor;
        if (_shader->fs(vec3{ 1.0, 0.0, 0.0 }, fsColor)) {
            fsColor = fsColor * 255.0f;
            jrender::Color color{ (uint8_t)fsColor[0], (uint8_t)fsColor[1], (uint8_t)fsColor[2], (uint8_t)fsColor[3] };
            frame.setPixel((int)pt.x, (int)pt.y, color);
        }
    }

    void drawLine(Image& frame, vec3 line[2])
    {
        vec4 v0 = vec4(line[0], 1.0);
        vec4 v1 = vec4(line[1], 1.0);
        _shader->vs(v0);
        _shader->vs(v1);
        vec4 pV0 = _viewport * v0;
        vec4 pV1 = _viewport * v1;

        vec2 pts[2] = {
            { pV0[0] / pV0[3], pV0[1] / pV0[3] },
            { pV1[0] / pV1[3], pV1[1] / pV1[3] },
        };

#pragma omp parallel for
        for (const auto& p : linePoints(vec2{ pts[0].x, pts[0].y }, vec2{ pts[1].x, pts[1].y })) {
            vec4 fsColor;
            if (_shader->fs(barycentricLine(pts, p), fsColor)) {
                fsColor = fsColor * 255.0f;
                jrender::Color color{ (uint8_t)fsColor[0], (uint8_t)fsColor[1], (uint8_t)fsColor[2],
                                      (uint8_t)fsColor[3] };
                frame.setPixel(p.x, p.y, color);
            }
        }
    }

    void drawTriangle(Image& frame, vec3 tri[3])
    {
        vec4 v0 = vec4(tri[0], 1.0);
        vec4 v1 = vec4(tri[1], 1.0);
        vec4 v2 = vec4(tri[2], 1.0);

        _shader->vs(v0);
        _shader->vs(v1);
        _shader->vs(v2);

        vec4 pV0 = _viewport * v0;
        vec4 pV1 = _viewport * v1;
        vec4 pV2 = _viewport * v2;

        vec2 pts[3] = { vec2(pV0 / pV0[3]), vec2(pV1 / pV1[3]), vec2(pV2 / pV2[3]) };

        int minX = std::min({ pts[0].x, pts[1].x, pts[2].x });
        int maxX = std::max({ pts[0].x, pts[1].x, pts[2].x });
        int minY = std::min({ pts[0].y, pts[1].y, pts[2].y });
        int maxY = std::max({ pts[0].y, pts[1].y, pts[2].y });

#pragma omp parallel for
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                vec3 bc_screen = barycentric(pts, vec2{ (double)x, (double)y });
                if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0) continue;

                vec4 fsColor;
                if (_shader->fs(bc_screen, fsColor)) {
                    fsColor = fsColor * 255.0f;
                    Color color{ (uint8_t)fsColor[0], (uint8_t)fsColor[1], (uint8_t)fsColor[2], (uint8_t)fsColor[3] };
                    frame.setPixel(x, y, color);
                }
            }
        }
    }

private:
    ModelPtr  _model;
    ShaderPtr _shader;
    glm::mat4 _viewport;
};

}  // namespace jrender