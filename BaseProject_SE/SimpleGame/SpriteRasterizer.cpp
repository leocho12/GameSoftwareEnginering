#include "stdafx.h"
#include "SpriteRasterizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace
{
constexpr int Width = 512;
constexpr int Height = 640;
constexpr float Scale = 4.0f;

struct Point
{
    float x;
    float y;
};

struct Color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

class Canvas
{
  public:
    Canvas() : pixels(Width * Height * 4, 0)
    {
    }

    void Rectangle(float x, float y, float w, float h, Color color)
    {
        Rasterize(x,
                  y,
                  x + w,
                  y + h,
                  [=](float px, float py)
                  {
                      return px >= x && px <= x + w && py >= y && py <= y + h;
                  },
                  color);
    }

    void Ellipse(float x, float y, float w, float h, Color color)
    {
        const float centerX = x + w * .5f;
        const float centerY = y + h * .5f;
        const float radiusX = std::max(.001f, w * .5f);
        const float radiusY = std::max(.001f, h * .5f);
        Rasterize(x,
                  y,
                  x + w,
                  y + h,
                  [=](float px, float py)
                  {
                      const float dx = (px - centerX) / radiusX;
                      const float dy = (py - centerY) / radiusY;
                      return dx * dx + dy * dy <= 1;
                  },
                  color);
    }

    void Polygon(const Point *points, int count, Color color)
    {
        float left = points[0].x, right = points[0].x;
        float top = points[0].y, bottom = points[0].y;
        for (int i = 1; i < count; ++i)
        {
            left = std::min(left, points[i].x);
            right = std::max(right, points[i].x);
            top = std::min(top, points[i].y);
            bottom = std::max(bottom, points[i].y);
        }

        Rasterize(left,
                  top,
                  right,
                  bottom,
                  [=](float px, float py)
                  {
                      bool inside = false;
                      for (int i = 0, j = count - 1; i < count; j = i++)
                      {
                          const auto &a = points[i];
                          const auto &b = points[j];
                          const bool crosses = (a.y > py) != (b.y > py);
                          const float edgeX = (b.x - a.x) * (py - a.y) /
                                                  ((b.y - a.y) + .000001f) +
                                              a.x;
                          if (crosses && px < edgeX)
                              inside = !inside;
                      }
                      return inside;
                  },
                  color);
    }

    std::vector<unsigned char> TakePixels()
    {
        return std::move(pixels);
    }

  private:
    std::vector<unsigned char> pixels;

    template <class Contains>
    void Rasterize(float left, float top, float right, float bottom, Contains contains, Color color)
    {
        const int minX = std::clamp(int(std::floor(left * Scale)), 0, Width - 1);
        const int maxX = std::clamp(int(std::ceil(right * Scale)), 0, Width);
        const int minY = std::clamp(int(std::floor(top * Scale)), 0, Height - 1);
        const int maxY = std::clamp(int(std::ceil(bottom * Scale)), 0, Height);
        constexpr std::array<float, 2> samples{{.25f, .75f}};

        for (int y = minY; y < maxY; ++y)
            for (int x = minX; x < maxX; ++x)
            {
                int coverage = 0;
                for (float sampleY : samples)
                    for (float sampleX : samples)
                        coverage += contains((x + sampleX) / Scale, (y + sampleY) / Scale);
                if (coverage == 0)
                    continue;

                const unsigned int alpha = unsigned(coverage) * 255 / 4;
                const std::size_t index = (std::size_t(y) * Width + x) * 4;
                const unsigned int inverse = 255 - alpha;
                const unsigned int oldAlpha = pixels[index + 3];
                const unsigned int outputAlpha = alpha + oldAlpha * inverse / 255;
                const auto composite = [&](unsigned char source, std::size_t channel)
                {
                    if (outputAlpha == 0)
                        return static_cast<unsigned char>(0);
                    const unsigned int previous =
                        unsigned(pixels[index + channel]) * oldAlpha * inverse / 255;
                    return static_cast<unsigned char>((unsigned(source) * alpha + previous) /
                                                      outputAlpha);
                };
                pixels[index] = composite(color.r, 0);
                pixels[index + 1] = composite(color.g, 1);
                pixels[index + 2] = composite(color.b, 2);
                pixels[index + 3] = static_cast<unsigned char>(outputAlpha);
            }
    }
};
}

namespace SpriteRasterizer
{
std::vector<unsigned char> Build(int kind, int frame)
{
    Canvas canvas;
    const float swing = std::sin(frame * 6.2831853f / 8) * 7;
    const Color outline{24, 35, 39};
    const Color coat = kind == 2   ? Color{121, 58, 50}
                       : kind == 3 ? Color{120, 116, 87}
                                   : Color{67, 89, 83};
    const Color hair = kind == 1 ? Color{182, 174, 146} : Color{65, 73, 72};
    const Color skin{223, 196, 162};
    const Color accent = kind == 2 ? Color{240, 104, 73} : Color{91, 209, 194};

    if (kind == 3)
    {
        canvas.Ellipse(24, 65, 78, 54, outline);
        canvas.Ellipse(29, 62, 70, 49, coat);
        canvas.Ellipse(76, 50, 39, 34, coat);
        canvas.Rectangle(34, 105, 13, 30 + int(swing), outline);
        canvas.Rectangle(81, 105, 13, 30 - int(swing), outline);
        canvas.Ellipse(98, 59, 7, 5, accent);
        const Point ear[] = {{83, 54}, {82, 32}, {96, 51}};
        canvas.Polygon(ear, 3, coat);
    }
    else
    {
        canvas.Rectangle(43, 107, 17, 35 + int(swing), outline);
        canvas.Rectangle(69, 107, 17, 35 - int(swing), outline);
        canvas.Ellipse(31, 66, 67, 58, outline);
        canvas.Ellipse(35, 65, 58, 55, coat);
        canvas.Ellipse(28, 17, 75, 68, hair);
        canvas.Ellipse(39, 33, 53, 43, skin);
        canvas.Ellipse(29, 17, 73, 30, hair);
        canvas.Rectangle(28, 34, 13, 49, hair);
        canvas.Rectangle(91, 35, 12, 45, hair);
        canvas.Ellipse(49, 51, 5, 7, outline);
        canvas.Ellipse(76, 51, 5, 7, outline);
        canvas.Rectangle(23, 77 + int(swing * .4f), 19, 37, coat);
        canvas.Rectangle(103, 54, 5, 87, outline);
        canvas.Ellipse(98, 43, 15, 20, accent);
        canvas.Rectangle(47, 84, 12, 14, accent);
        canvas.Rectangle(55, 99, 10, 20, outline);
        const Point robe[] = {{42, 83}, {85, 83}, {96, 126}, {31, 126}};
        canvas.Polygon(robe, 4, coat);
        canvas.Rectangle(40, 99, 47, 5, accent);
    }

    return canvas.TakePixels();
}
}
