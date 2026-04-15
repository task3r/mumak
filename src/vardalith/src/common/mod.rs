pub mod config;
pub mod file_utils;
pub mod symbols;

pub use config::*;

pub type VardalithResult<T> = Result<T, String>;

pub const IMG_FILE: &str = "img.txt";
pub const TRACE_FILE: &str = "trace.bin";
pub const RAMDISK_PATH: &str = "/mnt/ramdisk";
