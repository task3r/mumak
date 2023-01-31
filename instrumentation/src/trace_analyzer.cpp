/*
 *  Mumak Offline Trace Analyzer
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

#include "trace.hpp"

using std::endl;
using std::ifstream;
using std::istream_iterator;
using std::istringstream;
using std::map;
using std::ofstream;
using std::ostringstream;
using std::pair;
using std::string;
using std::vector;

ofstream bugs_out;

uint32_t total_bugs = 0;

void RecordPossibleBug(trace::BugType type, uint32_t id) {
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
    std::vector<trace::Bug> bugs;
    while (getline(trace_in, operation)) {
        // Split arguments of each operation
        istringstream iss(operation);
        vector<string> trace_line{istream_iterator<string>{iss},
                                  istream_iterator<string>{}};

        trace::Instruction type = static_cast<trace::Instruction>(
            strtoul(trace_line[0].c_str(), NULL, 10));
        uint64_t address = 0;
        uint32_t size = 0;
        uint32_t id;

        switch (type) {
            case trace::Instruction::STORE:
            case trace::Instruction::NON_TEMPORAL_STORE:
                address = strtoul(trace_line[1].c_str(), NULL, 10);
                size = strtoul(trace_line[2].c_str(), NULL, 10);
                id = strtoul(trace_line[3].c_str(), NULL, 10);
                trace::ta.ProcessStore(
                    address, size,
                    type == trace::Instruction::NON_TEMPORAL_STORE, id, bugs);
                break;
            case trace::Instruction::CLFLUSH:
            case trace::Instruction::CLFLUSHOPT:
            case trace::Instruction::CLWB:
                address = strtoul(trace_line[1].c_str(), NULL, 10);
                id = strtoul(trace_line[2].c_str(), NULL, 10);
                trace::ta.ProcessFlush(type, address, id, bugs);
                break;
            case trace::Instruction::FENCE:
                id = strtoul(trace_line[1].c_str(), NULL, 10);
                trace::ta.ProcessFence(id, bugs, false);
                break;
            case trace::Instruction::RMW:
                address = strtoul(trace_line[1].c_str(), NULL, 10);
                size = strtoul(trace_line[2].c_str(), NULL, 10);
                id = strtoul(trace_line[3].c_str(), NULL, 10);
                trace::ta.ProcessFence(id, bugs, true);
                if (size != 0)
                    trace::ta.ProcessStore(address, size, false, id, bugs);
                break;
            default:
                std::cerr << "Found an unknown operation: " << type << endl;
        }
        for (auto bug : bugs) {
            RecordPossibleBug(bug.type, bug.instruction_id);
        }
        bugs.clear();
    }

    trace::ta.CheckRemainder(bugs);
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
