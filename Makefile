CXXFLAGS += -O3 -g -I3rd/json/single_include -I3rd/bxzstr/include -I3rd/xtensor/include -I3rd/xtl/include -fopenmp -std=c++20
LDFLAGS += -fopenmp
LDLIBS += -lz

all:	run_all_countries match_emails


match_emails:	match_emails.cpp match.cpp
