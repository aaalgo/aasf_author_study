#include <atomic>
#include <array>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <format>
#include <nlohmann/json.hpp>
#define BXZSTR_CONFIG_HPP
#define BXZSTR_Z_SUPPORT 1
#define BXZSTR_BZ2_SUPPORT 0
#define BXZSTR_LZMA_SUPPORT 0
#define BXZSTR_ZSTD_SUPPORT 0
#include <bxzstr.hpp>
#include <xtensor/xtensor.hpp>
#include <xtensor/xfixed.hpp>
#include <xtensor/xview.hpp>
#include <xtensor/xio.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;
using std::atomic;
using std::ifstream;
using std::ofstream;
using std::array;
using std::cout;
using std::cerr;
using std::endl;
using std::format;
using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;

typedef int64_t openalex_id_t;
openalex_id_t constexpr INVALID_ID = -1;
int constexpr COUNTRY_US = 0x01;
int constexpr COUNTRY_CN = 0x02;
int constexpr YEAR_BEGIN = 1990;
int constexpr YEAR_END = 2030;
int constexpr TOTAL_YEARS = YEAR_END - YEAR_BEGIN;

namespace errors {
    atomic<int> bad_json(0);
    atomic<int> invalid_id(0);
};

class Surnames {
    unordered_set<string> surnames;
    static Surnames singleton;
    Surnames () {
        ifstream is("data/surnames.json");
        json j = json::parse(is);
        for (string const &name : j) {
            surnames.insert(name);
        }
        #if 0
        for (string const &name : surnames) {
            cout << name << endl;
        }
        #endif
        cerr << "Loaded " << surnames.size() << " Chinese surnames" << endl;
    }

public:
    static bool is_chinese (string const &name) {
        size_t space = name.rfind(' ');
        if (space == string::npos) return false;
        string last = name.substr(space + 1);
        transform(last.begin(), last.end(), last.begin(), ::tolower);
        return singleton.surnames.find(last) != singleton.surnames.end();
    }
};

Surnames Surnames::singleton;

class YearMask: array<uint8_t, TOTAL_YEARS> {
public:
    YearMask () { fill(0); }
    void add (int year, uint8_t value) {
        if (year < YEAR_BEGIN || year >= YEAR_END) {
            //cerr << "Invalid year: " << year << endl;
            return;
        }
        at(year - YEAR_BEGIN) |= value;
    }

    int count (uint8_t value) const {
        int cnt = 0;
        for (int i = 0; i < TOTAL_YEARS; ++i) {
            if (at(i) & value) ++cnt;
        }
        return cnt;
    }

    void encode (json *j) const {
        std::vector<int> us;
        std::vector<int> cn;
        for (int i = 0; i < TOTAL_YEARS; ++i) {
            if (at(i) & COUNTRY_US) us.push_back(i + YEAR_BEGIN);
            if (at(i) & COUNTRY_CN) cn.push_back(i + YEAR_BEGIN);
        }
        (*j)["us_years"] = us;
        (*j)["cn_years"] = cn;
    }

    int move_year () {
        // detect the year when the author moves from US to CN
        // if not detected return -1
        // Criteria
        // there exists a year N such that
        // 1. author is in CN at year N
        // 2. for all n > N, author is in CN and not in US
        // 3. for all n < N, author is in US only
        // 4. author must be in US in N or N-1
        // we don't count people who move back and forth
        return 0;
    }
};

openalex_id_t extract_id (string const &url, string const &prefix) {
    if (!url.starts_with(prefix)) {
        errors::invalid_id += 1;
        return INVALID_ID;;
    }
    string idstr = url.substr(prefix.size());
    openalex_id_t id = std::stoll(idstr);
    if (id < 0) {
        errors::invalid_id += 1;
        return INVALID_ID;;
    }
    return id;
}

struct Domain {
    static string const URL_PREFIX;
    openalex_id_t id;
    string display_name;
    string url () const {
        return format("{}/{}", URL_PREFIX, id);
    }
    Domain (): id(-1) {}
    Domain (json const &j): id(extract_id(j["id"], URL_PREFIX)) {
        display_name = j["display_name"];
    }
};

string const Domain::URL_PREFIX("https://openalex.org/domains/");

struct DomainInfo: public Domain {
    xt::xtensor_fixed<int, xt::xshape<TOTAL_YEARS>> counts;
    DomainInfo (): Domain() { counts.fill(0); }
};

struct Author {
    static string const URL_PREFIX;
    openalex_id_t id;
    string display_name;
    unordered_map<openalex_id_t, string> domains;
    YearMask years;
    Author (json const &j) {
        id = extract_id(j["id"], URL_PREFIX);
        display_name = j["display_name"];
        domains[INVALID_ID] = "All";
        if (j.contains("topics")) {
            for (auto const &topic : j["topics"]) {
                Domain domain(topic["domain"]);
                domains[domain.id] = domain.display_name;
            }
        }
        if (j.contains("affiliations")) {
            for (auto const &affiliation : j["affiliations"]) {
                string country = affiliation["institution"]["country_code"];
                int country_code = 0;
                if (country == "US") country_code = COUNTRY_US;
                else if (country == "CN") country_code = COUNTRY_CN;
                else continue;
                for (int year: affiliation["years"]) {
                    years.add(year, country_code);
                }
            }
        }
    }

    bool relevant () const {
        if (!Surnames::is_chinese(display_name)) return false;
        return years.count(COUNTRY_US) > 0 && years.count(COUNTRY_CN) > 0;
    }

    void encode (json *j) const {
        (*j)["id"] = id;
        (*j)["display_name"] = display_name;
        json jdomains = json::array();
        for (auto const &[id, name] : domains) {
            jdomains.push_back({{"id", id}, {"display_name", name}});
        }
        (*j)["domains"] = jdomains;
        json jyears;
        years.encode(&jyears);    
        (*j)["years"] = jyears;
    }
};

string const Author::URL_PREFIX("https://openalex.org/A");

class Survey {
    unordered_map<int, Domain> domains;
public:
};

// Function to process a file and extract matching authors
void test (string const &path) {
    ifstream is(path);
    for (;;) {
        string line;
        if (!getline(is, line)) break;
        try {
            Author author(json::parse(line));
            json out;
            author.encode(&out);
            cout << "================ " << author.id << endl;
            cout << out.dump() << endl;
        } catch (const json::exception& e) {
            cerr << "JSON parsing error: " << e.what() << endl;
            //cerr << line << endl;
        }
    }
}

void scan_files (string const &datadir, vector<string> *paths) {
    for (const auto& entry : fs::recursive_directory_iterator(datadir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gz") {
            paths->push_back(entry.path().string());
        }
    }
}

void filter_chinese (string const &datadir) {
    vector<string> files;
    scan_files(datadir, &files);
    cout << "Found " << files.size() << " files" << endl;
    int done = 0;
    int total_in = 0;
    int total_out = 0;
    fs::create_directory("data/filtered");
    #pragma omp parallel for
    for (size_t i = 0; i < files.size(); ++i) {
        bxz::ifstream iss(files[i]);
        bxz::ofstream oss(format("data/filtered/{}.gz", i), bxz::z);
        string line;
        int count_in = 0;
        int count_out = 0;
        while (getline(iss, line)) {
            try {
                Author author(json::parse(line));
                ++count_in;
                if (!author.relevant()) continue;
                ++count_out;
                oss << line << endl;
            } catch (const json::exception& e) {
                errors::bad_json += 1;
            }
        }
        #pragma omp critical
        {
            total_in += count_in;
            total_out += count_out;
            ++done;
            cout << format("Processed {}/{}: {} in {} out, ratio = {:.4f}",
                done, files.size(), count_in, count_out, 1.0 * count_out / count_in) << endl;
        }
    }
    cout << format("Total: {} in {} out, ratio = {:.4f}", total_in, total_out, 1.0 * total_out / total_in) << endl;
    cerr << format("Errors: {} bad JSON, {} invalid IDs", errors::bad_json.load(), errors::invalid_id.load()) << endl;
}

void count_migration (string const &datadir) {
    vector<string> files;
    scan_files(datadir, &files);
    cout << "Found " << files.size() << " files" << endl;
    int done = 0;
    #pragma omp parallel for
    for (size_t i = 0; i < files.size(); ++i) {
        bxz::ifstream iss(files[i]);
        string line;
        while (getline(iss, line)) {
            try {
                Author author(json::parse(line));
            } catch (const json::exception& e) {
                errors::bad_json += 1;
            }
        }
    }
    cerr << format("Errors: {} bad JSON, {} invalid IDs", errors::bad_json.load(), errors::invalid_id.load()) << endl;
}


int main(int argc, char **argv) {
    if (argc <= 1) {
        cerr << "Usage: " << argv[0] <<  " [test | filter | count]" << endl;
    }
    else if (strcmp(argv[1], "test") == 0) {
        if (argc < 3) {
            cerr << "Usage: " << argv[0] << " test <jsonl_file>" << endl;
        }
        else {
            test(argv[2]);
        }
    }
    else if (strcmp(argv[1], "filter") == 0) {
        filter_chinese("data/authors");
    }
    else if (strcmp(argv[1], "count") == 0) {
        count_migration("data/filtered");
    }
    return 0;
}

