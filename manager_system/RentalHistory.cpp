#include "RentalHistory.h"

#include <filesystem>
#include <fstream>
#include <sstream>

/* ============================================================
 *  CSV round-trip (data/rental_history.csv, v3.0.0)
 * ============================================================ */

bool RentalHistory::saveCSV(const std::string &path) const
{
    const std::filesystem::path parent =
        std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);

    std::ofstream file(path);
    if (!file)
        return false;

    file << "carName,customer,startDate,endDate,returnDate,"
            "base,discount,penalty,total\n";
    for (const RentalRecord &r : records_)
    {
        file << r.carName << ',' << r.customer << ',' << r.startISO << ','
             << r.endISO << ',' << r.returnISO << ',' << r.base << ','
             << r.discount << ',' << r.penalty << ',' << r.total << '\n';
    }
    return true;
}

bool RentalHistory::loadCSV(const std::string &path)
{
    records_.clear();

    std::ifstream file(path);
    if (!file)
        return true; // fresh install: an empty ledger is a valid state

    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::string carName, customer, startISO, endISO, returnISO;
        std::string baseStr, discountStr, penaltyStr, totalStr;
        std::getline(ss, carName, ',');
        std::getline(ss, customer, ',');
        std::getline(ss, startISO, ',');
        std::getline(ss, endISO, ',');
        std::getline(ss, returnISO, ',');
        std::getline(ss, baseStr, ',');
        std::getline(ss, discountStr, ',');
        std::getline(ss, penaltyStr, ',');
        std::getline(ss, totalStr, ',');

        // A corrupt row is dropped, not fatal — same rule as every
        // other loader in this project.
        try
        {
            RentalRecord record(carName, customer, startISO, endISO,
                                returnISO, std::stod(baseStr),
                                std::stod(discountStr),
                                std::stod(penaltyStr),
                                std::stod(totalStr));
            records_.push_back(record);
        }
        catch (...)
        {
            continue;
        }
    }
    return true;
}
