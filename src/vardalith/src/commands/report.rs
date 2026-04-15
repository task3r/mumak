use crate::common::{
    VardalithResult,
    file_utils::{find_file_by_extension, find_files_by_extension},
    symbols::{SymbolInfo, parse_hex_address, parse_pifr_addresses, resolve_symbols_for_addresses},
};

use serde::{Deserialize, Serialize};

use std::collections::{HashMap, HashSet};
use std::fs::File;
use std::io::{BufReader, Read, Write};
use std::mem;

#[derive(Debug, Serialize, Deserialize)]
pub struct PersistencyRaceReport {
    pub mem_addr: isize,
    pub pifr_id: isize,
    pub other_pifr_id: isize,
    pub tid: i32,
    pub other_tid: i32,
    #[serde(skip_serializing, skip_deserializing)]
    pub backtrace: Vec<u64>,
    #[serde(skip_serializing, skip_deserializing)]
    pub other_backtrace: Vec<u64>,
    pub backtrace_symbols: Vec<SymbolInfo>,
    pub other_backtrace_symbols: Vec<SymbolInfo>,
}

fn read_value<T: Copy>(reader: &mut BufReader<File>) -> VardalithResult<T> {
    let mut buffer = vec![0u8; mem::size_of::<T>()];
    reader.read_exact(&mut buffer).map_err(|e| e.to_string())?;

    let value = unsafe { std::ptr::read(buffer.as_ptr() as *const T) };

    Ok(value)
}

pub fn parse_bug_reports(filename: &str) -> VardalithResult<Vec<PersistencyRaceReport>> {
    let file = File::open(filename).map_err(|e| e.to_string())?;
    let mut reader = BufReader::new(file);
    let mut reports = Vec::new();

    while let Ok(mem_addr) = read_value::<isize>(&mut reader) {
        let pifr_id = read_value::<isize>(&mut reader)?;
        let other_pifr_id = read_value::<isize>(&mut reader)?;
        let tid = read_value::<i32>(&mut reader)?;
        let other_tid = read_value::<i32>(&mut reader)?;
        let backtrace_size = read_value::<i32>(&mut reader)?;
        let other_backtrace_size = read_value::<i32>(&mut reader)?;

        let mut backtrace = Vec::with_capacity(backtrace_size as usize);
        for _ in 0..backtrace_size {
            let addr = read_value::<u64>(&mut reader)?;
            backtrace.push(addr);
        }

        let mut other_backtrace = Vec::with_capacity(other_backtrace_size as usize);
        for _ in 0..other_backtrace_size {
            let addr = read_value::<u64>(&mut reader)?;
            other_backtrace.push(addr);
        }

        let report = PersistencyRaceReport {
            mem_addr,
            pifr_id,
            other_pifr_id,
            tid,
            other_tid,
            backtrace,
            other_backtrace,
            backtrace_symbols: Vec::new(),
            other_backtrace_symbols: Vec::new(),
        };

        reports.push(report);
    }

    Ok(reports)
}

pub fn resolve_symbols(
    reports: &mut [PersistencyRaceReport],
    binary_path: &str,
    json_addresses: Option<&HashMap<isize, Vec<String>>>,
) -> VardalithResult<()> {
    let mut unique_addresses = HashSet::new();
    for report in reports.iter() {
        for &addr in &report.backtrace {
            unique_addresses.insert(addr);
        }
        for &addr in &report.other_backtrace {
            unique_addresses.insert(addr);
        }
    }

    if let Some(json_addrs) = json_addresses {
        for addrs in json_addrs.values() {
            for addr_str in addrs {
                if let Ok(addr) = parse_hex_address(addr_str) {
                    unique_addresses.insert(addr);
                }
            }
        }
    }

    // Resolve symbols for all unique addresses
    let symbol_map = resolve_symbols_for_addresses(binary_path, &unique_addresses)?;

    // Populate backtrace symbols for each report
    for report in reports.iter_mut() {
        let mut backtrace_symbols = Vec::new();
        let mut other_backtrace_symbols = Vec::new();

        // Prepend PIFR: this is slightly broken, multiple addrs for the same pifr id, not sure how to deal with that
        if let Some(json_addrs) = json_addresses {
            if let Some(pifr_addrs) = json_addrs.get(&report.pifr_id) {
                for addr_str in pifr_addrs {
                    if let Ok(addr) = parse_hex_address(addr_str) {
                        if let Some(symbol) = symbol_map.get(&addr) {
                            backtrace_symbols.push(symbol.clone());
                        }
                    }
                }
            }
        }

        for &addr in &report.backtrace {
            backtrace_symbols.push(symbol_map.get(&addr).cloned().unwrap_or(SymbolInfo {
                function: None,
                filename: None,
                line: None,
            }));
        }

        // don't like how this is duplicated, but whatever
        if let Some(json_addrs) = json_addresses {
            if let Some(other_pifr_addrs) = json_addrs.get(&report.other_pifr_id) {
                for addr_str in other_pifr_addrs {
                    if let Ok(addr) = parse_hex_address(addr_str) {
                        if let Some(symbol) = symbol_map.get(&addr) {
                            other_backtrace_symbols.push(symbol.clone());
                        }
                    }
                }
            }
        }

        for &addr in &report.other_backtrace {
            other_backtrace_symbols.push(symbol_map.get(&addr).cloned().unwrap_or(SymbolInfo {
                function: None,
                filename: None,
                line: None,
            }));
        }

        report.backtrace_symbols = backtrace_symbols;
        report.other_backtrace_symbols = other_backtrace_symbols;
    }

    Ok(())
}

pub fn execute_report(
    workdir: &str,
    reports_dir: &str,
    json_output: bool,
    output_file: Option<String>,
) -> VardalithResult<()> {
    let reports_w_backtrace = find_files_by_extension(reports_dir, "bin")?;
    if reports_w_backtrace.is_empty() {
        return Err(format!(
            "No *.bin files found in directory: {}",
            reports_dir
        ));
    }

    let mut all_reports = Vec::new();
    for bin_file in &reports_w_backtrace {
        match parse_bug_reports(bin_file) {
            Ok(mut reports) => all_reports.append(&mut reports),
            Err(e) => eprintln!("Warning: Failed to parse {}: {}", bin_file, e),
        }
    }

    let unique_reports = filter_unique_pifr_pairs(all_reports);

    let mut final_reports = unique_reports;
    if let Ok(Some(binary_path)) = find_file_by_extension(workdir, "patched") {
        let json_addresses = match find_file_by_extension(workdir, "json") {
            Ok(Some(json_path)) => match parse_pifr_addresses(&json_path) {
                Ok(addrs) => Some(addrs),
                Err(_) => None,
            },
            _ => None,
        };

        resolve_symbols(&mut final_reports, &binary_path, json_addresses.as_ref())?;
    }

    if json_output {
        output_json_report(&final_reports, output_file)?;
    } else {
        output_plain_text_report(&final_reports, output_file)?;
    }

    Ok(())
}

fn filter_unique_pifr_pairs(reports: Vec<PersistencyRaceReport>) -> Vec<PersistencyRaceReport> {
    let mut seen_pairs = HashSet::new();
    let mut unique_reports = Vec::new();

    for report in reports {
        let pair = if report.pifr_id <= report.other_pifr_id {
            (report.pifr_id, report.other_pifr_id)
        } else {
            (report.other_pifr_id, report.pifr_id)
        };

        if !seen_pairs.contains(&pair) {
            seen_pairs.insert(pair);
            unique_reports.push(report);
        }
    }

    unique_reports
}

fn output_json_report(
    reports: &[PersistencyRaceReport],
    output_file: Option<String>,
) -> VardalithResult<()> {
    let json_output = serde_json::to_string_pretty(reports)
        .map_err(|e| format!("Failed to serialize to JSON: {}", e))?;

    if let Some(output_path) = output_file {
        let mut file = File::create(&output_path)
            .map_err(|e| format!("Failed to create output file {}: {}", output_path, e))?;
        file.write_all(json_output.as_bytes())
            .map_err(|e| format!("Failed to write to output file: {}", e))?;
        println!("JSON report written to: {}", output_path);
    } else {
        println!("{}", json_output);
    }

    Ok(())
}

fn output_plain_text_report(
    reports: &[PersistencyRaceReport],
    output_file: Option<String>,
) -> VardalithResult<()> {
    let text_output = generate_plain_text_report(reports);

    if let Some(output_path) = output_file {
        let mut file = File::create(&output_path)
            .map_err(|e| format!("Failed to create output file {}: {}", output_path, e))?;
        file.write_all(text_output.as_bytes())
            .map_err(|e| format!("Failed to write to output file: {}", e))?;
        println!("Text report written to: {}", output_path);
    } else {
        print!("{}", text_output);
    }

    Ok(())
}

fn generate_plain_text_report(reports: &[PersistencyRaceReport]) -> String {
    let mut output = String::new();

    output.push_str("=== Vardalith Report ===\n");
    output.push_str(&format!(
        "Unique PIFR ID pairs found: {}\n\n",
        reports.len()
    ));

    for (i, report) in reports.iter().enumerate() {
        output.push_str(&format!("--- Bug Report #{} ---\n", i + 1));
        output.push_str(&format!("Memory Address: 0x{:x}\n", report.mem_addr));
        output.push_str(&format!("PIFR ID: {}\n", report.pifr_id));
        output.push_str(&format!("Other PIFR ID: {}\n", report.other_pifr_id));
        output.push_str(&format!("Thread ID: {}\n", report.tid));
        output.push_str(&format!("Other Thread ID: {}\n", report.other_tid));

        output.push_str(&format!("Backtrace ({} frames):\n", report.backtrace.len()));
        for (j, (&addr, symbol)) in report
            .backtrace
            .iter()
            .zip(&report.backtrace_symbols)
            .enumerate()
        {
            output.push_str(&format!("  Frame {}: 0x{:x}", j, addr));
            if let Some(ref func) = symbol.function {
                output.push_str(&format!(" in {}", func));
            }
            if let (Some(file), Some(line)) = (&symbol.filename, symbol.line) {
                output.push_str(&format!(" at {}:{}", file, line));
            }
            output.push('\n');
        }

        output.push_str(&format!(
            "Other Backtrace ({} frames):\n",
            report.other_backtrace.len()
        ));
        for (j, (&addr, symbol)) in report
            .other_backtrace
            .iter()
            .zip(&report.other_backtrace_symbols)
            .enumerate()
        {
            output.push_str(&format!("  Frame {}: 0x{:x}", j, addr));
            if let Some(ref func) = symbol.function {
                output.push_str(&format!(" in {}", func));
            }
            if let (Some(file), Some(line)) = (&symbol.filename, symbol.line) {
                output.push_str(&format!(" at {}:{}", file, line));
            }
            output.push('\n');
        }
        output.push('\n');
    }

    output
}
