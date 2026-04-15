use crate::common::VardalithResult;
use std::fs;
use std::path::Path;

/// Returns **the first file** found with the given extension in the specified directory
pub fn find_file_by_extension(dir: &str, extension: &str) -> VardalithResult<Option<String>> {
    let entries =
        fs::read_dir(dir).map_err(|e| format!("Failed to read directory {}: {}", dir, e))?;

    for entry in entries {
        let entry = entry.map_err(|e| format!("Failed to read directory entry: {}", e))?;
        let path = entry.path();

        if path.is_file() {
            if let Some(file_extension) = path.extension() {
                if file_extension == extension {
                    if let Some(path_str) = path.to_str() {
                        return Ok(Some(path_str.to_string()));
                    }
                }
            }
        }
    }

    Ok(None)
}

/// Returns **all files** found with the given extension in the specified directory
pub fn find_files_by_extension(dir: &str, extension: &str) -> VardalithResult<Vec<String>> {
    let entries =
        fs::read_dir(dir).map_err(|e| format!("Failed to read directory {}: {}", dir, e))?;

    let mut files = Vec::new();
    for entry in entries {
        let entry = entry.map_err(|e| format!("Failed to read directory entry: {}", e))?;
        let path = entry.path();

        if path.is_file() {
            if let Some(file_extension) = path.extension() {
                if file_extension == extension {
                    if let Some(path_str) = path.to_str() {
                        files.push(path_str.to_string());
                    }
                }
            }
        }
    }

    Ok(files)
}

/// Checks if a directory exists and is readable
pub fn validate_workdir(dir: &str) -> VardalithResult<()> {
    let path = Path::new(dir);

    if !path.exists() {
        return Err(format!("Directory does not exist: {}", dir));
    }

    if !path.is_dir() {
        return Err(format!("Path is not a directory: {}", dir));
    }

    // Try to read the directory to check if it's accessible
    fs::read_dir(dir).map_err(|e| format!("Cannot access directory {}: {}", dir, e))?;

    Ok(())
}
