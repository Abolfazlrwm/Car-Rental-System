#include "AuditLog.h"

#include <fstream>

/* ============================================================
 *  Reading the trail
 * ============================================================ */

std::vector<AuditEntry> AuditLog::all() const
{
    std::vector<AuditEntry> out;
    out.reserve(entries_.size());
    entries_.forEach([&out](const AuditEntry &e) { out.push_back(e); });
    return out;
}

std::vector<AuditEntry> AuditLog::recent(std::size_t n) const
{
    std::vector<AuditEntry> out = all();
    if (n == 0)
        out.clear();
    else if (out.size() > n)
        out.erase(out.begin(), out.end() - static_cast<long>(n));
    return out;
}

/* ============================================================
 *  CSV escaping (RFC 4180)
 * ============================================================ */

std::string AuditLog::escapeField(const std::string &raw)
{
    // Keep the file one-record-per-line: newlines inside details
    // become the two-character sequences \n / \r (unescaped on
    // load) and a literal backslash is doubled. Commas and quotes
    // then get RFC 4180 quoting.
    std::string flat;
    flat.reserve(raw.size() + 8);
    for (const char c : raw)
    {
        if (c == '\n')
            flat += "\\n";
        else if (c == '\r')
            flat += "\\r";
        else if (c == '\\')
            flat += "\\\\";
        else
            flat += c;
    }

    if (flat.find_first_of(",\"") == std::string::npos)
        return flat; // plain field, no quoting needed

    std::string out = "\"";
    for (const char c : flat)
    {
        if (c == '"')
            out += "\"\""; // doubled quote inside a quoted field
        else
            out += c;
    }
    out += '"';
    return out;
}

namespace
{

/*
 * Split one CSV line into exactly five fields, honoring double
 * quotes ("" inside a quoted field is a literal quote). Returns
 * false when the line does not have exactly five fields.
 */
bool splitCSV5(const std::string &line, std::string out[5])
{
    std::size_t field = 0;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size() && field < 5; ++i)
    {
        const char c = line[i];
        if (inQuotes)
        {
            if (c == '"')
            {
                if (i + 1 < line.size() && line[i + 1] == '"')
                {
                    out[field] += '"';
                    ++i; // skip the second quote of the pair
                }
                else
                {
                    inQuotes = false;
                }
            }
            else
            {
                out[field] += c;
            }
        }
        else if (c == '"')
        {
            inQuotes = true;
        }
        else if (c == ',')
        {
            ++field;
        }
        else
        {
            out[field] += c;
        }
    }
    return field == 4;
}

/// Inverse of the escaping in escapeField(): \n, \r and the
/// doubled backslash go back to their single-character forms.
std::string unescapeField(const std::string &in)
{
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '\\' && i + 1 < in.size())
        {
            const char next = in[i + 1];
            if (next == 'n')
            {
                out += '\n';
                ++i;
            }
            else if (next == 'r')
            {
                out += '\r';
                ++i;
            }
            else if (next == '\\')
            {
                out += '\\';
                ++i;
            }
            else
            {
                out += in[i];
            }
        }
        else
        {
            out += in[i];
        }
    }
    return out;
}

} // namespace

/* ============================================================
 *  Persistence
 * ============================================================ */

bool AuditLog::saveCSV(const std::string &path) const
{
    std::ofstream file(path);
    if (!file)
        return false;

    file << "date,actor,action,result,detail\n";
    entries_.forEach([&file](const AuditEntry &e)
                     {
                         file << escapeField(e.date) << ','
                              << escapeField(e.actor) << ','
                              << escapeField(e.action) << ','
                              << (e.allowed ? "ok" : "denied") << ','
                              << escapeField(e.detail) << '\n';
                     });
    return true;
}

bool AuditLog::loadCSV(const std::string &path)
{
    std::ifstream file(path);
    if (!file)
        return false;

    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        std::string f[5];
        if (!splitCSV5(line, f))
            continue; // skip malformed rows instead of aborting
        record(unescapeField(f[0]), unescapeField(f[1]),
               unescapeField(f[2]), unescapeField(f[4]),
               f[3] != "denied");
    }
    return true;
}
