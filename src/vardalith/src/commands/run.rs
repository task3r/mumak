use crate::common::{ExperimentConfig, RAMDISK_PATH, ReportType, VardalithConfig, VardalithResult};
use std::collections::HashMap;
use std::fs::{self, File};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Output, Stdio};
use std::thread;
use std::time::{Duration, Instant};

const BUGS_TEXT_FILE: &str = "bugs.txt";
const BUGS_BIN_FILE: &str = "bugs.bin";

fn report_type_to_value(report_type: &ReportType) -> i32 {
    match report_type {
        ReportType::Off => 0,
        ReportType::Minimal => 1,
        ReportType::Full => 2,
    }
}

fn bool_to_env(value: bool) -> String {
    if value {
        "1".to_string()
    } else {
        "0".to_string()
    }
}

fn format_duration(duration: Duration) -> String {
    let total_secs = duration.as_secs_f64();
    let minutes = (total_secs / 60.0).floor();
    let seconds = total_secs - (minutes * 60.0);

    format!(
        "real\t{}m{:.3}s\nuser\t0m0.000s\nsys\t0m0.000s\n",
        minutes as u64, seconds
    )
}

fn default_experiment() -> ExperimentConfig {
    ExperimentConfig {
        sampling: None,
        report_type: None,
        minimal_regions: None,
        delay: None,
        decay: None,
        threads: None,
        workload: None,
        profiling: None,
        tracing: None,
        timeout_seconds: None,
    }
}

fn resolve_binary_path(binary: &str, working_dir: &str) -> PathBuf {
    let binary_path = Path::new(binary);
    if binary_path.is_relative() {
        let candidate = Path::new(working_dir).join(binary_path);
        if candidate.exists() {
            return candidate;
        }
    }

    binary_path.to_path_buf()
}

fn build_ld_preload(env_vars: &HashMap<String, String>) -> VardalithResult<String> {
    let libpifrrt_root = std::env::var("LIBPIFRRT_ROOT")
        .map_err(|_| "LIBPIFRRT_ROOT environment variable is not set")?;
    let mmap_wrapper = Path::new(&libpifrrt_root).join("mmap_wrapper.so");
    let mut entries = vec![mmap_wrapper.to_string_lossy().into_owned()];

    if let Some(ld_preload) = env_vars.get("LD_PRELOAD") {
        entries.push(ld_preload.clone());
    }

    Ok(entries.join(":"))
}

fn move_file(src: &Path, dest: &Path) -> VardalithResult<()> {
    if let Some(parent) = dest.parent() {
        fs::create_dir_all(parent)
            .map_err(|e| format!("Failed to create directory {}: {}", parent.display(), e))?;
    }

    match fs::rename(src, dest) {
        Ok(_) => Ok(()),
        Err(_) => {
            fs::copy(src, dest).map_err(|e| format!("Failed to copy {}: {}", src.display(), e))?;
            fs::remove_file(src)
                .map_err(|e| format!("Failed to remove {}: {}", src.display(), e))?;
            Ok(())
        }
    }
}

fn clear_dir_contents(dir: &Path) -> VardalithResult<()> {
    if dir.exists() {
        fs::remove_dir_all(dir)
            .map_err(|e| format!("Failed to remove directory {}: {}", dir.display(), e))?;
    }

    fs::create_dir_all(dir)
        .map_err(|e| format!("Failed to create directory {}: {}", dir.display(), e))?;

    Ok(())
}

fn run_command_with_timeout(
    mut command: Command,
    timeout_seconds: Option<u64>,
) -> VardalithResult<(Output, Duration, bool)> {
    let start = Instant::now();

    if let Some(timeout) = timeout_seconds {
        let mut child = command
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .map_err(|e| format!("Failed to spawn command: {}", e))?;
        let timeout = Duration::from_secs(timeout);
        let mut timed_out = false;

        loop {
            if let Some(_status) = child
                .try_wait()
                .map_err(|e| format!("Failed to check command status: {}", e))?
            {
                break;
            }

            if start.elapsed() >= timeout {
                timed_out = true;
                let _ = child.kill();
                break;
            }

            thread::sleep(Duration::from_millis(100));
        }

        let output = child
            .wait_with_output()
            .map_err(|e| format!("Failed to capture command output: {}", e))?;

        Ok((output, start.elapsed(), timed_out))
    } else {
        let output = command
            .output()
            .map_err(|e| format!("Failed to execute command: {}", e))?;
        Ok((output, start.elapsed(), false))
    }
}

fn option_to_string<T: ToString>(value: Option<T>, fallback: &str) -> String {
    value
        .map(|v| v.to_string())
        .unwrap_or_else(|| fallback.to_string())
}

pub fn execute_run(
    working_dir: &str,
    results_dir: Option<&str>,
    config: &VardalithConfig,
) -> VardalithResult<()> {
    let base = config
        .base
        .as_ref()
        .ok_or("Run configuration missing base section")?;
    let run = config
        .run
        .as_ref()
        .ok_or("Run configuration missing run section")?;

    fs::create_dir_all(working_dir)
        .map_err(|e| format!("Failed to create working directory {}: {}", working_dir, e))?;

    let binary_path = resolve_binary_path(&base.binary, working_dir);
    if !binary_path.exists() {
        return Err(format!(
            "Binary file does not exist: {}",
            binary_path.display()
        ));
    }

    let binary_basename = binary_path
        .file_name()
        .and_then(|name| name.to_str())
        .ok_or("Invalid binary path")?;

    let results_dir = results_dir
        .map(PathBuf::from)
        .unwrap_or_else(|| Path::new(working_dir).join("results"));
    fs::create_dir_all(&results_dir).map_err(|e| {
        format!(
            "Failed to create results directory {}: {}",
            results_dir.display(),
            e
        )
    })?;

    let bugs_dir = results_dir.join("bugs");
    fs::create_dir_all(&bugs_dir).map_err(|e| {
        format!(
            "Failed to create bugs directory {}: {}",
            bugs_dir.display(),
            e
        )
    })?;

    let experiments = match &run.experiments {
        Some(exps) if !exps.is_empty() => exps.clone(),
        _ => vec![default_experiment()],
    };

    let pmem_target = base.pmem_mount.as_ref().map(PathBuf::from);
    if let Some(target) = &pmem_target {
        fs::create_dir_all(target)
            .map_err(|e| format!("Failed to create PMEM mount {}: {}", target.display(), e))?;
    }

    for (index, experiment) in experiments.iter().enumerate() {
        let effective = experiment.effective_config(run);
        let mut env_vars: HashMap<String, String> = HashMap::new();

        if let Some(base_env) = &base.environment {
            env_vars.extend(base_env.clone());
        }

        if let Some(pmem_mount) = &base.pmem_mount {
            env_vars.insert("PMEM_MOUNT".to_string(), pmem_mount.clone());
        }

        env_vars.insert("LD_PRELOAD".to_string(), build_ld_preload(&env_vars)?);

        if let Some(sampling) = effective.sampling {
            env_vars.insert("PIFR_SAMPLING".to_string(), sampling.to_string());
        }

        if let Some(report_type) = effective.report_type {
            env_vars.insert(
                "PIFR_REPORT".to_string(),
                report_type_to_value(&report_type).to_string(),
            );
        }

        if let Some(minimal_regions) = effective.minimal_regions {
            env_vars.insert(
                "PIFR_MINIMAL_REGIONS".to_string(),
                bool_to_env(minimal_regions),
            );
        }

        if let Some(delay) = effective.delay {
            env_vars.insert("PIFR_DELAY".to_string(), delay.to_string());
        }

        if let Some(decay) = effective.decay {
            env_vars.insert("PIFR_DECAY".to_string(), decay.to_string());
        }

        if let Some(profiling) = effective.profiling {
            env_vars.insert("PIFR_PROFILING".to_string(), bool_to_env(profiling));
        }

        if let Some(tracing) = effective.tracing {
            env_vars.insert("PIFR_TRACING".to_string(), bool_to_env(tracing));
        }

        let exp_name = format!(
            "{}-sampling_{}-delay_{}-decay_{}-{}-{}-{}.out",
            binary_basename,
            option_to_string(effective.sampling, "0"),
            option_to_string(effective.delay, "0"),
            option_to_string(effective.decay, "0"),
            option_to_string(effective.threads, "0"),
            option_to_string(effective.workload, "0"),
            index + 1
        );
        let output_path = results_dir.join(&exp_name);

        println!("{}", exp_name);

        if !base.pmem_dont_clean {
            if let Some(target) = &pmem_target {
                clear_dir_contents(target)?;
            }
        }

        let mut command = Command::new(&binary_path);
        if let Some(args) = &base.args {
            command.args(args);
        }

        command.current_dir(working_dir);
        command.envs(&env_vars);

        let timeout_seconds = effective.timeout_seconds.or(base.timeout_seconds);
        let (output, duration, timed_out) = run_command_with_timeout(command, timeout_seconds)?;

        let mut combined = Vec::new();
        combined.extend_from_slice(&output.stdout);
        combined.extend_from_slice(&output.stderr);
        let timing_summary = format_duration(duration);

        let mut file = File::create(&output_path).map_err(|e| {
            format!(
                "Failed to create output file {}: {}",
                output_path.display(),
                e
            )
        })?;
        file.write_all(&combined).map_err(|e| {
            format!(
                "Failed to write output file {}: {}",
                output_path.display(),
                e
            )
        })?;
        file.write_all(timing_summary.as_bytes())
            .map_err(|e| format!("Failed to write timing info: {}", e))?;

        print!("{}", String::from_utf8_lossy(&combined));
        print!("{}", timing_summary);

        if timed_out {
            eprintln!(
                "Warning: experiment timed out after {}s",
                timeout_seconds.unwrap()
            );
        }

        if !output.status.success() {
            eprintln!("Warning: experiment exited with status {}", output.status);
        }

        let bugs_txt = Path::new(RAMDISK_PATH).join(BUGS_TEXT_FILE);
        if bugs_txt.exists() {
            let dest = bugs_dir.join(format!("bugs-{}.txt", exp_name));
            move_file(&bugs_txt, &dest)?;
        } else {
            println!("Warning: {} does not exist", bugs_txt.display());
        }

        let bugs_bin = Path::new(RAMDISK_PATH).join(BUGS_BIN_FILE);
        if bugs_bin.exists() {
            let dest = bugs_dir.join(format!("bugs-{}.bin", exp_name));
            move_file(&bugs_bin, &dest)?;
        } else {
            println!("Warning: {} does not exist", bugs_bin.display());
        }

        println!("== == ==");
    }

    Ok(())
}
