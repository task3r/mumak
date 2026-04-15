mod commands;
mod common;

use clap::{Arg, Command};
use commands::*;
use common::VardalithConfig;

fn main() {
    let matches = Command::new("vardalith")
        .about("A tool for tracing, patching, running, and reporting on persistency race bugs")
        .version("0.1.0")
        .subcommand_required(true)
        .arg_required_else_help(true)
        .subcommand(
            Command::new("trace")
                .about("Run a binary with arguments and output to working directory")
                .arg(
                    Arg::new("config")
                        .help("Path to the configuration file")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("workdir")
                        .help("Path to the working directory")
                        .short('w')
                        .long("working-dir")
                        .value_name("DIR"),
                )
        )
        .subcommand(
            Command::new("patch")
                .about("Apply patches to files in a working directory")
                .arg(
                    Arg::new("workdir")
                        .help("Path to the working directory")
                        .required(true)
                        .index(1),
                ),
        )
        .subcommand(
            Command::new("run")
                .about("Run experiments with configuration file")
                .arg(
                    Arg::new("workdir")
                        .help("Path to the working directory")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("config")
                        .help("Path to the configuration file")
                        .required(true)
                        .index(2),
                )
                .arg(
                    Arg::new("results_dir")
                        .help("Path to the results directory (defaults to <workdir>/results)")
                        .required(false)
                        .index(3),
                ),
        )
        .subcommand(
            Command::new("report")
                .about("Generate reports from bug report files")
                .arg(
                    Arg::new("workdir")
                        .help("Path to the working directory containing .patched binary and JSON file")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("reports_dir")
                        .help("Path to the directory containing *.bin report files")
                        .required(true)
                        .index(2),
                )
                .arg(
                    Arg::new("json")
                        .help("Output results in JSON format")
                        .short('j')
                        .long("json")
                        .action(clap::ArgAction::SetTrue),
                )
                .arg(
                    Arg::new("output")
                        .help("Output file path (stdout if not specified)")
                        .short('o')
                        .long("output")
                        .value_name("FILE"),
                ),
        )
        .subcommand(
            Command::new("extras")
                .about("Extra utilities")
                .arg(
                    Arg::new("workdir")
                        .help("Path to the working directory containing .patched binary and JSON file")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("pifrs")
                        .help("Map PIFR addresses to symbols")
                        .short('p')
                        .long("pifrs")
                        .action(clap::ArgAction::SetTrue),
                )
        )
        .subcommand(
            Command::new("results")
                .about("Process execution result files in a working directory")
                .arg(
                    Arg::new("workdir")
                        .help("Path to the working directory containing result files")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("config")
                        .help("Path to the configuration file")
                        .required(true)
                        .index(2),
                )
                .arg(
                    Arg::new("results_dir")
                        .help("Path to the results directory (defaults to <workdir>/results)")
                        .required(false)
                        .index(3),
                ),
        )
        .get_matches();

    let result = match matches.subcommand() {
        Some(("trace", sub_matches)) => {
            let config_path = sub_matches.get_one::<String>("config").unwrap();
            let workdir = sub_matches.get_one::<String>("workdir");

            match VardalithConfig::from_toml_file(config_path) {
                Ok(config) => match execute_trace(&config, workdir) {
                    Ok(work_dir) => {
                        println!("Working directory: {}", work_dir);
                        Ok(())
                    }
                    Err(e) => Err(e),
                },
                Err(e) => Err(format!("Failed to parse config file: {}", e)),
            }
        }
        Some(("patch", sub_matches)) => {
            let workdir = sub_matches.get_one::<String>("workdir").unwrap();
            execute_patch(workdir)
        }
        Some(("run", sub_matches)) => {
            let workdir = sub_matches.get_one::<String>("workdir").unwrap();
            let config_path = sub_matches.get_one::<String>("config").unwrap();
            let results_dir = sub_matches.get_one::<String>("results_dir");

            match VardalithConfig::from_toml_file(config_path) {
                Ok(config) => execute_run(workdir, results_dir.map(String::as_str), &config),
                Err(e) => Err(format!("Failed to parse config file: {}", e)),
            }
        }
        Some(("report", sub_matches)) => {
            let workdir = sub_matches.get_one::<String>("workdir").unwrap();
            let reports_dir = sub_matches.get_one::<String>("reports_dir").unwrap();
            let json_output = sub_matches.get_flag("json");
            let output_file = sub_matches.get_one::<String>("output").cloned();

            execute_report(workdir, reports_dir, json_output, output_file)
        }
        Some(("extras", sub_matches)) => {
            let workdir = sub_matches.get_one::<String>("workdir").unwrap();
            let pifr_mapping = sub_matches.get_flag("pifrs");

            if pifr_mapping {
                execute_pifr_mapping(workdir);
            }

            Ok(())
        }
        Some(("results", sub_matches)) => {
            let workdir = sub_matches.get_one::<String>("workdir").unwrap();
            let config_path = sub_matches.get_one::<String>("config").unwrap();
            let results_dir = sub_matches.get_one::<String>("results_dir");

            match VardalithConfig::from_toml_file(config_path) {
                Ok(config) => execute_results(workdir, results_dir.map(String::as_str), &config),
                Err(e) => Err(format!("Failed to parse config file: {}", e)),
            }
        }
        _ => unreachable!(),
    };

    if let Err(e) = result {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}
