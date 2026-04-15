#include "cfg.hpp"

KNOB<bool> KnobBuildCFG(KNOB_MODE_WRITEONCE, "pintool", "cfg", "false",
                        "build cfg");

INT32 Usage() {
    std::cerr << "This tool collects/constructs CFGs." << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char *argv[]) {
    // Initialize pin
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    if (KnobBuildCFG.Value())
        BuildCFG();
    else
        CollectCFG();

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
