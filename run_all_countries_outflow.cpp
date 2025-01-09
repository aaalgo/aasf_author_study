#include <atomic>
#include <array>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
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
openalex_id_t constexpr INVALID_ID = -1;    // also used as the ID of domain "ALL"
uint32_t constexpr COUNTRY_ID_US = 0;
uint32_t constexpr COUNTRY_MASK_US = 1 << COUNTRY_ID_US;
int constexpr YEAR_BEGIN = 1990;
int constexpr YEAR_END = 2030;
int constexpr TOTAL_YEARS = YEAR_END - YEAR_BEGIN;

string const EnCS_DOMAIN_NAME = "Engineering and Computer Science";
int constexpr EnCS_DOMAIN_ID = -2;
// openalex does not have this domain
// but it is used in the study
// So we propote the subdomain to this special top-level ID
int constexpr FIELD_ENGINEERING = 22;
int constexpr FIELD_CS = 17;

namespace errors {
    atomic<int> bad_json(0);
    atomic<int> invalid_id(0);
};

// A dictionary to check if a name is Chinese
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

static char const *COUNTRY_CODES[] = {
    "US","CN","IN","CA","DE","FR","AU","KR","JP","CH","UK", "other", nullptr
    /*
    "US", "CN", "DE", "UK", "JP", "FR", "CA", "KR", 
    "CH", "AU", "IN", "IT", "ES", "NL", "SE", "IL", 
    "DK", "SG", "TW", "RU", "other", nullptr
    */
};
static constexpr int NUM_COUNTRIES = 12;
static constexpr int OTHER_COUNTRY_ID = NUM_COUNTRIES - 1;

std::mutex missing_country_stats_mutex;
unordered_map<string, int> missing_country_stats;

class CountryLookup {
    unordered_map<string, int> lookup;
    static CountryLookup singleton;
public:
    CountryLookup () {
        for (int i = 0; COUNTRY_CODES[i]; ++i) {
            lookup[string(COUNTRY_CODES[i])] = i;
        }
        cerr << "Loaded " << lookup.size() << " countries" << endl;
        if (lookup.size() >= 32) {
            cerr << "Too many countries: " << lookup.size() << endl;
            throw 0;
        }
        if (lookup.size() != NUM_COUNTRIES) {
            cerr << "Wrong number of countries: " << lookup.size() << endl;
            throw 0;
        }
        auto it = lookup.find("US");
        if (it == lookup.end() || it->second != COUNTRY_ID_US) {
            cerr << "US not found" << endl;
            throw 0;
        }
    }

    static int get (string const &code) {
        {
            std::lock_guard<std::mutex> lock(missing_country_stats_mutex);
            missing_country_stats[code] += 1;
        }
        auto it = singleton.lookup.find(code);
        if (it == singleton.lookup.end()) {
            return singleton.lookup.size() - 1;
        }
        return it->second;
    }
};

CountryLookup CountryLookup::singleton;

struct Migration {
    int year_offset;        // offset from YEAR_BEGIN
    uint32_t country_id;    // country_id == 0 means invalid
                            // because there's no migration
                            // we only study authors started in US
    Migration (): year_offset(-1), country_id(0) {}
    Migration (int year_offset_, uint32_t country_id_)
        : year_offset(year_offset_),
          country_id(country_id_) {
    }
};

// A mask to record the years when an author is in a country
// Each year entry is a bitmask of country IDs
class YearMask: array<uint32_t, TOTAL_YEARS> {
public:
    YearMask () { fill(0); }
    void add (int year, uint32_t country_id) {
        if (year < YEAR_BEGIN || year >= YEAR_END) {
            //cerr << "Invalid year: " << year << endl;
            return;
        }
        at(year - YEAR_BEGIN) |= 1 << country_id;
    }

/*
    int count (uint8_t value) const {
        int cnt = 0;
        for (uint32_t mask: *this) {
            if (mask & value) ++cnt;
        }
        return cnt;
        // Each year entry is a bitmask of country IDs
    }
    */

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

    bool relevant () const {
        auto mig = get_migration();
        return mig.country_id > 0;
    }

    Migration get_migration () const {
        // get migration info of this author based on the year masks
        Migration invalid;
        if (has_gap()) return invalid;
        // Rule 1.  The initial institute must be in US (trained in US)
        int off = 0;
        while ((off < size()) && (at(off) == 0)) ++off;
        if (off >= size()) return invalid;
        if ((at(off) & COUNTRY_MASK_US) == 0) return invalid;
        // Rule 2.  If no country history, the author is not relevant.
        off = TOTAL_YEARS - 1;
        while (off >= 0 && (at(off) == 0)) --off;
        if (off < 0) return invalid;
        // found the last year with non-zero mask
        int last_year = off;
        // Rule 3.  If the last year is in US, the author is not relevant.
        if (at(last_year) & COUNTRY_MASK_US) return invalid;
        // now we are sure the last year author is not in US
        // find the last year author was in US
        while (off >= 0 && ((at(off) & COUNTRY_MASK_US) == 0)) --off;
        // Rule 4.  If we cannot find a year author was in US, the author is not relevant.
        if (off < 0) return invalid;
        if ((at(off) & COUNTRY_MASK_US) == 0) throw 0;
        int last_us_year = off;
        // At this point, we are sure that:
        // - last_year was not in US
        // - last_us_year ( < last_year)was in US (but could also be in another country)
    
        // find the first non-US year before
        while (off >= 0 && ((at(off) & COUNTRY_MASK_US) || (at(off) == 0))) --off;
        //                  in US                        or UNKNOWN
        if (off < 0) {
            ; // OK, the author was only in US previously
        }
        else {
            int first_non_us_year_before = off;
        /*
        // Rule 5.  If the author stayed in US for less than 5 years before migration, the author is not relevant.
        // Rule 5 was rejected.
            if (last_us_year - first_non_us_year_before < 5) {
                //  N U U U U U
                return invalid; // less than 5 years in US
            }
        */
        }
        // now we are sure that
        // - author was a US person <= last_us_year
        // - author eventually migrated to another country
        off = last_us_year + 1;
        // find the first year the author was not in US and in a non-US country
        while (off < TOTAL_YEARS && ((at(off) == 0) || (at(off) & COUNTRY_MASK_US))) ++off;
        if (off >= TOTAL_YEARS) throw 0;
        uint32_t mask = at(off);
        if ((mask == 0) || (mask & COUNTRY_MASK_US)) throw 0;
        // at off, the author is only in a non-US country
        // find the destination country
        int country_id = __builtin_ctz(mask);   // here we assume the author is only in one country, if the author is in multiple countries, the most populus will be used
        int migration_year_off = off;
        // Rule 6. If there's an overlap year, the overlap year + 1 is the migration year,
        // otherwise, the migration year is the first year the author is in a non-US country
        if (at(last_us_year) & (1 << country_id)) {
            migration_year_off = last_us_year + 1;
        }
        return Migration(migration_year_off, country_id);
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
    // here's where we want to match the funding data
                int country_id = CountryLookup::get(country);
                if (country_id < 0) {
                    cerr << "Invalid country code: " << country << endl;
                    throw 0;
                }
                for (int year: affiliation["years"]) {
                    years.add(year, country_id);
                }
            }
        }
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
        //years.encode(&jyears);    
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

void filter_relevant (string const &datadir) {
    vector<string> files;
    scan_files(datadir, &files);
    cout << "Found " << files.size() << " files" << endl;
    int done = 0;
    int total_in = 0;
    int total_out = 0;
    fs::create_directory("data/filtered_all");
    #pragma omp parallel for
    for (size_t i = 0; i < files.size(); ++i) {
        bxz::ifstream iss(files[i]);
        bxz::ofstream oss(format("data/filtered_all/{}.gz", i), bxz::z);
        string line;
        int count_in = 0;
        int count_out = 0;
        while (getline(iss, line)) {
            try {
                Author author(json::parse(line));
                ++count_in;
                if (!author.years.relevant()) continue;
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

// migration count in each domain
struct DomainCount: public Domain {
    // Dim 0: 0 non-chinese, 1 chinese, 2 all
    // Dim 1: year
    // Dim 2: destination country
    xt::xtensor_fixed<int, xt::xshape<3, TOTAL_YEARS, NUM_COUNTRIES>> counts;
    DomainCount (): Domain() { counts.fill(0); }

    void add (int id, string const &name, int is_chinese, int year_offset, int country_id, int delta = 1) {
        if (this->display_name.empty()) {
            this->id = id;
            this->display_name = name;
        }
        else {
            if (this->id != id) throw 0;
        }
        counts(is_chinese, year_offset, country_id) += delta;
        counts(2, year_offset, country_id) += delta;
    }

    void merge (DomainCount const &other) {
        add(other.id, other.display_name, 0, 0, 0, 0);
        counts += other.counts;
    }
};

struct Survey {
    unordered_map<openalex_id_t, DomainCount> domains;
public:
    void add (Author const &author) {
        Migration mig = author.years.get_migration();
        int year_offset = mig.year_offset;
        if (year_offset < 0) return;
        //if (mig.country_id == OTHER_COUNTRY_ID) return;
        int is_chinese = Surnames::is_chinese(author.display_name) ? 1 : 0;
        for (auto const &[id, name] : author.domains) {
            domains[id].add(id, name, is_chinese, year_offset, mig.country_id);
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
       xt::xtensor<int, 4> counts;
       counts.resize({domains.size(), 3, TOTAL_YEARS, NUM_COUNTRIES});
       int i = 0;
       for (auto const &[id, domain]: domains) {
           jdomains.push_back({{"id", domain.id},
                               {"display_name", domain.display_name}});
           xt::view(counts, i, xt::all(), xt::all(), xt::all()) = domain.counts;
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

int main (int argc, char **argv) {
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
        filter_relevant("data/authors");
    }
    else if (strcmp(argv[1], "count") == 0) {
        if (argc < 3) {
            cerr << "Usage: " << argv[0] << " test <out_dir>" << endl;
        }
        else {
            count_migration("data/filtered_all", argv[2]);
            ofstream os("data/missing_country_stats.txt");
            vector<std::pair<string, int>> sorted;
            for (auto const &p: missing_country_stats) {
                sorted.emplace_back(p);
            }
            sort(sorted.begin(), sorted.end(), 
                 [](auto const &a, auto const &b) { return a.second > b.second; });
            for (auto const &p: sorted) {
                os << p.first << "\t" << p.second << endl;
            }
        }
    }
    return 0;
}

