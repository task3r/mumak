use serde::{Deserialize, Serialize};
use std::fs;
use std::path::Path;

use crate::common::VardalithResult;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VardalithConfig {
    pub base: Option<BaseConfig>,
    pub run: Option<RunConfig>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BaseConfig {
    pub binary: String,
    pub args: Option<Vec<String>>,
    pub timeout_seconds: Option<u64>,
    pub environment: Option<std::collections::HashMap<String, String>>,
    pub pmem_mount: Option<String>,
    #[serde(default)]
    pub pmem_dont_clean: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum ReportType {
    Off,
    Minimal,
    Full,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RunConfig {
    pub experiments: Option<Vec<ExperimentConfig>>,
    pub sampling: Option<i32>,
    pub report_type: Option<ReportType>,
    pub minimal_regions: Option<bool>,
    pub delay: Option<i32>,
    pub decay: Option<i32>,
    pub threads: Option<usize>,
    pub workload: Option<usize>,
    pub profiling: Option<bool>,
    pub tracing: Option<bool>,
    pub timeout_seconds: Option<u64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExperimentConfig {
    // Experiment-specific overrides (inherit from RunConfig if None)
    pub sampling: Option<i32>,
    pub report_type: Option<ReportType>,
    pub minimal_regions: Option<bool>,
    pub delay: Option<i32>,
    pub decay: Option<i32>,
    pub threads: Option<usize>,
    pub workload: Option<usize>,
    pub profiling: Option<bool>,
    pub tracing: Option<bool>,
    pub timeout_seconds: Option<u64>,
}

#[allow(dead_code)]
impl VardalithConfig {
    pub fn from_toml_file(path: &str) -> VardalithResult<Self> {
        if !Path::new(path).exists() {
            return Err(format!("Configuration file does not exist: {}", path));
        }

        let content = fs::read_to_string(path)
            .map_err(|e| format!("Failed to read config file {}: {}", path, e))?;

        let config: VardalithConfig = toml::from_str(&content)
            .map_err(|e| format!("Failed to parse TOML config file {}: {}", path, e))?;

        Ok(config)
    }

    pub fn from_toml_str(content: &str) -> VardalithResult<Self> {
        let config: VardalithConfig =
            toml::from_str(content).map_err(|e| format!("Failed to parse TOML config: {}", e))?;

        Ok(config)
    }

    pub fn to_toml_string(&self) -> VardalithResult<String> {
        toml::to_string_pretty(self)
            .map_err(|e| format!("Failed to serialize config to TOML: {}", e))
    }

    pub fn save_to_file(&self, path: &str) -> VardalithResult<()> {
        let content = self.to_toml_string()?;
        fs::write(path, content)
            .map_err(|e| format!("Failed to write config file {}: {}", path, e))?;
        Ok(())
    }
}

impl Default for VardalithConfig {
    fn default() -> Self {
        VardalithConfig {
            base: Some(BaseConfig::default()),
            run: Some(RunConfig::default()),
        }
    }
}

impl Default for BaseConfig {
    fn default() -> Self {
        BaseConfig {
            binary: String::new(),
            args: None,
            timeout_seconds: None,
            environment: None,
            pmem_mount: None,
            pmem_dont_clean: false,
        }
    }
}

impl Default for RunConfig {
    fn default() -> Self {
        RunConfig {
            experiments: None,
            sampling: None,
            report_type: Some(ReportType::Minimal),
            minimal_regions: Some(false),
            delay: Some(0),
            decay: Some(0),
            threads: None,
            workload: None,
            profiling: Some(false),
            tracing: Some(false),
            timeout_seconds: None,
        }
    }
}

#[allow(dead_code)]
impl ExperimentConfig {
    /// Get the effective configuration for this experiment, inheriting from run config where needed
    pub fn effective_config(&self, run_config: &RunConfig) -> ExperimentConfig {
        ExperimentConfig {
            sampling: self.sampling.or(run_config.sampling),
            report_type: self.report_type.clone().or(run_config.report_type.clone()),
            minimal_regions: self.minimal_regions.or(run_config.minimal_regions),
            delay: self.delay.or(run_config.delay),
            decay: self.decay.or(run_config.decay),
            threads: self.threads.or(run_config.threads),
            workload: self.workload.or(run_config.workload),
            profiling: self.profiling.or(run_config.profiling),
            tracing: self.tracing.or(run_config.tracing),
            timeout_seconds: self.timeout_seconds.or(run_config.timeout_seconds),
        }
    }
}
