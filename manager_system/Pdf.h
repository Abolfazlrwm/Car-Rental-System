/**
 * ============================================================
 *  Pdf — dependency-free PDF writer + consolidated report
 * ============================================================
 *  (v3.9.0) The manager can save every report as ONE printable
 *  PDF (option 21): a cover page, then the monthly report, the
 *  loyalty overview and the fleet analytics dashboard, each
 *  embedded as a full-page DeviceRGB image drawn by the same
 *  png::render* functions the PNG files and the REST layer use —
 *  so file, image and screen can never drift apart.
 *
 *  Hand-rolled, still zero dependencies: objects, xref and
 *  trailer are assembled here; text pages use the Helvetica base
 *  fonts (ASCII only — the same contract as the image font); the
 *  image XObjects are uncompressed raw RGB streams. Bytes are
 *  fully deterministic (no timestamps), so the suite pins them.
 * ============================================================
 */
#ifndef PDF_H
#define PDF_H

#include <string>

#include "Analytics.h"     // AnalyticsReport
#include "Loyalty.h"       // LoyaltyOverview
#include "MonthlyReport.h" // MonthlyReport

namespace pdf
{

/// Build the consolidated report as PDF bytes in memory — the
/// REST layer serves it straight from here. An invalid year or
/// month is rejected before anything is built. False with the
/// reason in `errorOut`.                           (v3.9.0)
bool buildAllReportsPdf(const MonthlyReport &monthly, int year,
                        int month, const LoyaltyOverview &loyalty,
                        const AnalyticsReport &dashboard,
                        std::string &out, std::string &errorOut);

/// Write the consolidated report as `path` (cover + one page per
/// report: monthly, loyalty, dashboard). Missing parent
/// directories are created. False with the reason in `errorOut`.
///                                                 (v3.9.0)
bool writeAllReportsPdf(const MonthlyReport &monthly, int year,
                        int month, const LoyaltyOverview &loyalty,
                        const AnalyticsReport &dashboard,
                        const std::string &path, std::string &errorOut);

} // namespace pdf

#endif // PDF_H
