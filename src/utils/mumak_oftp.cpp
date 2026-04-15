/*
 *  Mumak Offline Trace Processer
 */

#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "./tracing.hpp"

using std::endl;
using std::ifstream;
using std::istream_iterator;
using std::istringstream;
using std::ofstream;
using std::ostringstream;
using std::string;
using std::vector;

using tracing::pm::Bug;
using tracing::pm::BugType;
using tracing::pm::Instruction;
using tracing::pm::proc;

ofstream bugs_out;

uint32_t total_bugs = 0;

void RecordPossibleBug(BugType type, uint32_t id) {
    bugs_out << type << " " << id << "\n";
    total_bugs++;
}

int main(int argc, char *argv[]) {
    // Obtain file path to all necessary files to report the bugs and warnings
    string out_path = argv[1];

    // Define all necessary files to report the detected bugs and warnings
    string bugs_out_file = out_path + "/bug-ids.txt";
    string trace_file = out_path + "/pm-trace.txt";

    // Initialize logging files
    bugs_out.open(bugs_out_file.c_str(), ofstream::out | ofstream::trunc);

    // Get vector of all operations
    ifstream trace_in(trace_file.c_str());
    string operation;
    std::vector<Bug> bugs;
    while (getline(trace_in, operation)) {
        // Split arguments of each operation
        istringstream iss(operation);
        vector<string> trace_line{istream_iterator<string>{iss},
                                  istream_iterator<string>{}};

        Instruction type =
            static_cast<Instruction>(strtoul(trace_line[0].c_str(), NULL, 10));
        uint64_t address = 0;
        uint32_t size = 0;
        uint32_t id;

        switch (type) {
            case Instruction::STORE:
            case Instruction::NON_TEMPORAL_STORE:
                address = strtoul(trace_line[1].c_str(), NULL, 10);
                size = strtoul(trace_line[2].c_str(), NULL, 10);
                id = strtoul(trace_line[3].c_str(), NULL, 10);
                proc.ProcessStore(address, size,
                                  type == Instruction::NON_TEMPORAL_STORE, id,
                                  bugs);
                break;
            case Instruction::CLFLUSH:
            case Instruction::CLFLUSHOPT:
            case Instruction::CLWB:
                address = strtoul(trace_line[1].c_str(), NULL, 10);
                id = strtoul(trace_line[2].c_str(), NULL, 10);
                proc.ProcessFlush(type, address, id, bugs);
                break;
            case Instruction::FENCE:
                id = strtoul(trace_line[1].c_str(), NULL, 10);
                proc.ProcessFence(id, bugs, false);
                break;
            case Instruction::RMW:
                address = strtoul(trace_line[1].c_str(), NULL, 10);
                size = strtoul(trace_line[2].c_str(), NULL, 10);
                id = strtoul(trace_line[3].c_str(), NULL, 10);
                proc.ProcessFence(id, bugs, true);
                if (size != 0)
                    proc.ProcessStore(address, size, false, id, bugs);
                break;
            default:
                std::cerr << "Found an unknown operation: " << type << endl;
        }
        for (auto bug : bugs) {
            RecordPossibleBug(bug.type, bug.instruction_id);
        }
        bugs.clear();
    }

    proc.CheckRemainder(bugs);
    for (auto bug : bugs) {
        RecordPossibleBug(bug.type, bug.instruction_id);
    }

    trace_in.close();
    bugs_out.close();

    std::cerr << "Total amount of bugs reported: " << total_bugs << endl;
    // std::cerr << "Total amount of warnings reported: " <<
    // totalWarningsReported
    //   << endl;

    return 0;
}
