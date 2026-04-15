// Copyright 2023 João Gonçalves

#include <cxxopts.hpp>

#include "./cfg.hpp"
#include "./disassembler.hpp"
#include "./statica.hpp"

#define PATH_ACQUIRE "./acquire_calls.txt"
#define PATH_RELEASE "./release_calls.txt"

int main(int argc, char* argv[]) {
    cxxopts::Options options("palantir", "x86_64 analysis tool suite");

    options.add_options()("elf_file", "Path to elf target file",
                          cxxopts::value<std::string>());
    options.add_options()("p,pifrs", "Define PIFRs");
    options.add_options()("pifrs-with-ends",
                          "Define PIFRs with statically defined ends");
    options.add_options()("minimal", "Define minimal vulnerability windows");
    options.add_options()("i,img_id", "Enable debugging",
                          cxxopts::value<int>()->default_value("1"));
    options.add_options()(
        "t,trace_path", "Path to execution trace",
        cxxopts::value<std::string>()->default_value(PATH_ACQUIRE));
    options.add_options()(
        "a,acq_path", "Path to acquire calls",
        cxxopts::value<std::string>()->default_value(PATH_ACQUIRE));
    options.add_options()(
        "r,rel_path", "Path to release calls",
        cxxopts::value<std::string>()->default_value(PATH_RELEASE));
    options.add_options()("dot", "Export CFGs in dot representation");
    options.add_options()("dom", "Export dominators, postdominators and loops");
    options.add_options()("cg", "Export call graph in dot representation");
    options.add_options()(
        "persist",
        "Only consider persist calls (typically only for libraries)");
    options.add_options()("h,help", "Print usage");

    options.positional_help("<elf_file>");
    options.parse_positional("elf_file");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    if (!result.count("elf_file")) {
        std::cout << options.help() << std::endl;
        return 1;
    }

    const char* elf_name = result["elf_file"].as<std::string>().c_str();
    palantir::Module* module = disassemble_module(elf_name);
    if (!module) return 1;

    if (result.count("pifrs") || result.count("pifrs-with-ends")) {
        const char* acq_path = result["acq_path"].as<std::string>().c_str();
        const char* rel_path = result["rel_path"].as<std::string>().c_str();
        const char* trace_path = result["trace_path"].as<std::string>().c_str();
        int img_id = result["img_id"].as<int>();

        palantir::Context ctx =
            palantir::CreateContext(img_id, trace_path, acq_path, rel_path);
        palantir::FindPIFRs(module, ctx, result.count("persist") != 0,
                            result.count("pifrs-with-ends") != 0);

    } else if (result.count("minimal")) {
        const char* trace_path = result["trace_path"].as<std::string>().c_str();
        int img_id = result["img_id"].as<int>();
        palantir::Context ctx = palantir::CreateContext(img_id, trace_path);
        palantir::FindMinimalWindows(module, ctx);
    }

    // palantir::IdentifyFlowControlRW(module);
    // palantir::FindRetBBs(module);
    // palantir::FindCalls(module);

    if (result.count("dot")) module->dot(false);
    if (result.count("dom")) module->dot_dominators();
    if (result.count("cg")) {
        module->BuildCallGraph();
        std::ofstream os;
        std::stringstream ss;
        ss << module->get_name() << "_cg.dot";
        os.open(ss.str().c_str());
        module->get_call_graph()->dot(os);
        os.close();
    }

    return 0;
}
