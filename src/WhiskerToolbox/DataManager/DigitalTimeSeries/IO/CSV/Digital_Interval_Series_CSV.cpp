
#include "Digital_Interval_Series_CSV.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

std::vector<Interval> load_digital_series_from_csv(
        std::string const & filename,
        char delimiter) {
    std::string csv_line;

    std::fstream myfile;
    myfile.open(filename, std::fstream::in);

    int64_t start, end;
    auto output = std::vector<Interval>();
    while (getline(myfile, csv_line)) {
        std::stringstream ss(csv_line);
        ss >> start >> delimiter >> end;
        output.emplace_back(Interval{start, end});
    }

    return output;
}

void save_intervals(
        std::vector<Interval> const & intervals,
        std::string const & block_output) {
    std::fstream fout;
    fout.open(block_output, std::fstream::out);

    for (auto & interval: intervals) {
        fout << std::round(interval.start) << "," << std::round(interval.end) << "\n";
    }

    fout.close();
}

