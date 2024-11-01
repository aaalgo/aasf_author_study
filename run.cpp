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
#include <xtensor/xnpy.hpp>

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
int constexpr COUNTRY_OTHER = 0x04;
int constexpr YEAR_BEGIN = 1990;
int constexpr YEAR_END = 2030;
int constexpr TOTAL_YEARS = YEAR_END - YEAR_BEGIN;

string const EnCS_DOMAIN_NAME = "Engineering and Computer Science";
int constexpr EnCS_DOMAIN_ID = -2; // openalex does not have this domain
                                    // but it is used in the study
int constexpr FIELD_ENGINEERING = 22;
int constexpr FIELD_CS = 17;

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

    bool has_gap () const {
        // where there's a gap of more than 5 years
        vector<int> years;
        for (int i = 0; i < TOTAL_YEARS; ++i) {
            if (at(i) > 0) years.push_back(i);
        }
        for (int i = 1; i < years.size(); ++i) {
            if (years[i] - years[i - 1] > 5) return true;
        }
        return false;
    }

    int migrate_year_offset () const {
        if (has_gap()) return -1;
        // This criteria results in more results than previous study
        int off = 0;
        while (off < TOTAL_YEARS && !(at(off) & COUNTRY_US)) ++off;
        if (off >= TOTAL_YEARS) return -1;
        int first_us = off;
        off = 0;
        while (off < TOTAL_YEARS && !(at(off) & COUNTRY_CN)) ++off;
        if (off >= TOTAL_YEARS) return -1;
        int first_cn = off;
        if (!(first_us < first_cn)) return -1;

        off = TOTAL_YEARS - 1;
        // find the first appearance of CN backward
        while (off >= 0 && !(at(off) & COUNTRY_CN)) --off;
        if (off < 0) return -1;
        int last_cn = off;
        while (off >= 0 && !(at(off) & COUNTRY_US)) --off;
        if (off < 0) return -1;
        int last_us = off;
        if (!(last_us < last_cn)) return -1;
        // we are sure now that last_us < last_cn
        if (at(last_us) & COUNTRY_CN) return last_us;
        return last_us + 1;
    }

#if 0
    int migrate_year_offset_strict () const {
        // detect the year when the author moves from US to CN
        // if not detected return -1
        // Criteria
        // there exists a year N such that
        // 1. author is in CN at year N
        // 2. for all n > N, author is in CN and not in US
        // 3. for all n < N, author is in US only
        // 4. author must be in US in N or N-1
        // we don't count people who move back and forth
        int off = 0;
        while (off < TOTAL_YEARS && !(at(off) & COUNTRY_US)) ++off;
        if (off >= TOTAL_YEARS) return -1;
        int first_us = off;
        off = 0;
        while (off < TOTAL_YEARS && !(at(off) & COUNTRY_CN)) ++off;
        if (off >= TOTAL_YEARS) return -1;
        int first_cn = off;
        if (!(first_us < first_cn)) return -1;

        off = TOTAL_YEARS - 1;
        // find the first appearance of CN backward
        while (off >= 0 && !(at(off) & COUNTRY_CN)) --off;
        if (off < 0) return -1;
        int last_cn = off;
        while (off >= 0 && !(at(off) & COUNTRY_US)) --off;
        if (off < 0) return -1;
        int last_us = off;
        if (!(last_us < last_cn)) return -1;

        if (first_cn < last_us) return -1;
        // we are sure now that last_us < last_cn
        if (at(last_us) & COUNTRY_CN) return last_us;
        return last_us + 1;
    }
#endif
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

int constexpr DOMAIN_LEVEL_DOMAIN = 0;
int constexpr DOMAIN_LEVEL_FIELD = 1;
int constexpr NUM_LEVELS = 10;

struct Domain {
    // this covers fields and domains
    // id = openalex_id * NUM_LEVELS + level
    static string const URL_PREFIX;
    openalex_id_t id;
    string display_name;
    string url () const {
        return format("{}{}", URL_PREFIX, id);
    }
    Domain (): id(-1) {}
    Domain (json const &j) {
        id = extract_id(j["id"], URL_PREFIX);
        display_name = j["display_name"];
    }
};

string const Domain::URL_PREFIX = "https://openalex.org/domains/";
string const FIELD_URL_PREFIX = "https://openalex.org/fields/";

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
            bool has_en_cs = false;
            for (auto const &topic : j["topics"]) {
                Domain domain(topic["domain"]);
                domains[domain.id] = domain.display_name;

                int field_id = extract_id(topic["field"]["id"], FIELD_URL_PREFIX);
                if (field_id < 0) {
                    cerr << "Invalid field ID: " << topic["field"]["id"] << endl;
                    throw 0;
                }
                if (field_id == FIELD_ENGINEERING || field_id == FIELD_CS) {
                    has_en_cs = true;
                }
            }
            if (has_en_cs) {
                domains[EnCS_DOMAIN_ID] = EnCS_DOMAIN_NAME;
            }
        }
        if (j.contains("affiliations")) {
            for (auto const &affiliation : j["affiliations"]) {
                string country = affiliation["institution"]["country_code"];
                int country_code = COUNTRY_OTHER;
                if (country == "US") country_code = COUNTRY_US;
                else if (country == "CN") country_code = COUNTRY_CN;
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
            jdomains.push_back({{"id", id},
                                {"display_name", name}});
        }
        (*j)["domains"] = jdomains;
        json jyears;
        years.encode(&jyears);    
        (*j)["years"] = jyears;
    }
};

string const Author::URL_PREFIX("https://openalex.org/A");

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

struct DomainCount: public Domain {
    xt::xtensor_fixed<int, xt::xshape<TOTAL_YEARS>> counts;
    DomainCount (): Domain() { counts.fill(0); }

    void add (int id, string const &name, int year_offset, int delta = 1) {
        if (this->display_name.empty()) {
            this->id = id;
            this->display_name = name;
        }
        counts[year_offset] += delta;
    }

    void merge (DomainCount const &other) {
        add(other.id, other.display_name, 0, 0);
        counts += other.counts;
    }
};

struct Survey {
    unordered_map<openalex_id_t, DomainCount> domains;
public:
    void add (Author const &author) {
        int year_offset = author.years.migrate_year_offset();
        if (year_offset < 0) return;
        for (auto const &[id, name] : author.domains) {
            domains[id].add(id, name, year_offset);
        }
    }
    void merge (Survey const &other) {
        for (auto const &[id, count] : other.domains) {
            domains[id].merge(count);
        }
    }
    void save (string const &path) const {
       json meta;
       meta["year_begin"] = YEAR_BEGIN;
       meta["year_end"] = YEAR_END;
       json jdomains = json::array();
       xt::xtensor<int, 2> counts;
       counts.resize({domains.size(), TOTAL_YEARS});
       int i = 0;
       for (auto const &[id, domain]: domains) {
           jdomains.push_back({{"id", domain.id},
                               {"display_name", domain.display_name}});
           xt::view(counts, i, xt::all()) = domain.counts;
           ++i;
       }
       meta["domains"] = jdomains;
       fs::create_directories(path);
       ofstream os(path + "/meta.json");
       os << meta.dump(2) << endl;
       xt::dump_npy(path + "/counts.npy", counts);
    }
};

void count_migration (string const &datadir, string const &outdir) {
    vector<string> files;
    scan_files(datadir, &files);
    cout << "Found " << files.size() << " files" << endl;
    int done = 0;
    Survey survey;
    #pragma omp parallel for
    for (size_t i = 0; i < files.size(); ++i) {
        bxz::ifstream iss(files[i]);
        string line;
        Survey local;
        while (getline(iss, line)) {
            try {
                Author author(json::parse(line));
                local.add(author);
            } catch (const json::exception& e) {
                errors::bad_json += 1;
            }
        }
        #pragma omp critical
        {
            survey.merge(local);
            ++done;
            cout << format("Processed {}/{}", done, files.size()) << endl;
        }
    }
    cerr << format("Errors: {} bad JSON, {} invalid IDs", errors::bad_json.load(), errors::invalid_id.load()) << endl;
    survey.save(outdir);
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
        if (argc < 3) {
            cerr << "Usage: " << argv[0] << " test <out_dir>" << endl;
        }
        else {
            count_migration("data/filtered", argv[2]);
        }
    }
    return 0;
}

