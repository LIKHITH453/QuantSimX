#include "CSVWriter.h"
#include <iostream>

CSVWriter::CSVWriter(const std::string& path) {
    fout_.open(path, std::ios::out | std::ios::trunc);
    if (!fout_.is_open()) {
        std::cerr << "Failed to open CSV file: " << path << "\n";
    }
}

CSVWriter::~CSVWriter() {
    flush();
    if (fout_.is_open()) fout_.close();
}

void CSVWriter::write_row(const std::string& row) {
    if (fout_.is_open()) {
        fout_ << row << '\n';
    }
}

void CSVWriter::flush() {
    if (fout_.is_open()) fout_.flush();
}
