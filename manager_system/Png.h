/**
 * ============================================================
 *  Png — dependency-free PNG writer + monthly report raster
 * ============================================================
 *  (v3.7.0) The manager can save the monthly report as a real
 *  PNG image (option 18) without a single external library:
 *
 *    - Canvas: a tiny RGB pixel buffer with rects and 8x8 text
 *      (the glyphs come from PngFont.h, rasterised at build time);
 *    - the PNG encoder emits IHDR / IDAT / IEND chunks by hand.
 *      The IDAT payload is a zlib stream made of STORED deflate
 *      blocks (BTYPE=00) — no compressor needed, any image
 *      decoder accepts it — closed with a hand-rolled Adler32;
 *    - CRC32 is the standard PNG polynomial, also hand-rolled.
 *
 *  Labels are drawn in English/ASCII on purpose: the terminal
 *  already speaks Persian (`--lang fa`), while the image keeps
 *  the ASCII-only font contract.
 * ============================================================
 */
#ifndef PNG_H
#define PNG_H

#include <string>
#include <vector>

#include "MonthlyReport.h" // CarRevenue, MonthlyReport

namespace png
{

/// Standard PNG CRC polynomial (IEEE 802.3), bitwise — no table.
unsigned long crc32(const unsigned char *data, std::size_t len);

/// zlib's Adler32 (checksum that closes the IDAT stream).
unsigned long adler32(const unsigned char *data, std::size_t len);

/// A small RGB canvas: fill, rects and 8x8 (scaled) text.
class Canvas
{
  public:
    Canvas(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    /// Raw byte at index i of the RGB buffer (encoder + tests).
    unsigned char pixel(std::size_t index) const { return px_.at(index); }

    void fill(unsigned char r, unsigned char g, unsigned char b);
    /// Bounds-checked single pixel (no-op outside the canvas).
    void set(int x, int y, unsigned char r, unsigned char g, unsigned char b);
    void fillRect(int x, int y, int w, int h, unsigned char r,
                  unsigned char g, unsigned char b);
    /// Draw text at (x, y), scale s (1 = 8px glyphs). ASCII only.
    void text(int x, int y, const std::string &s, unsigned char r,
              unsigned char g, unsigned char b, int scale = 1);

  private:
    int width_;
    int height_;
    std::vector<unsigned char> px_; // RGB triplets
};

/// Encode the canvas as a valid 8-bit RGB PNG file. Missing parent
/// directories are created; false when the file cannot be written.
bool writePng(const std::string &path, const Canvas &canvas);

/// Render the monthly report as `path` (bars + figures, same
/// numbers as the console screen). An all-zero month still
/// produces a valid image. False with the reason in `errorOut`.
bool writeMonthlyReportPng(const MonthlyReport &report, int year,
                           int month, const std::string &path,
                           std::string &errorOut);

} // namespace png

#endif // PNG_H
