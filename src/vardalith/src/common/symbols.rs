use crate::common::VardalithResult;
use addr2line::Loader;
use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};
use std::fs::File;
use std::io::BufReader;
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SymbolInfo {
    pub function: Option<String>,
    pub filename: Option<String>,
    pub line: Option<u32>,
}

pub fn parse_pifr_addresses(json_path: &str) -> VardalithResult<HashMap<isize, Vec<String>>> {
    let file = File::open(json_path)
        .map_err(|e| format!("Failed to open JSON file {}: {}", json_path, e))?;
    let reader = BufReader::new(file);
    let addresses: HashMap<String, Vec<String>> = serde_json::from_reader(reader)
        .map_err(|e| format!("Failed to parse JSON file {}: {}", json_path, e))?;

    let mut pifr_addresses = HashMap::new();
    for (key, addrs) in addresses {
        if let Ok(pifr_id) = key.parse::<isize>() {
            pifr_addresses.insert(pifr_id, addrs);
        }
    }
    Ok(pifr_addresses)
}

pub fn resolve_symbol_for_address(loader: &Loader, addr: u64) -> VardalithResult<SymbolInfo> {
    let location = loader.find_location(addr).map_err(|e| e.to_string())?;
    let mut frames = loader.find_frames(addr).map_err(|e| e.to_string())?;
    let frame = frames.next().map_err(|e| e.to_string())?;

    let function = if let Some(frame) = frame {
        frame.function.as_ref().and_then(|f| {
            f.demangle().ok().map(|cow| {
                let demangled = cow.to_string();
                // Strip function arguments
                if let Some(paren_pos) = demangled.find('(') {
                    demangled[..paren_pos].to_string()
                } else {
                    demangled
                }
            })
        })
    } else {
        None
    };

    let (filename, line) = if let Some(loc) = location {
        let filename = loc.file.map(|f| f.to_string());
        let line = loc.line;
        (filename, line)
    } else {
        (None, None)
    };

    Ok(SymbolInfo {
        function,
        filename,
        line,
    })
}

pub fn resolve_symbols_for_addresses(
    binary_path: &str,
    addresses: &HashSet<u64>,
) -> VardalithResult<HashMap<u64, SymbolInfo>> {
    if !Path::new(binary_path).exists() {
        return Err(format!("Binary file does not exist: {}", binary_path));
    }

    let loader = Loader::new(binary_path).map_err(|e| e.to_string())?;
    let mut symbol_map = HashMap::new();

    for &addr in addresses {
        let symbol = resolve_symbol_for_address(&loader, addr).unwrap_or(SymbolInfo {
            function: None,
            filename: None,
            line: None,
        });
        symbol_map.insert(addr, symbol);
    }

    Ok(symbol_map)
}

pub fn parse_hex_address(addr_str: &str) -> Result<u64, std::num::ParseIntError> {
    u64::from_str_radix(addr_str.trim_start_matches("0x"), 16)
}
