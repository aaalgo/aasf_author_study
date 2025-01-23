#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>      // for std::exit
#include <iomanip>      // for std::setprecision

// Include nlohmann's JSON library (you need to have the header available)
#include <nlohmann/json.hpp>
#include "match.h"      // Your matching interface and implementations

// For convenience
using json = nlohmann::json;

// -----------------------------------------------------------------------------
// Structures to hold CSV data and JSON data
// -----------------------------------------------------------------------------
struct CsvRow {
    std::string firstName;
    std::string lastName;
    std::string email;
    std::string institution; // e.g., "MIT"
    double amount;
};

struct JsonAuthor {
    int64_t id;           // e.g., "123"
    std::string displayName;   // e.g., "John Doe"
    std::vector<std::string> alternative_names;
};

struct JsonInstitution {
    int64_t id;           // e.g., "inst-001"
    std::string displayName;  // e.g., "Massachusetts Institute of Technology"
    std::vector<JsonAuthor> authors;
};

// -----------------------------------------------------------------------------
// Naive CSV parser (just for demonstration)
// -----------------------------------------------------------------------------
std::vector<CsvRow> loadCsv(const std::string &csvPath)
{
    std::vector<CsvRow> rows;

    std::ifstream in(csvPath);
    if (!in.is_open()) {
        std::cerr << "Error: cannot open CSV file: " << csvPath << std::endl;
        std::exit(1);
    }

    std::string headerLine;
    if (!std::getline(in, headerLine)) {
        std::cerr << "Error: empty CSV file or no header.\n";
        std::exit(1);
    }
    // We expect columns: first_name, last_name, email, institution, amount
    // Let's parse line-by-line. A robust parser might handle quotes, commas, etc.

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        CsvRow row;
        std::string amountStr;

        // We’ll assume these columns come in the correct order:
        // first_name,last_name,email,institution,amount
        // NOTE: This is naive – real CSV parsing can be more complicated.
        if (!std::getline(ss, row.firstName, ',')) continue;
        if (!std::getline(ss, row.lastName, ','))   continue;
        if (!std::getline(ss, row.email, ','))      continue;
        if (!std::getline(ss, row.institution, ',')) continue;
        if (!std::getline(ss, amountStr))      continue;

        // Remove double quotes from institution name if present
        if (row.institution.size() >= 2 && 
            row.institution.front() == '"' && 
            row.institution.back() == '"') {
            row.institution = row.institution.substr(1, row.institution.size() - 2);
        }

        // Convert firstName, lastName, and institution to lowercase for case-insensitive matching
        std::transform(row.firstName.begin(), row.firstName.end(), row.firstName.begin(), ::tolower);
        std::transform(row.lastName.begin(), row.lastName.end(), row.lastName.begin(), ::tolower); 
        std::transform(row.institution.begin(), row.institution.end(), row.institution.begin(), ::tolower);

        try {
            row.amount = std::stod(amountStr);
        } catch (...) {
            row.amount = 0.0;
        }
        if (rows.empty()) {
            std::cout << "email top lines:" << std::endl;
            std::cout << headerLine << std::endl;
            std::cout << line << std::endl;
            std::cout << "parsed: " << row.firstName << " " << row.lastName << " " << row.email << " " << row.institution << " " << row.amount << std::endl;
        }

        rows.push_back(row);
    }
    std::cout << "\nFirst 5 rows of CSV data:" << std::endl;
    for (size_t i = 0; i < std::min(rows.size(), size_t(5)); ++i) {
        const auto& row = rows[i];
        std::cout << row.firstName << ", " 
                  << row.lastName << ", "
                  << row.email << ", "
                  << row.institution << ", "
                  << row.amount << std::endl;
    }

    return rows;
}

// -----------------------------------------------------------------------------
// Load JSON file: expects a list of JSON objects, each with "id", "display_name",
// and "authors" (an array of { "id":..., "display_name":... }).
// -----------------------------------------------------------------------------
void loadJsonInstitutions(const std::string &jsonPath, std::vector<JsonInstitution> *ptr)
{
    std::ifstream in(jsonPath);
    if (!in.is_open()) {
        std::cerr << "Error: cannot open JSON file: " << jsonPath << std::endl;
        std::exit(1);
    }

    json j;
    try {
        std::cout << "Parsing JSON file: " << jsonPath << std::endl;
        in >> j;
        std::cout << "JSON parsing completed successfully." << std::endl;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        std::cerr << "Error at byte offset: " << e.byte << "\n";
        // Print some context around the error location
        in.clear();
        in.seekg(std::max(0LL, static_cast<long long>(e.byte) - 50));
        std::string context;
        std::getline(in, context);
        std::cerr << "Context around error:\n" << context << "\n";
        std::cerr << std::string(std::min(50UL, context.length()), '~') << "^\n";
        std::exit(1);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << "\n";
        std::exit(1);
    }

    if (!j.is_array()) {
        std::cerr << "Error: top-level JSON is not an array.\n";
        std::exit(1);
    }

    std::vector<JsonInstitution> institutions;
    int author_count = 0;
    for (auto &elem : j) {
        JsonInstitution inst;
        // Safely extract fields
        //std::cout << "Processing institution:"  << elem << std::endl;
        inst.id = strtoll(elem["id"].get<std::string>().c_str(), nullptr, 10);
        inst.displayName = elem.value("display_name", "");
        std::transform(inst.displayName.begin(), inst.displayName.end(), inst.displayName.begin(), ::tolower);
        if (elem.contains("authors") && elem["authors"].is_array()) {
            for (auto &auth : elem["authors"]) {
                JsonAuthor ja;
                ja.id = strtoll(auth["id"].get<std::string>().c_str(), nullptr, 10);
                ja.displayName = auth.value("display_name", "");
                std::transform(ja.displayName.begin(), ja.displayName.end(), ja.displayName.begin(), ::tolower);

                if (auth.contains("display_name_alternatives")) {
                    for (auto const &name : auth["display_name_alternatives"]) {
                        std::string lower_name = name;
                        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                        ja.alternative_names.push_back(lower_name);
                    }
                }
                
                inst.authors.push_back(ja);
                author_count++;
            }
        }
        institutions.push_back(std::move(inst));
    }
    std::cout << "Number of JSON institutions loaded: " << institutions.size() << "\n";
    std::cout << "Number of JSON authors loaded: " << author_count << "\n";
    std::cout << "\nFirst 5 institutions:\n";
    for (size_t i = 0; i < std::min(size_t(5), institutions.size()); ++i) {
        std::cout << "Institution " << i+1 << ":\n";
        std::cout << "  ID: " << institutions[i].id << "\n";
        std::cout << "  Name: " << institutions[i].displayName << "\n";
        std::cout << "  Number of authors: " << institutions[i].authors.size() << "\n";
    }
    std::cout << "\n";
    ptr->swap(institutions);
}

// -----------------------------------------------------------------------------
// Find the best matching institution in JSON by comparing s with inst.displayName
// Uses the given StringMatcher. Returns (indexInVector, bestRatio).
// If no good match found, returns (size_t(-1), 0.0).
// -----------------------------------------------------------------------------
std::pair<size_t,double> bestInstitutionMatch(
    const std::string &s,
    const std::vector<JsonInstitution> &jsonInsts,
    const StringMatcher &matcher)
{
    size_t bestIdx = size_t(-1);
    double bestRatio = 0.0;

    for (size_t i = 0; i < jsonInsts.size(); ++i) {
        double r = matcher.match(s, jsonInsts[i].displayName);
        if (r > bestRatio) {
            bestRatio = r;
            bestIdx = i;
        }
    }
    return {bestIdx, bestRatio};
}

// -----------------------------------------------------------------------------
// Find the best matching author in a JSON institution by comparing s with
// each author's displayName. Returns (indexInVector, bestRatio).
// If no good match found, returns (size_t(-1), 0.0).
// -----------------------------------------------------------------------------
std::pair<size_t,double> bestAuthorMatch(
    const std::string &s,
    const std::vector<JsonAuthor> &authors,
    const StringMatcher &matcher)
{
    size_t bestIdx = size_t(-1);
    double bestRatio = 0.0;

    for (size_t i = 0; i < authors.size(); ++i) {
        double r = matcher.match(s, authors[i].displayName);
        for (auto const &name: authors[i].alternative_names) {
            double r2 = matcher.match(s, name);
            if (r2 > r) {
                r = r2;
            }
        }
        if (r > bestRatio) {
            bestRatio = r;
            bestIdx = i;
        }
    }
    return {bestIdx, bestRatio};
}

// -----------------------------------------------------------------------------
// Command-line parsing
// -----------------------------------------------------------------------------
struct CmdArgs {
    std::string jsonFile;
    std::string csvFile;
    std::string outFile;
    double instThreshold;
    double nameThreshold;
    bool useEditDistance; // e.g. user can select which matcher
};

CmdArgs parseArgs(int argc, char** argv)
{
    CmdArgs args;
    args.jsonFile = "data/list/institutions.json";
    args.csvFile  = "data/NSF/emails.csv";
    args.outFile  = "data/NSF/matched_authors.csv";
    args.instThreshold = 0.85;
    args.nameThreshold = 0.9;
    args.useEditDistance = false;

    for (int i = 1; i < argc; ++i) {
        std::string opt = argv[i];
        if ((opt == "--json" || opt == "-j") && i+1 < argc) {
            args.jsonFile = argv[++i];
        }
        else if ((opt == "--csv" || opt == "-c") && i+1 < argc) {
            args.csvFile = argv[++i];
        }
        else if ((opt == "--output" || opt == "-o") && i+1 < argc) {
            args.outFile = argv[++i];
        }
        else if (opt == "--inst_threshold" && i+1 < argc) {
            args.instThreshold = std::stod(argv[++i]);
        }
        else if (opt == "--name_threshold" && i+1 < argc) {
            args.nameThreshold = std::stod(argv[++i]);
        }
        else if (opt == "--edit_distance") {
            args.useEditDistance = true;
        }
        else if (opt == "--help" || opt == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                     << "Options:\n"
                     << "  -j, --json FILE           JSON institutions file (default: data/list/institutions.json)\n"
                     << "  -c, --csv FILE            CSV emails file (default: /home/wdong/crawl/NSF/emails.csv)\n" 
                     << "  -o, --output FILE         Output file (default: matched_authors.csv)\n"
                     << "  --inst_threshold VALUE    Institution matching threshold (default: 0.9)\n"
                     << "  --name_threshold VALUE    Author name matching threshold (default: 0.9)\n"
                     << "  --edit_distance          Use edit distance matcher instead of Ratcliff/Obershelp\n"
                     << "  -h, --help               Show this help message\n";
            std::exit(0);
        }
        else {
            std::cerr << "Unknown option: " << opt << std::endl;
            std::exit(1);
        }
    }
    std::cout << "Command line arguments:\n"
              << "  JSON file: " << args.jsonFile << "\n"
              << "  CSV file: " << args.csvFile << "\n" 
              << "  Output file: " << args.outFile << "\n"
              << "  Institution threshold: " << args.instThreshold << "\n"
              << "  Name threshold: " << args.nameThreshold << "\n"
              << "  Using edit distance: " << (args.useEditDistance ? "yes" : "no") << "\n";
    return args;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    // 1) Parse cmd line
    CmdArgs args = parseArgs(argc, argv);

    // 2) Load data
    std::cout << "Loading JSON from: " << args.jsonFile << std::endl;
    std::vector<JsonInstitution> jsonInsts;
    loadJsonInstitutions(args.jsonFile, &jsonInsts);
    std::cout << "Number of JSON institutions loaded: " << jsonInsts.size() << "\n";

    std::cout << "Loading CSV from: " << args.csvFile << std::endl;
    auto csvData = loadCsv(args.csvFile);
    std::cout << "Number of CSV rows: " << csvData.size() << "\n";

    // 3) Group CSV rows by institution in an unordered_map
    std::vector<std::pair<std::string, std::vector<CsvRow>>> csvGroups;
    {
        std::unordered_map<std::string, std::vector<CsvRow>> csvGroupsMap;
        for (auto &row : csvData) {
            csvGroupsMap[row.institution].push_back(row);
        }
        std::cout << "Number of distinct CSV institutions: " << csvGroupsMap.size() << "\n";
        csvGroups.reserve(csvGroupsMap.size());
        for (auto &kv : csvGroupsMap) {
            csvGroups.push_back(kv);
        }
    }
    // 4) Create chosen matcher (Ratcliff/Obershelp or EditDistance)
    RatcliffObershelpMatcher roMatcher;
    EditDistanceMatcher edMatcher;
    const StringMatcher &instMatcher = (args.useEditDistance ? (const StringMatcher&)edMatcher
                                                            : (const StringMatcher&)roMatcher);
    const StringMatcher &nameMatcher = instMatcher; 
    // (We could choose separate matchers for institution vs. name if desired.)

    // 5) For each CSV institution, find best JSON institution => match ratio
    //    Then for each row in that institution, find best JSON author => match ratio
    std::vector<
        std::tuple<
            int64_t,     // institute_id
            std::string, // institute_name
            std::string, // matched_csv_institute
            double       // inst_match_ratio
        >
    > instMatchedResults;
    std::vector<
        std::tuple<
            int64_t,
            std::string, // author_name
            std::string, // author_name
            int64_t,     // institute_id
            std::string, // institute_name
            std::string, // matched_csv_institute
            double,      // amount_received
            std::string, // email
            double,      // person_match_ratio
            double       // inst_match_ratio
        >
    > matchedResults;

    matchedResults.reserve(csvData.size()); // rough upper bound

    size_t progressCount = 0;
    #pragma omp parallel for
    for (int i = 0; i < csvGroups.size(); ++i) {
        // Just a small progress indicator
        #pragma omp critical
        {
            ++progressCount;
            if (progressCount % 100 == 0) {
                std::cout << "Processing " << progressCount << " CSV institutions out of "
                        << csvGroups.size() << "\n";
            }
        }
        auto &kv = csvGroups[i];
        const std::string &csvInstName = kv.first;               // e.g. "MIT"
        auto &rowsThisInst = kv.second;                          // vector of CSV rows
        auto [bestIdx, instRatio] = bestInstitutionMatch(csvInstName, jsonInsts, instMatcher);

        // If no match or ratio < threshold, skip
        if (bestIdx == size_t(-1) || instRatio < args.instThreshold) {
            continue;
        }

        // We have a best JSON institution
        const auto &bestInst = jsonInsts[bestIdx]; // e.g. "Massachusetts Institute of Technology"


        #pragma omp critical
        instMatchedResults.push_back({
            bestInst.id,
            bestInst.displayName,
            csvInstName,
            instRatio
        });


        // For each CSV row in this institution, attempt to match the author
        for (auto &row : rowsThisInst) {
            // build full name from CSV
            std::string csvFullName = row.firstName + " " + row.lastName;

            auto [bestAuthorIdx, nameRatio] = bestAuthorMatch(csvFullName, bestInst.authors, nameMatcher);
            if (bestAuthorIdx == size_t(-1) || nameRatio < args.nameThreshold) {
                continue;
            }

            // We have a best author
            const auto &jsonAuth = bestInst.authors[bestAuthorIdx];
            // Store result
            #pragma omp critical
            matchedResults.push_back({
                jsonAuth.id,
                jsonAuth.displayName,
                csvFullName,
                bestInst.id,
                bestInst.displayName,
                csvInstName,
                row.amount,
                row.email,
                nameRatio,
                instRatio
            });
        }

    }

    // 6) Write the results to an output CSV
    std::ofstream out_inst(args.outFile + ".inst.csv");
    out_inst << "institute_id,institute_name,matched_csv_institute,inst_match_ratio\n";
    std::sort(instMatchedResults.begin(), instMatchedResults.end(), 
        [](const auto& a, const auto& b) {
            return std::get<3>(a) < std::get<3>(b); // Sort by score (4th element)
        });
    for (auto &tup : instMatchedResults) {
        out_inst << std::get<0>(tup) << "," << std::get<1>(tup) << "," << std::get<2>(tup) << "," << std::get<3>(tup) << "\n";
    }
    out_inst.close();   



    std::ofstream out(args.outFile);
    if (!out.is_open()) {
        std::cerr << "Error: cannot open output file: " << args.outFile << "\n";
        return 1;
    }

    std::sort(matchedResults.begin(), matchedResults.end(),
        [](const auto& a, const auto& b) {
            return std::min<double>(std::get<8>(a), std::get<9>(a)) < std::min<double>(std::get<8>(b), std::get<9>(b)); // Sort by person_match_ratio (8th element)
        });


    // Write header
    out << "author_id,author_name,csv_author_name,institute_id,institute_name,"
        << "matched_csv_institute,amount_received,email,"
        << "person_match_ratio,inst_match_ratio\n";

    for (auto &tup : matchedResults) {
        // unpack the tuple
        const auto &authorId            = std::get<0>(tup);
        const auto &authorName          = std::get<1>(tup);
        const auto &csvAuthorName       = std::get<2>(tup);
        const auto &instId              = std::get<3>(tup);
        const auto &instName            = std::get<4>(tup);
        const auto &matchedCsvInst      = std::get<5>(tup);
        double amount                    = std::get<6>(tup);
        const auto &email               = std::get<7>(tup);
        double personMatchRatio         = std::get<8>(tup);
        double instMatchRatio           = std::get<9>(tup);

        out << authorId << ","
            << "\"" << authorName << "\","
            << "\"" << csvAuthorName << "\","
            << instId << ","
            << "\"" << instName << "\","
            << "\"" << matchedCsvInst << "\","
            << amount << ","
            << "\"" << email << "\","
            << personMatchRatio << ","
            << instMatchRatio << "\n";
    }

    out.close();

    // 7) Print a summary
    std::cout << "\nOutput CSV written to: " << args.outFile << "\n";
    std::cout << "===== MATCH SUMMARY =====\n";
    size_t totalJsonAuthors = 0;
    for (auto &inst : jsonInsts) {
        totalJsonAuthors += inst.authors.size();
    }
    std::cout << "Total JSON institutions: " << jsonInsts.size() << "\n";
    std::cout << "Total JSON authors:      " << totalJsonAuthors << "\n";
    std::cout << "Total CSV rows:         " << csvData.size() << "\n";
    std::cout << "Number of matched rows: " << matchedResults.size() << "\n";

    return 0;
}
