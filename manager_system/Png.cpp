/**
 * ============================================================
 *  Png — hand-rolled encoder + monthly report raster (impl)
 * ============================================================
 *  Zero dependencies: CRC32, Adler32 and the STORED-deflate zlib
 *  framing are implemented here in ~120 lines. Every image the
 *  writer produces is verified by the unit suite (signature,
 *  IHDR geometry, chunk CRCs) and by round-tripping through a
 *  real decoder in the release checks.
 * ============================================================
 */
#include "Png.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "PngFont.h"

namespace png
{

/* ---------------- checksums ---------------- */

unsigned long crc32(const unsigned char *data, std::size_t len)
{
    unsigned long crc = 0xFFFFFFFFUL;
    for (std::size_t i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
    return crc ^ 0xFFFFFFFFUL;
}

unsigned long adler32(const unsigned char *data, std::size_t len)
{
    unsigned long a = 1, b = 0;
    for (std::size_t i = 0; i < len; ++i)
    {
        a = (a + data[i]) % 65521UL;
        b = (b + a) % 65521UL;
    }
    return (b << 16) | a;
}

/* ---------------- canvas ---------------- */

Canvas::Canvas(int width, int height) : width_(width), height_(height)
{
    px_.assign(static_cast<std::size_t>(width_) * height_ * 3, 255);
}

void Canvas::fill(unsigned char r, unsigned char g, unsigned char b)
{
    for (std::size_t i = 0; i < px_.size(); i += 3)
    {
        px_[i] = r;
        px_[i + 1] = g;
        px_[i + 2] = b;
    }
}

void Canvas::set(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_)
        return;
    const std::size_t at = (static_cast<std::size_t>(y) * width_ + x) * 3;
    px_[at] = r;
    px_[at + 1] = g;
    px_[at + 2] = b;
}

void Canvas::fillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
{
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx)
            set(x + dx, y + dy, r, g, b);
}

void Canvas::text(int x, int y, const std::string &s, unsigned char r, unsigned char g,
                  unsigned char b, int scale)
{
    if (scale < 1)
        scale = 1;
    int cursor = x;
    for (char raw : s)
    {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (c < 32 || c > 126)
            continue; // the font covers printable ASCII only
        const unsigned char *glyph = PNG_FONT[c - 32];
        for (int gy = 0; gy < 8; ++gy)
            for (int gx = 0; gx < 8; ++gx)
                if (glyph[gy] & (1u << gx))
                    fillRect(cursor + gx * scale, y + gy * scale, scale, scale, r, g, b);
        cursor += 8 * scale;
    }
}

/* ---------------- encoder ---------------- */

namespace
{

void be32(std::vector<unsigned char> &v, unsigned long x)
{
    v.push_back(static_cast<unsigned char>((x >> 24) & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 16) & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
    v.push_back(static_cast<unsigned char>(x & 0xFF));
}

void chunk(std::vector<unsigned char> &out, const char type[4],
           const std::vector<unsigned char> &data)
{
    be32(out, data.size());
    std::vector<unsigned char> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    be32(out, crc32(body.data(), body.size()));
}

const unsigned char kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

} // namespace

bool writePng(const std::string &path, const Canvas &canvas)
{
    // raw scanlines: one filter byte (0 = None) per row
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<std::size_t>(canvas.height()) * (canvas.width() * 3 + 1));
    for (int y = 0; y < canvas.height(); ++y)
    {
        raw.push_back(0x00);
        for (int x = 0; x < canvas.width(); ++x)
        {
            const std::size_t at = (static_cast<std::size_t>(y) * canvas.width() + x) * 3;
            raw.push_back(canvas.pixel(at));
            raw.push_back(canvas.pixel(at + 1));
            raw.push_back(canvas.pixel(at + 2));
        }
    }

    // zlib stream: 0x78 0x01 + stored deflate blocks + Adler32
    std::vector<unsigned char> idat;
    idat.push_back(0x78);
    idat.push_back(0x01);
    std::size_t off = 0;
    while (off < raw.size() || off == 0) // at least one (empty) block
    {
        const std::size_t take = std::min<std::size_t>(65535, raw.size() - off);
        const bool last = off + take >= raw.size();
        idat.push_back(last ? 0x01 : 0x00);
        idat.push_back(static_cast<unsigned char>(take & 0xFF));
        idat.push_back(static_cast<unsigned char>((take >> 8) & 0xFF));
        idat.push_back(static_cast<unsigned char>((~take) & 0xFF));
        idat.push_back(static_cast<unsigned char>((~take >> 8) & 0xFF));
        idat.insert(idat.end(), raw.begin() + off, raw.begin() + off + take);
        off += take;
        if (raw.empty())
            break;
    }
    be32(idat, adler32(raw.data(), raw.size()));

    std::vector<unsigned char> ihdr;
    be32(ihdr, static_cast<unsigned long>(canvas.width()));
    be32(ihdr, static_cast<unsigned long>(canvas.height()));
    ihdr.push_back(8); // bit depth
    ihdr.push_back(2); // colour type: truecolor RGB
    ihdr.push_back(0); // compression: deflate
    ihdr.push_back(0); // filter: adaptive (all-None here)
    ihdr.push_back(0); // interlace: none

    std::vector<unsigned char> file(kSignature, kSignature + 8);
    chunk(file, "IHDR", ihdr);
    chunk(file, "IDAT", idat);
    chunk(file, "IEND", std::vector<unsigned char>());

    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    out.write(reinterpret_cast<const char *>(file.data()),
              static_cast<std::streamsize>(file.size()));
    return static_cast<bool>(out);
}

/* ---------------- monthly report raster ---------------- */

namespace
{

/// 2-decimal fixed money text, same look as the console's money().
std::string money2(double amount)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", amount);
    return buf;
}

const unsigned char kBg = 246, kInk = 45, kSoft = 110, kBar[3] = {66, 110, 180};

void drawBarRow(Canvas &cv, int x, int y, const std::string &label, double value, double maxValue,
                const std::string &suffix, int labelWidthPx)
{
    cv.text(x, y, label, kSoft, kSoft, kSoft, 1);
    const int barX = x + labelWidthPx;
    const int barMax = 230;
    int barLen = 0;
    if (maxValue > 0.0)
        barLen = static_cast<int>(value / maxValue * barMax + 0.5);
    cv.fillRect(barX, y + 2, barLen, 6, kBar[0], kBar[1], kBar[2]);
    cv.text(barX + barMax + 8, y, suffix, kInk, kInk, kInk, 1);
}

} // namespace

bool writeMonthlyReportPng(const MonthlyReport &report, int year, int month,
                           const std::string &path, std::string &errorOut)
{
    if (month < 1 || month > 12 || year < 1)
    {
        errorOut = "invalid year/month";
        return false;
    }

    int maxRevenue = 0;
    for (const CarRevenue &car : report.perCar)
        maxRevenue = std::max(maxRevenue, car.rentals > 0 ? static_cast<int>(car.revenue) : 0);
    int maxDay = 0;
    for (const NamedCount &day : report.perDay)
        maxDay = std::max(maxDay, day.count);

    const int rows =
        6 + static_cast<int>(report.perCar.size()) + 1 + static_cast<int>(report.perDay.size()) + 2;
    const int height = std::max(120, 18 + rows * 12 + 14);
    Canvas cv(620, height);
    cv.fill(kBg, kBg, kBg);

    char monthLabel[24]; // worst case "2147483647-2147483647" = 22 bytes
    std::snprintf(monthLabel, sizeof monthLabel, "%04d-%02d", year, month);

    int y = 10;
    cv.text(16, y, "MONTHLY REPORT  " + std::string(monthLabel), kInk, kInk, kInk, 2);
    y += 26;
    cv.fillRect(16, y, 588, 1, 200, 200, 200);
    y += 8;

    cv.text(16, y, "Rentals completed: " + std::to_string(report.rentalsCompleted), kInk, kInk,
            kInk, 1);
    y += 12;
    cv.text(16, y,
            "Revenue booked: " + money2(report.revenue) + "   (discounts: " +
                money2(report.discountGiven) + ", late fees: " + money2(report.lateFees) + ")",
            kInk, kInk, kInk, 1);
    y += 12;
    cv.text(16, y, "Distinct customers: " + std::to_string(report.distinctCustomers), kInk, kInk,
            kInk, 1);
    y += 12;
    if (report.topDay > 0)
    {
        const std::string dayLabel =
            (report.topDay < 10 ? "0" : "") + std::to_string(report.topDay);
        cv.text(16, y,
                std::string("Busiest day: ") + dayLabel + "  (" +
                    std::to_string(report.topDayRentals) + " returns)",
                kInk, kInk, kInk, 1);
    }
    else
    {
        cv.text(16, y, "No rentals were completed in " + std::string(monthLabel), kSoft, kSoft,
                kSoft, 1);
    }
    y += 18;

    cv.text(16, y, "Revenue by car", kSoft, kSoft, kSoft, 1);
    y += 12;
    for (const CarRevenue &car : report.perCar)
    {
        drawBarRow(cv, 16, y, car.name, car.revenue, static_cast<double>(maxRevenue),
                   std::to_string(car.rentals) + " rentals, " + money2(car.revenue), 140);
        y += 12;
    }
    y += 6;

    cv.text(16, y, "Returns per day", kSoft, kSoft, kSoft, 1);
    y += 12;
    for (const NamedCount &day : report.perDay)
    {
        drawBarRow(cv, 16, y, day.label, static_cast<double>(day.count),
                   static_cast<double>(maxDay), std::to_string(day.count), 140);
        y += 12;
    }

    if (!writePng(path, cv))
    {
        errorOut = "cannot open the file for writing";
        return false;
    }
    return true;
}

} // namespace png
