# Vardalith Usage Guide

Use `vardalith --help` or `vardalith <command> --help` for the exact CLI syntax.

## Commands

- `trace`: run the target under tracing and create a working directory
- `patch`: apply patches to a traced working directory
- `run`: run one or more experiments from a config and write outputs under a results directory
- `report`: read bug report binaries and produce a human-readable or JSON report
- `extras`: run auxiliary utilities on a working directory
- `results`: summarize experiment `.out` files produced by `run`

## Expected Workflow

1. Run `trace` with a config file. If you do not pass a working directory, vardalith creates one.
2. Run `patch` on that working directory.
3. Run `run` on that working directory with the same config.
4. Inspect:
   - `<workdir>/results` by default for experiment outputs
   - `<workdir>/results/bugs` by default for bug artifacts
5. Run `results` to summarize the `.out` files from `run`.
6. Run `report` on a reports directory when you want to inspect bug report binaries in detail.

## Config

`trace` and `run` use a TOML config with `[base]` and `[run]` sections. For a full example, see [example_config.toml](/Users/task3r/dev/mumak/src/vardalith/example_config.toml).

### `[base]`

Shared target configuration:
- `binary`
- `args`
- `timeout_seconds`
- `pmem_mount`
- `pmem_dont_clean`
- `[base.environment]`

### `[run]`

Default experiment settings for `run`:
- `sampling`
- `report_type`
- `minimal_regions`
- `delay`
- `decay`
- `threads`
- `workload`
- `profiling`
- `tracing`
- `timeout_seconds`

Each `[[run.experiments]]` entry inherits from `[run]` and can override any subset of those fields.

## Output Layout

- `trace` writes artifacts into the working directory.
- `run` writes `.out` files under `<workdir>/results` by default.
- `run` writes bug artifacts under `<workdir>/results/bugs` by default.
- `results` reads `.out` files from `<workdir>/results` by default.
