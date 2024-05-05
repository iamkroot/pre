from pathlib import Path
import subprocess as sp
import logging
import os
import re
import time
import pandas

logging.basicConfig(level=logging.DEBUG, style="{", format="{asctime} - {levelname} - {message}")


TEST_DIR = Path(__file__).parent
HANDWRITTEN_TESTS_DIR = TEST_DIR / "handwritten"
assert HANDWRITTEN_TESTS_DIR.is_dir(), f"handwritten tests not found at {HANDWRITTEN_TESTS_DIR}"
BENCHGAME_TESTS_DIR = TEST_DIR / "benchgame"
assert BENCHGAME_TESTS_DIR.is_dir(), f"benchgame tests not found at {BENCHGAME_TESTS_DIR}"

PAT = re.compile(r"Number of expressions: (\d+)")

def run_test(name: str, path: Path):
    src_file = path / f"{name}.c"
    assert src_file.exists(), f"{src_file} not found"
    target = f"{path.name}/{name}"
    logging.info(f"Running test for {src_file}")
    err_file = src_file.with_suffix(".err.log")
    sp.call(["make", "run"], env={"TARGET": target, **os.environ}, cwd=TEST_DIR, stderr=err_file.open("wb"))
    replaced = 0
    for m in PAT.finditer(err_file.read_text()):
        replaced += int(m.group(1))
    return replaced

def compare_output(src_file: Path, skip_out_cmp=False):
    orig_exec = src_file.with_suffix(".orig")
    assert orig_exec.exists()
    opt_exec = src_file.with_suffix(".opt")
    assert opt_exec.exists()
    gvn_exec = src_file.with_suffix(".gvn")
    assert gvn_exec.exists()
    orig_out = src_file.with_suffix(".orig.out")
    opt_out = src_file.with_suffix(".opt.out")
    gvn_out = src_file.with_suffix(".gvn.out")
    try:
        start = time.time_ns()
        orig_res = sp.call(orig_exec, stdout=orig_out.open("wb"), timeout=10)
        orig_time = time.time_ns() - start
        gvn_start = time.time_ns()
        gvn_res = sp.call(gvn_exec, stdout=gvn_out.open("wb"), timeout=10)
        gvn_time = time.time_ns() - gvn_start
        opt_start = time.time_ns()
        opt_res = sp.call(opt_exec, stdout=opt_out.open("wb"), timeout=10)
        opt_time = time.time_ns() - opt_start
    except sp.TimeoutExpired:
        logging.warning(f"timeout with {src_file}")
        return
    assert orig_res == opt_res == gvn_res
    if not skip_out_cmp:
        assert orig_out.read_bytes() == opt_out.read_bytes()
    return orig_time, opt_time, gvn_time



def run_all_tests(path: Path):
    logging.info(f"Running tests in {path}")
    df = pandas.DataFrame()
    SKIP_OUT_CMP = ["flops"]
    for src_file in path.iterdir():
        if src_file.suffix != ".c":
            continue
        replaced = run_test(src_file.stem, path)
        skip_out_cmp = src_file.stem in SKIP_OUT_CMP
        try:
            times = compare_output(src_file, skip_out_cmp)
        except AssertionError as e:
            logging.exception(f"Error with {src_file}")
            continue
        if times is None:
            continue
        # for each file, have 10 runs. 
        runs = pandas.DataFrame()
        for i in range(10):
            times = compare_output(src_file)
            assert times is not None
            datapoint = {
                # "benchmark": src_file.stem,
                "run": i,
                "orig_time_ns": times[0],
                "opt_time_ns": times[1],
                "gvn_time_ns": times[2],
            }
            # add to pandas dataframe for each run
            runs = runs.append(datapoint, ignore_index=True)
        # for this file calculate the average time for each run, and add another column for the average
        df = df.append({
            "benchmark": src_file.stem,
            "num_replaced": replaced,
            "orig_time_ms": runs["orig_time_ns"].mean() / 1_000_000,
            "opt_time_ms": runs["opt_time_ns"].mean() / 1_000_000,
            "gvn_time_ms": runs["gvn_time_ns"].mean() / 1_000_000,
            "orig_time_std": runs["orig_time_ns"].std() / 1_000_000,
            "opt_time_std": runs["opt_time_ns"].std() / 1_000_000,
            "gvn_time_std": runs["gvn_time_ns"].std() / 1_000_000,
            "opt_time_speedup": (runs["orig_time_ns"].mean() - runs["opt_time_ns"].mean()) / runs["orig_time_ns"].mean(),
            "gvn_time_speedup": (runs["orig_time_ns"].mean() - runs["gvn_time_ns"].mean()) / runs["orig_time_ns"].mean(),
        }, ignore_index=True)

    df.to_clipboard()
    df.to_csv("stats.tsv", index=False, sep="\t")

if __name__ == "__main__":
    sp.check_call(["make", "clean"], cwd=TEST_DIR)
    # run_all_tests(HANDWRITTEN_TESTS_DIR)
    run_all_tests(BENCHGAME_TESTS_DIR)
