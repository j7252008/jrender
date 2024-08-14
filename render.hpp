#include <iostream>
#include <vector>
#include <cstdint>

namespace jrender {

struct Point
{
    int x;
    int y;
};

struct Color
{
    uint8_t color[4];
};

enum class Format { BGRA, RGBA };

std::vector<Point> linePoints(Point&& p0, Point&& p1)
{
    std::vector<Point> pts;

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
            pts.emplace_back(y, x);
        }
        else {
            pts.emplace_back(x, y);
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
        int index = (y * _width + x) * FormatSize(_format);
        switch (_format) {
        case Format::BGRA: {
            _pixels[index] = c.color[0];
            _pixels[index + 1] = c.color[1];
            _pixels[index + 2] = c.color[2];
            _pixels[index + 3] = c.color[3];
        } break;
        case Format::RGBA: {
            _pixels[index] = c.color[2];
            _pixels[index + 1] = c.color[1];
            _pixels[index + 2] = c.color[0];
            _pixels[index + 3] = c.color[3];
        } break;
        default:
            break;
        }
    }

    char* data() { return _pixels.data(); }

private:
    Format            _format;
    int               _width;
    int               _height;
    std::vector<char> _pixels;
};

inline void DrawPoint(Image& image, const Point& p, const Color& c)
{
    image.setPixel(p.x, p.y, c);
}

void DrawLine(Image& image, const Point& p0, const Point& p1, const Color& c)
{
    for (const auto& p : linePoints(Point{ p0 }, Point{ p1 })) {
        DrawPoint(image, p, c);
    }
}

void DrawTriangle(Image& image, const Point& p0, const Point& p1, const Point& p2, const Color& c)
{
    DrawLine(image, p0, p1, c);
    DrawLine(image, p1, p2, c);
    DrawLine(image, p2, p0, c);
}

}  // namespace jrender