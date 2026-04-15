// Copyright 2023 João Gonçalves

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#define BT_BUFFER_SIZE 100

struct Report {
    intptr_t mem_addr;
    intptr_t id1;
    intptr_t id2;
    pid_t pid1;
    pid_t pid2;
    int size_bt1;
    int size_bt2;
};

struct CompleteReport {
    Report r;
    uint64_t bt1[BT_BUFFER_SIZE];
    uint64_t bt2[BT_BUFFER_SIZE];
};

#define ADDR2LINE "addr2line -p -s -e "
#define ADDR2LINE_DEMANGLE "addr2line -f -p -s -C -e "

int addr2line(char* target, std::set<uint64_t>& bt_addrs,
              std::map<uint64_t, std::string>& bt_symbols) {
    std::string tmp = std::tmpnam(nullptr);
    std::ofstream tmp_out(tmp, std::ofstream::out);
    std::vector<uint64_t> bt_addrs_ordered;
    for (auto addr : bt_addrs) {
        tmp_out << "0x" << std::hex << addr << std::endl;
        bt_addrs_ordered.push_back(addr);
    }
    tmp_out.close();
    std::cout << tmp << std::endl;

    std::stringstream ss;
    ss << ADDR2LINE << target << " < " << tmp << std::endl;
    std::cout << ss.str();
    FILE* pipe = popen(ss.str().c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to open pipe." << std::endl;
        return 1;
    }

    size_t idx = 0;
    char buffer[10000];
    while (!feof(pipe)) {
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            bt_symbols[bt_addrs_ordered[idx++]] = std::string(buffer);
        }
    }
    pclose(pipe);
    return 0;
}

void print_bt_line(std::ofstream& out, uint64_t backtrace_addr,
                   std::map<uint64_t, std::string>& bt_symbols) {
    out << "\t\t0x" << std::hex << backtrace_addr;
    if (bt_symbols.find(backtrace_addr) != bt_symbols.end())
        out << ": " << bt_symbols[backtrace_addr];
    else
        out << std::endl;
}

void translate(char* target, char* in_file, char* out_file) {
    std::ifstream in(in_file, std::ifstream::binary);
    std::ofstream out(out_file, std::ofstream::out);
    std::vector<CompleteReport> reports;
    std::set<uint64_t> bt_addrs;
    std::map<uint64_t, std::string> bt_symbols;

    while (true) {
        CompleteReport bug;
        in.read(reinterpret_cast<char*>(&bug.r), sizeof(Report));
        if (in.eof()) break;
        in.read(reinterpret_cast<char*>(&bug.bt1),
                bug.r.size_bt1 * sizeof(uint64_t));
        in.read(reinterpret_cast<char*>(&bug.bt2),
                bug.r.size_bt2 * sizeof(uint64_t));
        reports.push_back(bug);
        for (int i = 0; i < bug.r.size_bt1; i++) {
            bt_addrs.insert(bug.bt1[i]);
        }
        for (int i = 0; i < bug.r.size_bt2; i++) {
            bt_addrs.insert(bug.bt2[i]);
        }
    }

    addr2line(target, bt_addrs, bt_symbols);

    int bug_idx = 0;
    for (CompleteReport bug : reports) {
        out << "BUG #" << std::dec << bug_idx++ << ":" << std::endl;
        out << "\t ADDR: 0x" << std::hex << bug.r.mem_addr << std::endl;
        out << "\t PIFRS: " << std::dec << bug.r.id1 << " " << bug.r.id2
            << std::endl;
        out << "\t THREADS: " << std::dec << bug.r.pid1 << ", " << bug.r.pid2
            << std::endl;
        out << "\t BT1:" << std::endl;
        for (int i = 0; i < bug.r.size_bt1; i++) {
            print_bt_line(out, bug.bt1[i], bt_symbols);
        }
        out << "\t BT2:" << std::endl;
        for (int i = 0; i < bug.r.size_bt2; i++) {
            print_bt_line(out, bug.bt2[i], bt_symbols);
        }
        out << std::endl;
    }
    in.close();
    out.close();
}

int main(int argc, char* argv[]) {
    translate(argv[1], argv[2], argv[3]);
    return 0;
}
