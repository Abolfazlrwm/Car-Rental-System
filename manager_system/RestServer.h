/**
 * ============================================================
 *  Rest — read-only REST layer over the RentalManager (v3.7.0)
 * ============================================================
 *  A tiny HTTP server on plain sockets (POSIX / Winsock, both
 *  hand-rolled — still zero dependencies) that exposes the same
 *  numbers the manager's screens render, as JSON:
 *
 *    GET /health                    liveness + version
 *    GET /version                   version metadata
 *    GET /cars                      the fleet, name-sorted
 *    GET /cars/{name}               one car (404 when absent)
 *    GET /cars/{name}/reservations  live bookings on one car
 *    GET /stats                     FleetStats + waitlist depth
 *    GET /loyalty                   the program snapshot
 *    GET /audit?limit=N             newest audit entries (cap 500)
 *    GET /pickups?days=N            upcoming pickups (1..30)
 *    GET /monthly?year=Y&month=M    the monthly report
 *    GET /reports/monthly.png?year=Y&month=M   monthly as image
 *    GET /reports/loyalty.png       the loyalty raster as image
 *    GET /reports/dashboard.png     the dashboard raster as image
 *    GET /reports/all.pdf?year=Y&month=M   one consolidated PDF
 *
 *  Design contract:
 *    - READ-ONLY on purpose: mutations stay in the TUI where every
 *      change is validated, priced and audited interactively;
 *    - the router (routeGet) is pure — no sockets — so the unit
 *      suite tests every endpoint without networking;
 *    - the server binds 127.0.0.1 only (dev/automation use; expose
 *      it through `docker run -p` if you need it elsewhere);
 *    - runs with NO logged-in session, and the facade's permission
 *      gates pass through exactly in that programmatic mode — the
 *      same lane the JSON/CSV exports use;
 *    - the WebAssembly build has no sockets: serve() reports it is
 *      unavailable there (parse/route still compile and are tested).
 * ============================================================
 */
#ifndef REST_SERVER_H
#define REST_SERVER_H

#include <string>

class RentalManager;

namespace rest
{

struct Request
{
    std::string method;
    std::string path;
    std::string query;
};

/// Parse "GET /cars?a=1 HTTP/1.1". An empty method marks a
/// malformed line (the caller answers 400).
Request parseRequestLine(const std::string &line);

/// %XX and '+' decoding for path segments and query values.
std::string urlDecode(const std::string &text);

/// Lookup one key in an "a=1&b=2" query string ("" when absent).
std::string queryParam(const std::string &query, const std::string &key);

struct Response
{
    int status = 200;
    std::string body;
    std::string contentType = "application/json; charset=utf-8";
};

/// The pure router: every GET endpoint, no sockets involved.
Response routeGet(RentalManager &manager, const std::string &path,
                  const std::string &query);

/// Serialise a Response as a full HTTP/1.1 reply.
std::string httpResponse(const Response &response);

/// Blocking accept loop on 127.0.0.1:port. Returns non-zero (with
/// the reason in `errorOut`) when the socket cannot be set up.
int serve(RentalManager &manager, int port, std::string &errorOut);

} // namespace rest

#endif // REST_SERVER_H
