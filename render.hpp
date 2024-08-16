#include <iostream>
#include <vector>
#include <cstdint>

#include "geometry.h"

namespace jrender {

struct Point2D
{
    int x;
    int y;
};

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

enum class Format { BGRA, RGBA };

std::vector<Point2D> linePoints(Point2D&& p0, Point2D&& p1)
{
    std::vector<Point2D> pts;

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
            pts.emplace_back(Point2D{ y, x });
        }
        else {
            pts.emplace_back(Point2D{ x, y });
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
                norms.push_back(n.normalized());
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
    vec3 vertex(int face, int i) const { return _vertices[_indices[face * 3 + i]]; }

private:
    std::vector<vec3> _vertices;   // array of vertices
    std::vector<vec2> _texCoords;  // per-vertex array of tex coords
    std::vector<vec3> norms;       // per-vertex array of normal vectors
    std::vector<int>  _indices;    // per-triangle indices in the above arrays
    std::vector<int>  facet_tex;
    std::vector<int>  facet_nrm;
};

inline void DrawPoint(Image& image, const vec2& p, const Color& c)
{
    image.setPixel((int)std::round(((p.x + 1) / 2) * (image.width() - 1)),
                   (int)std::round(((p.y + 1) / 2) * (image.height() - 1)), c);
}

void DrawLine(Image& image, const vec2& p0, const vec2& p1, const Color& c)
{
    auto vec2ToPoint = [&image](const vec2& p) {
        return Point2D{ (int)std::round(((p.x + 1) / 2) * (image.width() - 1)),
                        (int)std::round(((p.y + 1) / 2) * (image.height() - 1)) };
    };

    for (const auto& p : linePoints(vec2ToPoint(p0), vec2ToPoint(p1))) {
        image.setPixel(p.x, p.y, c);
    }
}

void DrawTriangle(Image& image, const vec3& p0, const vec3& p1, const vec3& p2, const Color& c)
{
    DrawLine(image, vec2{ p0.x, p0.y }, vec2{ p1.x, p1.y }, c);
    DrawLine(image, vec2{ p1.x, p1.y }, vec2{ p2.x, p2.y }, c);
    DrawLine(image, vec2{ p2.x, p2.y }, vec2{ p0.x, p0.y }, c);
}

}  // namespace jrender