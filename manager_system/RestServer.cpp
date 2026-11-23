/**
 * ============================================================
 *  Rest — router + hand-rolled HTTP server (implementation)
 * ============================================================
 *  The router builds JSON with the same jsonEscape() the report
 *  writer uses, so a car named `a"b` cannot break a payload. The
 *  socket loop is deliberately boring: accept, read one request,
 *  answer, close — one connection at a time is plenty for a
 *  local dashboard.
 * ============================================================
 */
#include "RestServer.h"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "Analytics.h" // NamedCount
#include "AuditLog.h"  // AuditEntry
#include "Car.h"
#include "Date.h"
#include "JsonExport.h" // jsonEscape
#include "Loyalty.h"    // LoyaltyOverview, tier names
#include "MonthlyReport.h"
#include "Pdf.h" // consolidated report bytes
#include "Png.h" // Canvas, render*, encodePng
#include "RentalManager.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#ifndef __EMSCRIPTEN__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#endif

namespace rest
{

namespace
{

std::string num(double value)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    return oss.str();
}

std::string jstr(const std::string &value)
{
    return "\"" + jsonEscape(value) + "\"";
}

Response json(int status, const std::string &body)
{
    Response r;
    r.status = status;
    r.body = body;
    return r;
}

Response errorJson(int status, const std::string &message)
{
    return json(status, "{\"error\":" + jstr(message) + "}");
}

const char *statusText(int status)
{
    switch (status)
    {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    default:
        return "OK";
    }
}

} // namespace

/* ---------------- request parsing ---------------- */

Request parseRequestLine(const std::string &line)
{
    Request r;
    std::istringstream in(line);
    std::string target, version;
    if (!(in >> r.method >> target >> version))
        return Request(); // empty method = malformed
    const std::size_t q = target.find('?');
    if (q == std::string::npos)
    {
        r.path = urlDecode(target);
    }
    else
    {
        r.path = urlDecode(target.substr(0, q));
        r.query = target.substr(q + 1);
    }
    if (r.path.empty() || r.path[0] != '/')
        return Request();
    return r;
}

std::string urlDecode(const std::string &text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '%' && i + 2 < text.size())
        {
            const std::string hex = text.substr(i + 1, 2);
            const char *digits = "0123456789abcdefABCDEF";
            if (hex.size() == 2 && hex.find_first_not_of(digits) == std::string::npos)
            {
                out.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
                i += 2;
                continue;
            }
        }
        out.push_back(text[i] == '+' ? ' ' : text[i]);
    }
    return out;
}

std::string queryParam(const std::string &query, const std::string &key)
{
    std::size_t start = 0;
    while (start <= query.size())
    {
        const std::size_t amp = query.find('&', start);
        const std::string pair =
            query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        const std::size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key)
            return urlDecode(pair.substr(eq + 1));
        if (amp == std::string::npos)
            break;
        start = amp + 1;
    }
    return "";
}

/* ---------------- endpoints ---------------- */

Response routeGet(RentalManager &manager, const std::string &path, const std::string &query)
{
    if (path == "/health")
        return json(200, "{\"ok\":true,\"service\":\"car-rental\",\"live\":true}");

    if (path == "/version")
        return json(200, "{\"name\":\"Car Rental Management System\",\"c++\":\"C++17\","
                         "\"dependencies\":0}");

    if (path == "/cars")
    {
        std::ostringstream oss;
        oss << "{\"count\":";
        std::size_t count = 0;
        std::ostringstream cars;
        manager.forEachCarSorted(
            [&](const Car *car)
            {
                if (count++)
                    cars << ",";
                cars << "{\"name\":" << jstr(car->name()) << ",\"brand\":" << jstr(car->brand())
                     << ",\"pricePerDay\":" << num(car->pricePerDay())
                     << ",\"status\":" << jstr(toString(car->status()))
                     << ",\"reservations\":" << car->reservationCount()
                     << ",\"waitlisted\":" << car->waitlistCount()
                     << ",\"netRevenue\":" << num(car->netRevenue()) << "}";
            });
        oss << count << ",\"cars\":[" << cars.str() << "]}";
        return json(200, oss.str());
    }

    // /cars/{name} and /cars/{name}/reservations
    if (path.rfind("/cars/", 0) == 0 && path.size() > 6)
    {
        std::string rest = path.substr(6);
        const bool wantReservations =
            rest.size() > 13 && rest.substr(rest.size() - 13) == "/reservations";
        std::string name = wantReservations ? rest.substr(0, rest.size() - 13) : rest;
        const Car *car = manager.searchCar(name);
        if (car == nullptr)
            return errorJson(404, "car not found");
        if (wantReservations)
        {
            std::ostringstream oss;
            oss << "{\"car\":" << jstr(car->name()) << ",\"count\":" << car->reservationCount()
                << ",\"reservations\":[";
            bool first = true;
            car->forEachReservation(
                [&](const Reservation *r)
                {
                    if (!first)
                        oss << ",";
                    first = false;
                    oss << "{\"id\":" << r->id()
                        << ",\"start\":" << jstr(Date::toISO(r->startDate()))
                        << ",\"end\":" << jstr(Date::toISO(r->endDate()))
                        << ",\"days\":" << r->durationDays() << ",\"seriesId\":" << r->seriesId()
                        << "}";
                });
            oss << "]}";
            return json(200, oss.str());
        }
        std::ostringstream oss;
        oss << "{\"name\":" << jstr(car->name()) << ",\"brand\":" << jstr(car->brand())
            << ",\"pricePerDay\":" << num(car->pricePerDay())
            << ",\"status\":" << jstr(toString(car->status()))
            << ",\"reservations\":" << car->reservationCount()
            << ",\"waitlisted\":" << car->waitlistCount()
            << ",\"totalIncome\":" << num(car->totalIncome())
            << ",\"maintenanceCost\":" << num(car->maintenanceCost())
            << ",\"netRevenue\":" << num(car->netRevenue()) << "}";
        return json(200, oss.str());
    }

    if (path == "/stats")
    {
        const FleetStats s = manager.computeStats();
        std::ostringstream oss;
        oss << "{\"totalCars\":" << s.totalCars << ",\"availableCars\":" << s.availableCars
            << ",\"reservedCars\":" << s.reservedCars << ",\"rentedCars\":" << s.rentedCars
            << ",\"maintenanceCars\":" << s.maintenanceCars
            << ",\"activeBookings\":" << s.activeBookings
            << ",\"waitlistedCustomers\":" << manager.totalWaitlisted()
            << ",\"utilization\":" << s.utilization << ",\"grossIncome\":" << num(s.grossIncome)
            << ",\"maintenanceSpend\":" << num(s.maintenanceSpend)
            << ",\"netRevenue\":" << num(s.netRevenue) << ",\"topEarner\":" << jstr(s.topEarner)
            << "}";
        return json(200, oss.str());
    }

    if (path == "/loyalty")
    {
        const LoyaltyOverview o = manager.loyaltyOverview();
        std::ostringstream oss;
        oss << "{\"totalMembers\":" << o.totalMembers
            << ",\"outstandingPoints\":" << o.outstandingPoints
            << ",\"pointsLiability\":" << num(o.pointsLiability)
            << ",\"membersByTier\":{\"Bronze\":" << o.membersByTier[0]
            << ",\"Silver\":" << o.membersByTier[1] << ",\"Gold\":" << o.membersByTier[2]
            << ",\"Platinum\":" << o.membersByTier[3] << "}"
            << ",\"topMember\":" << jstr(o.topMember)
            << ",\"topMemberLifetime\":" << o.topMemberLifetime << "}";
        return json(200, oss.str());
    }

    if (path == "/audit")
    {
        int limit = 50;
        const std::string want = queryParam(query, "limit");
        if (!want.empty())
        {
            try
            {
                limit = std::stoi(want);
            }
            catch (...)
            {
                return errorJson(400, "limit must be an integer");
            }
        }
        if (limit < 1 || limit > 500)
            return errorJson(400, "limit must be between 1 and 500");
        const std::vector<AuditEntry> tail =
            manager.auditLog().recent(static_cast<std::size_t>(limit));
        std::ostringstream oss;
        oss << "{\"count\":" << tail.size() << ",\"entries\":[";
        for (std::size_t i = 0; i < tail.size(); ++i)
        {
            if (i)
                oss << ",";
            oss << "{\"date\":" << jstr(tail[i].date) << ",\"actor\":" << jstr(tail[i].actor)
                << ",\"action\":" << jstr(tail[i].action) << ",\"detail\":" << jstr(tail[i].detail)
                << "}";
        }
        oss << "]}";
        return json(200, oss.str());
    }

    if (path == "/pickups")
    {
        int days = 7;
        const std::string want = queryParam(query, "days");
        if (!want.empty())
        {
            try
            {
                days = std::stoi(want);
            }
            catch (...)
            {
                return errorJson(400, "days must be an integer");
            }
        }
        if (days < 1 || days > 30)
            return errorJson(400, "days must be between 1 and 30");
        const std::vector<PickupReminder> pickups =
            manager.upcomingPickups(days, Date::todaySerial());
        std::ostringstream oss;
        oss << "{\"days\":" << days << ",\"count\":" << pickups.size() << ",\"pickups\":[";
        for (std::size_t i = 0; i < pickups.size(); ++i)
        {
            if (i)
                oss << ",";
            oss << "{\"car\":" << jstr(pickups[i].carName)
                << ",\"start\":" << jstr(Date::toISO(pickups[i].start))
                << ",\"daysAway\":" << pickups[i].daysAway
                << ",\"seriesId\":" << pickups[i].seriesId << "}";
        }
        oss << "]}";
        return json(200, oss.str());
    }

    if (path == "/monthly")
    {
        const std::string yearText = queryParam(query, "year");
        const std::string monthText = queryParam(query, "month");
        if (yearText.empty() || monthText.empty())
            return errorJson(400, "year and month are required");
        int year = 0, month = 0;
        try
        {
            year = std::stoi(yearText);
            month = std::stoi(monthText);
        }
        catch (...)
        {
            return errorJson(400, "year and month must be integers");
        }
        if (month < 1 || month > 12 || year < 1)
            return errorJson(400, "month must be 1..12 and year positive");
        const MonthlyReport r = manager.monthlyReport(year, month);
        std::ostringstream oss;
        oss << "{\"month\":" << jstr(r.month) << ",\"rentalsCompleted\":" << r.rentalsCompleted
            << ",\"revenue\":" << num(r.revenue) << ",\"discountGiven\":" << num(r.discountGiven)
            << ",\"lateFees\":" << num(r.lateFees)
            << ",\"distinctCustomers\":" << r.distinctCustomers << ",\"topDay\":" << r.topDay
            << ",\"topDayRentals\":" << r.topDayRentals << ",\"perCar\":[";
        for (std::size_t i = 0; i < r.perCar.size(); ++i)
        {
            if (i)
                oss << ",";
            oss << "{\"name\":" << jstr(r.perCar[i].name) << ",\"rentals\":" << r.perCar[i].rentals
                << ",\"revenue\":" << num(r.perCar[i].revenue) << "}";
        }
        oss << "],\"perDay\":[";
        for (std::size_t i = 0; i < r.perDay.size(); ++i)
        {
            if (i)
                oss << ",";
            oss << "{\"day\":" << jstr(r.perDay[i].label) << ",\"count\":" << r.perDay[i].count
                << "}";
        }
        oss << "]}";
        return json(200, oss.str());
    }

    // (v3.9.0) Binary report endpoints: the very renderers the TUI
    // and the consolidated PDF use, served straight from memory.
    if (path == "/reports/monthly.png" || path == "/reports/all.pdf")
    {
        const std::string yearText = queryParam(query, "year");
        const std::string monthText = queryParam(query, "month");
        if (yearText.empty() || monthText.empty())
            return errorJson(400, "year and month are required");
        int year = 0, month = 0;
        try
        {
            year = std::stoi(yearText);
            month = std::stoi(monthText);
        }
        catch (...)
        {
            return errorJson(400, "year and month must be integers");
        }
        if (month < 1 || month > 12 || year < 1)
            return errorJson(400, "month must be 1..12 and year positive");
        const MonthlyReport r = manager.monthlyReport(year, month);
        std::string err;
        if (path == "/reports/monthly.png")
        {
            png::Canvas cv(1, 1);
            png::renderMonthlyReport(r, year, month, cv, err);
            const std::vector<unsigned char> bytes = png::encodePng(cv);
            Response resp;
            resp.contentType = "image/png";
            resp.body.assign(bytes.begin(), bytes.end());
            return resp;
        }
        std::string bytes;
        pdf::buildAllReportsPdf(r, year, month, manager.loyaltyOverview(),
                                buildAnalytics(manager, Date::todaySerial()), bytes, err);
        Response resp;
        resp.contentType = "application/pdf";
        resp.body = bytes;
        return resp;
    }

    if (path == "/reports/loyalty.png" || path == "/reports/dashboard.png")
    {
        png::Canvas cv(1, 1);
        std::string err;
        if (path == "/reports/loyalty.png")
            png::renderLoyaltyOverview(manager.loyaltyOverview(), cv, err);
        else
            png::renderDashboard(buildAnalytics(manager, Date::todaySerial()), cv, err);
        const std::vector<unsigned char> bytes = png::encodePng(cv);
        Response resp;
        resp.contentType = "image/png";
        resp.body.assign(bytes.begin(), bytes.end());
        return resp;
    }

    return errorJson(404, "unknown endpoint");
}

std::string httpResponse(const Response &response)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.status << " " << statusText(response.status)
        << "\r\nContent-Type: " << response.contentType
        << "\r\nContent-Length: " << response.body.size() << "\r\nConnection: close\r\n\r\n"
        << response.body;
    return oss.str();
}

/* ---------------- socket loop ---------------- */

int serve(RentalManager &manager, int port, std::string &errorOut)
{
#ifdef __EMSCRIPTEN__
    (void)manager;
    (void)port;
    errorOut = "the REST server is not available in the WebAssembly build";
    return 1;
#else
    std::ostringstream banner;
    banner << "[REST] listening on http://127.0.0.1:" << port << " — Ctrl-C to stop\n";
    std::cout << banner.str();

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        errorOut = "WSAStartup failed";
        return 1;
    }
#define CLOSESOCK closesocket
#else
#define CLOSESOCK ::close
#endif

    const int fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (fd < 0)
    {
        errorOut = "socket() failed";
        return 1;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // never exposed to the LAN
    addr.sin_port = htons(static_cast<unsigned short>(port));
    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
    {
        errorOut = "bind() failed — is the port already taken?";
        CLOSESOCK(fd);
        return 1;
    }
    if (listen(fd, 8) != 0)
    {
        errorOut = "listen() failed";
        CLOSESOCK(fd);
        return 1;
    }

    for (;;)
    {
        const int conn = static_cast<int>(accept(fd, nullptr, nullptr));
        if (conn < 0)
            continue;
        std::string request;
        char buf[1024];
        while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192)
        {
            const int got = static_cast<int>(recv(conn, buf, sizeof buf, 0));
            if (got <= 0)
                break;
            request.append(buf, static_cast<std::size_t>(got));
        }
        const std::size_t lineEnd = request.find("\r\n");
        const std::string line =
            request.substr(0, lineEnd == std::string::npos ? request.size() : lineEnd);
        const Request req = parseRequestLine(line);
        Response resp;
        if (req.method.empty())
            resp = errorJson(400, "malformed request");
        else if (req.method != "GET")
            resp = errorJson(405, "read-only API: GET only");
        else
            resp = routeGet(manager, req.path, req.query);
        const std::string reply = httpResponse(resp);
        send(conn, reply.data(), static_cast<int>(reply.size()), 0);
        CLOSESOCK(conn);
        std::printf("[REST] %s %s -> %d\n", req.method.c_str(), req.path.c_str(), resp.status);
        std::fflush(stdout);
    }
#ifdef _WIN32
    WSACleanup();
#undef CLOSESOCK
#else
#undef CLOSESOCK
#endif
#endif
}

} // namespace rest
