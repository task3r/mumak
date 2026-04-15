// Copyright 2023 João Gonçalves

#include <cstdint>
#include <fstream>
#include <vector>

#include "./tracing.hpp"

#define RATIO 1000

void translate(char* in_file, char* out_file) {
    std::ifstream in(in_file, std::ifstream::binary);
    std::ofstream out(out_file, std::ofstream::out);

    while (true) {
        tracing::rwflow::TraceLine l = {0, 0, 0, 0, 0};
        in.read(reinterpret_cast<char*>(&l),
                sizeof(tracing::rwflow::TraceLine));
        if (in.eof()) break;
        switch (l.type) {
            case tracing::rwflow::WRITE:
                out << "wr ";
                break;
            case tracing::rwflow::PM_WRITE:
                out << "pwr";
                break;
            case tracing::rwflow::READ:
                out << "rd ";
                break;
            case tracing::rwflow::PM_READ:
                out << "prd";
                break;
            case tracing::rwflow::FLUSH:
                out << "fl ";
                break;
            case tracing::rwflow::BRANCH:
                out << "br ";
                break;
            default:
                std::cerr << "Unrecognized trace line" << std::endl;
                continue;
        }
        out << " " << l.img << " " << std::hex << l.ip << " " << std::hex
            << l.target << " " << std::dec << l.size << std::endl;
    }
    in.close();
    out.close();
}

int main(int argc, char* argv[]) {
    translate(argv[1], argv[2]);
    return 0;
}
