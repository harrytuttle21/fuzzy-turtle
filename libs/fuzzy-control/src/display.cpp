// fuzzy-control : ecrans de la Maschine MK3.
#include "fuzzy_control/display.h"
#include "fuzzy_control/maschine_mk3.h"
#include "font8x16.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace fuzzy::control {

namespace {
// Correction du vert (gamma 1,6) : les ecrans Maschine rendent l'orange trop jaune
const std::array<uint8_t, 256>& greenLut()
{
    static const auto lut = [] {
        std::array<uint8_t, 256> t {};
        for (int v = 0; v < 256; ++v)
            t[(size_t) v] = (uint8_t) std::lround (255.0 * std::pow (v / 255.0, 1.6));
        return t;
    }();
    return lut;
}

void put16 (std::vector<uint8_t>& out, int v) { out.push_back ((uint8_t) (v >> 8)); out.push_back ((uint8_t) v); }
} // namespace

uint16_t toRgb565 (Rgb c, bool colorCorrection)
{
    const int g = colorCorrection ? greenLut()[c.g] : c.g;
    return (uint16_t) (((c.r & 0xF8) << 8) | ((g & 0xFC) << 3) | (c.b >> 3));
}

Canvas::Canvas() : px ((size_t) (kWidth * kHeight)) {}

void Canvas::fill (Rgb c) { std::fill (px.begin(), px.end(), c); }

void Canvas::fillRect (int x, int y, int w, int h, Rgb c)
{
    const int x0 = std::max (0, x), y0 = std::max (0, y);
    const int x1 = std::min (kWidth, x + w), y1 = std::min (kHeight, y + h);
    for (int yy = y0; yy < y1; ++yy)
        std::fill (px.begin() + yy * kWidth + x0, px.begin() + yy * kWidth + std::max (x0, x1), c);
}

void Canvas::frameRect (int x, int y, int w, int h, Rgb c, int t)
{
    fillRect (x, y, w, t, c);
    fillRect (x, y + h - t, w, t, c);
    fillRect (x, y, t, h, c);
    fillRect (x + w - t, y, t, h, c);
}

int Canvas::text (int x, int y, std::string_view s, Rgb c, int scale)
{
    int cx = x;
    for (const char ch : s)
    {
        const int code = (unsigned char) ch;
        if (code >= 32 && code <= 126)
        {
            const auto& glyph = detail::kFont8x16[code - 32];
            for (int row = 0; row < 16; ++row)
                for (int col = 0; col < 8; ++col)
                    if (glyph[row] & (0x80 >> col))
                        fillRect (cx + col * scale, y + row * scale, scale, scale, c);
        }
        cx += 8 * scale;
    }
    return cx - x;
}

Rect changedArea (const Canvas& a, const Canvas& b)
{
    // On renvoie des LIGNES ENTIERES (x = 0, largeur 480) : les zones etroites a une
    // position quelconque decalaient l'image sur la MK3 (constate le 20/11/2026).
    int y0 = Canvas::kHeight, y1 = -1;
    const auto& pa = a.pixels();
    const auto& pb = b.pixels();
    for (int y = 0; y < Canvas::kHeight; ++y)
    {
        const long row = (long) y * Canvas::kWidth;
        if (! std::equal (pa.begin() + row, pa.begin() + row + Canvas::kWidth, pb.begin() + row))
        {
            y0 = std::min (y0, y);
            y1 = y;
        }
    }
    if (y1 < 0)
        return {};
    return { 0, y0, Canvas::kWidth, y1 - y0 + 1 };
}

std::vector<uint8_t> screenPacket (const Canvas& c, int screen, Rect r, bool colorCorrection)
{
    std::vector<uint8_t> out;
    const long count = (long) r.w * r.h;
    out.reserve ((size_t) (32 + count * 2));
    const uint8_t head[] = { 0x84, 0x00, (uint8_t) screen, 0x60, 0x00, 0x00, 0x00, 0x00 };
    out.insert (out.end(), std::begin (head), std::end (head));
    put16 (out, r.x); put16 (out, r.y); put16 (out, r.w); put16 (out, r.h);
    const uint8_t mid[] = { 0x02, 0x00, 0x00, 0x00 };
    out.insert (out.end(), std::begin (mid), std::end (mid));
    const long words = count / 2;
    out.push_back ((uint8_t) (words >> 24)); out.push_back ((uint8_t) (words >> 16));
    out.push_back ((uint8_t) (words >> 8));  out.push_back ((uint8_t) words);
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x)
            put16 (out, toRgb565 (c.at (x, y), colorCorrection));
    const uint8_t tail[] = { 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00 };
    out.insert (out.end(), std::begin (tail), std::end (tail));
    return out;
}

long Mk3Display::send (MaschineMk3& device)
{
    long total = 0;
    for (int s = 0; s < 2; ++s)
    {
        const Rect area = firstFrame ? Rect { 0, 0, Canvas::kWidth, Canvas::kHeight }
                                     : changedArea (canvas[(size_t) s], sent[(size_t) s]);
        if (area.empty())
            continue;
        const auto packet = screenPacket (canvas[(size_t) s], s, area, correction);
        if (! device.writeDisplay (packet.data(), (int) packet.size()))
            return -1;
        sent[(size_t) s] = canvas[(size_t) s];
        total += (long) packet.size();
    }
    firstFrame = false;
    return total;
}

} // namespace fuzzy::control
