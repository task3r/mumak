use std::collections::HashMap;
use std::path::Path;

use crate::common::{VardalithConfig, VardalithResult, file_utils::find_files_by_extension};

#[derive(Debug, Hash, PartialEq, Eq)]
struct ExecutionConfig {
    sampling: usize,
    delay: usize,
    decay: usize,
    workload: usize,
    threads: usize,
    // iteration: usize,
}

#[derive(Debug, PartialEq)]
struct ExecutionResult {
    time: f64,
    uniq_praces: f64,
    total_praces: f64,
}

fn find_pattern<'a>(lines: &'a Vec<&str>, pattern: &str) -> Option<&'a str> {
    for line in lines {
        if line.starts_with(pattern) {
            return Some(line);
        }
    }
    None
}

fn parse_time(line: &str) -> VardalithResult<f64> {
    // line is "real    0m1.234s"
    let parts: Vec<&str> = line.split_whitespace().collect();
    if parts.len() != 2 {
        return Err(format!("Malformed time line: {}", line));
    }
    let time_str = parts[1];
    let min_sec: Vec<&str> = time_str.split('m').collect();
    if min_sec.len() != 2 {
        return Err(format!("Malformed time string: {}", time_str));
    }
    let minutes: f64 = min_sec[0]
        .parse()
        .map_err(|e| format!("Failed to parse minutes: {}", e))?;
    let seconds_str = min_sec[1].trim_end_matches('s');
    let seconds: f64 = seconds_str
        .parse()
        .map_err(|e| format!("Failed to parse seconds: {}", e))?;
    Ok(minutes * 60.0 + seconds)
}

fn parse_praces(line: &str) -> VardalithResult<f64> {
    // total  r/w pifr overlaps: 1234
    // unique r/w pifr overlaps: 123
    let parts: Vec<&str> = line.split(':').collect();
    if parts.len() != 2 {
        return Err(format!("Malformed praces line: {}", line));
    }
    Ok(parts[1]
        .trim()
        .parse()
        .map_err(|e| format!("Failed to parse praces count: {}", e))?)
}

fn grep_execution_result(result_file: &str) -> VardalithResult<ExecutionResult> {
    let content = std::fs::read_to_string(result_file)
        .map_err(|e| format!("Failed to read result file {}: {}", result_file, e))?;
    let lines: Vec<&str> = content.lines().collect();

    let maybe_time = lines[lines.len() - 3];
    let time = if maybe_time.starts_with("real") {
        parse_time(maybe_time)?
    } else {
        find_pattern(&lines, "real")
            .ok_or(format!(
                "Failed to find time in result file: {}",
                result_file
            ))
            .and_then(|line| parse_time(line))?
    };
    let maybe_uniq_praces = lines[lines.len() - 7];
    let uniq_praces = if maybe_uniq_praces.starts_with("unique r/w pifr overlaps:") {
        parse_praces(maybe_uniq_praces)?
    } else {
        find_pattern(&lines, "unique r/w pifr overlaps:")
            .ok_or(format!(
                "Failed to find unique praces in result file: {}",
                result_file
            ))
            .and_then(|line| parse_praces(line))?
    };
    let maybe_total_praces = lines[lines.len() - 8];
    let total_praces = if maybe_total_praces.starts_with("total  r/w pifr overlaps:") {
        parse_praces(maybe_total_praces)?
    } else {
        find_pattern(&lines, "total  r/w pifr overlaps:")
            .ok_or(format!(
                "Failed to find total praces in result file: {}",
                result_file
            ))
            .and_then(|line| parse_praces(line))?
    };

    Ok(ExecutionResult {
        time,
        uniq_praces,
        total_praces
    })
}

fn parse_execution_config_from_filename(filename: &str) -> Option<ExecutionConfig> {
    let parts: Vec<&str> = filename.split("/").last()?.split('-').collect();
    // files are $target-sampling_$sampling-delay_$delay-decay_$decay-8-$workload-$itr.out
    if parts.len() < 7 {
        return None;
    }
    let sampling: usize = parts[1].split('_').nth(1)?.parse().ok()?;
    let delay: usize = parts[2].split('_').nth(1)?.parse().ok()?;
    let decay: usize = parts[3].split('_').nth(1)?.parse().ok()?;
    let threads: usize = parts[4].parse().ok()?;
    let workload: usize = parts[5].parse().ok()?;
    // let iteration: usize = parts[6]
    //     .split('.')
    //     .next()?
    //     .parse()
    //     .ok()?;

    Some(ExecutionConfig {
        sampling,
        delay,
        decay,
        workload,
        threads,
    })
}

fn mean(data: &[f64]) -> Option<f64> {
    let sum = data.iter().sum::<f64>();
    let count = data.len();

    match count {
        positive if positive > 0 => Some(sum / count as f64),
        _ => None,
    }
}

fn stdev(data: &[f64]) -> Option<f64> {
    match (mean(data), data.len()) {
        (Some(data_mean), count) if count > 0 => {
            let variance = data.iter().map(|value| {
                let diff = data_mean - (*value as f64);

                diff * diff
            }).sum::<f64>() / count as f64;

            Some(variance.sqrt())
        },
        _ => None
    }
}

pub fn execute_results(
    working_dir: &str,
    results_dir: Option<&str>,
    config: &VardalithConfig,
) -> VardalithResult<()> {
    let _ = config;
    let results_root = results_dir
        .map(|dir| dir.to_string())
        .unwrap_or_else(|| Path::new(working_dir).join("results").display().to_string());
    let mut results: HashMap<ExecutionConfig, Vec<ExecutionResult>> = HashMap::new();
    let result_files = find_files_by_extension(&results_root, "out")?;
    for file in result_files {
        let execution_config = parse_execution_config_from_filename(&file);
        if execution_config.is_none() {
            println!("Skipping malformed result file: {}", file);
            continue;
        }
        let execution_config = execution_config.unwrap();
        let execution_result = grep_execution_result(&file);
        if execution_result.is_err() {
            println!("Skipping result file due to parsing error: {}", file);
            continue;
        }
        let execution_result = execution_result.unwrap();
        results
            .entry(execution_config)
            .or_insert_with(Vec::new)
            .push(execution_result);
    }

    const THREADS: usize = 8;
    const WORKLOADS: [usize; 3] = [1000,10000,100000];
    const SAMPLING: [usize; 2] = [10,0];
    const DELAY_CONFIGS: [(usize, usize); 3] = [(0,0), (1,0), (1,5)];
    for workload in WORKLOADS {
        for sampling in SAMPLING {
            for (delay, decay) in DELAY_CONFIGS {
                let config = ExecutionConfig{sampling,delay,decay,workload,threads: THREADS};
                //println!("{:?}", config);
                if let Some(executions) = results.get(&config) {
                    let times: Vec<f64> = executions.iter().map(|e| e.time).collect();
                    let races: Vec<f64> = executions.iter().map(|e| e.uniq_praces).collect();
                    let t_mean = mean(&times);
                    let r_mean = mean(&races);
                    if t_mean.is_none() || r_mean.is_none() {
                        print!(" & --- & ---");
                    } else {
                        let t_mean = t_mean.unwrap();
                        let r_mean = r_mean.unwrap();
                        let t_stdev = stdev(&times).unwrap();
                        let r_stdev = stdev(&races).unwrap();
                        print!(" & ${:.1} \\pm {:.1}$ & ${:.1} \\pm {:.1}$", t_mean, t_stdev, r_mean, r_stdev);
                    }
                } else {
                    print!(" & --- & ---");
                }
            }
        }
        println!("\\\\")
    }

    Ok(())
}
