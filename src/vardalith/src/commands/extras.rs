use crate::common::{
    VardalithResult,
    file_utils::{find_file_by_extension, validate_workdir},
    symbols::{parse_hex_address, parse_pifr_addresses, resolve_symbols_for_addresses},
};
use std::collections::HashSet;

pub fn execute_pifr_mapping(workdir: &str) {
    match execute_pifr_mapping_impl(workdir) {
        Ok(()) => {}
        Err(e) => {
            eprintln!("Error: {}", e);
            std::process::exit(1);
        }
    }
}

fn execute_pifr_mapping_impl(workdir: &str) -> VardalithResult<()> {
    validate_workdir(workdir)?;

    let json_path = find_file_by_extension(workdir, "json")?
        .ok_or_else(|| format!("No JSON file found in directory: {}", workdir))?;

    let pifr_addresses = parse_pifr_addresses(&json_path)?;

    if pifr_addresses.is_empty() {
        println!("No PIFR mappings found in JSON file.");
        return Ok(());
    }

    let binary_path = find_file_by_extension(workdir, "patched")?;

    let symbol_info = if let Some(binary_path) = binary_path {
        let mut unique_addresses = HashSet::new();
        for addrs in pifr_addresses.values() {
            for addr_str in addrs {
                if let Ok(addr) = parse_hex_address(addr_str) {
                    unique_addresses.insert(addr);
                }
            }
        }

        if !unique_addresses.is_empty() {
            match resolve_symbols_for_addresses(&binary_path, &unique_addresses) {
                Ok(symbols) => Some(symbols),
                Err(e) => {
                    eprintln!("Warning: Failed to resolve symbols: {}", e);
                    None
                }
            }
        } else {
            None
        }
    } else {
        eprintln!("Warning: No binary file (.patched) found, symbols will not be resolved");
        None
    };

    output_pifr_mapping_table(&pifr_addresses, symbol_info.as_ref());

    Ok(())
}

fn output_pifr_mapping_table(
    pifr_addresses: &std::collections::HashMap<isize, Vec<String>>,
    symbol_map: Option<&std::collections::HashMap<u64, crate::common::symbols::SymbolInfo>>,
) {
    println!("=== PIFR Mapping Table ===");
    println!();
    println!(
        "{:<10} {:<18} {:<40} {:<30} {}",
        "PIFR ID", "Address", "Function", "File", "Line"
    );
    println!("{}", "-".repeat(120));

    let mut pifr_ids: Vec<_> = pifr_addresses.keys().copied().collect();
    pifr_ids.sort();

    for pifr_id in pifr_ids {
        let addresses = &pifr_addresses[&pifr_id];

        for (i, addr_str) in addresses.iter().enumerate() {
            let pifr_display = if i == 0 {
                pifr_id.to_string()
            } else {
                "".to_string()
            };

            if let Some(symbol_map) = symbol_map {
                if let Ok(addr) = parse_hex_address(addr_str) {
                    if let Some(symbol) = symbol_map.get(&addr) {
                        let function = symbol.function.as_deref().unwrap_or("<unknown>");
                        let filename = symbol.filename.as_deref().unwrap_or("<unknown>");
                        let line = symbol
                            .line
                            .map(|l| l.to_string())
                            .unwrap_or_else(|| "<unknown>".to_string());

                        println!(
                            "{:<10} {:<18} {:<40} {:<30} {}",
                            pifr_display, addr_str, function, filename, line
                        );
                    } else {
                        println!(
                            "{:<10} {:<18} {:<40} {:<30} {}",
                            pifr_display, addr_str, "<unresolved>", "<unresolved>", "<unresolved>"
                        );
                    }
                } else {
                    println!(
                        "{:<10} {:<18} {:<40} {:<30} {}",
                        pifr_display,
                        addr_str,
                        "<invalid_addr>",
                        "<invalid_addr>",
                        "<invalid_addr>"
                    );
                }
            } else {
                println!(
                    "{:<10} {:<18} {:<40} {:<30} {}",
                    pifr_display, addr_str, "<no_symbols>", "<no_symbols>", "<no_symbols>"
                );
            }
        }

        if addresses.len() > 1 {
            println!();
        }
    }

    println!();
    println!("Total PIFR IDs: {}", pifr_addresses.len());
    println!(
        "Total instruction addresses: {}",
        pifr_addresses.values().map(|v| v.len()).sum::<usize>()
    );
}
