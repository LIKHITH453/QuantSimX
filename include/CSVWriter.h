#pragma once

#include <string>
#include <fstream>

class CSVWriter {
public:
    explicit CSVWriter(const std::string& path);
    ~CSVWriter();
    void write_row(const std::string& row);
    void flush();
private:
    std::ofstream fout_;
};
