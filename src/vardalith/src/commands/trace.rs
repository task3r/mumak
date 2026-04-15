use crate::common::{IMG_FILE, RAMDISK_PATH, TRACE_FILE, VardalithConfig, VardalithResult};
use std::fs;
use std::path::Path;
use std::process::Command;

pub fn execute_trace(
    config: &VardalithConfig,
    workdir: Option<&String>,
) -> VardalithResult<String> {
    let config = config.base.as_ref().ok_or("Trace configuration missing")?;
    let binary = Path::new(&config.binary);

    if !binary.exists() {
        return Err(format!("Binary file does not exist: {}", config.binary));
    }

    let binary_basename = Path::new(&config.binary)
        .file_name()
        .and_then(|name| name.to_str())
        .ok_or("Invalid binary path")?;

    let work_dir = match workdir {
        Some(dir) => {
            if !Path::new(dir).exists() {
                fs::create_dir_all(dir)
                    .map_err(|e| format!("Failed to create working directory {}: {}", dir, e))?;
            }
            dir.clone()
        }
        None => {
            let timestamp = std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map_err(|e| format!("Failed to get timestamp: {}", e))?
                .as_secs();
            let dir_name = format!("vardalith_{}_{}", binary_basename, timestamp);

            fs::create_dir_all(&dir_name)
                .map_err(|e| format!("Failed to create working directory {}: {}", dir_name, e))?;

            dir_name
        }
    };

    fs::copy(
        &config.binary,
        Path::new(&work_dir).join(format!("{}.vanilla", binary_basename)),
    )
    .map_err(|e| format!("Failed to copy binary to working directory: {}", e))?;

    let mumak_root =
        std::env::var("MUMAK_ROOT").map_err(|_| "MUMAK_ROOT environment variable is not set.")?;
    let pin_root =
        std::env::var("PIN_ROOT").map_err(|_| "PIN_ROOT environment variable is not set.")?;

    // Trace target with PIN
    // setup
    let mut command = Command::new(format!("{}/pin", pin_root));
    command
        .current_dir(&work_dir)
        .arg("-t")
        .arg(format!(
            "{}/src/instrumentation/obj-intel84/pifr_trace.so",
            mumak_root
        ))
        .arg("--")
        .arg(format!("./{}.vanilla", binary_basename));

    if let Some(args) = &config.args {
        for arg in args {
            command.arg(arg);
        }
    }

    if let Some(env_vars) = &config.environment {
        for (key, value) in env_vars {
            command.env(key, value);
        }
    }

    // run
    let output = command
        .output()
        .map_err(|e| format!("Failed to execute binary {}: {}", config.binary, e))?;

    // clean up
    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        println!("Binary execution failed: {}", stderr);
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let trace_file_path = Path::new(&work_dir).join("trace.out");
    fs::write(&trace_file_path, stdout.as_bytes()).map_err(|_| "Failed to write trace file")?;

    // Probably should error if the file does not exist (instead of just printing)
    let trace_file = Path::new(RAMDISK_PATH).join(TRACE_FILE);
    if trace_file.exists() {
        fs::copy(&trace_file, Path::new(&work_dir).join(TRACE_FILE))
            .map_err(|_| "Copy trace failed")?;
    } else {
        println!("Warning: {} does not exist", trace_file.display());
    }
    let img_file = Path::new(RAMDISK_PATH).join(IMG_FILE);
    if img_file.exists() {
        fs::copy(&img_file, Path::new(&work_dir).join(IMG_FILE)).map_err(|_| "Copy img failed")?;
    } else {
        println!("Warning: {} does not exist", img_file.display());
    }

    Ok(work_dir)
}
