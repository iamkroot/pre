from pathlib import Path
import subprocess as sp
import logging
import os
import re
import time

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

def compare_output(src_file: Path):
    orig_exec = src_file.with_suffix(".orig")
    if not orig_exec.exists():
        print("3NOOOOOOOOOO\n"*10, src_file)
        return
    opt_exec = src_file.with_suffix(".opt")
    if not opt_exec.exists():
        print("2NOOOOOOOOOO\n"*10, src_file)
        return
    gvn_exec = src_file.with_suffix(".gvn")
    if not gvn_exec.exists():
        print("NOOOOOOOOOO\n"*10, src_file)
        return
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
    assert orig_res == opt_res
    if orig_out.read_bytes() != opt_out.read_bytes():
        return
    return orig_time, opt_time, gvn_time



def run_all_tests(path: Path):
    logging.info(f"Running tests in {path}")
    res = {}
    for src_file in path.iterdir():
        if src_file.suffix != ".c":
            continue
        replaced = run_test(src_file.stem, path)
        times = compare_output(src_file)
        if times is not None:
            res[src_file] = {
                "num_replaced": replaced,
                "orig_3NOOOOOOOOOOtime": times[0],
                "opt_time": times[1],
                "gvn_time": times[2],
            }

    print(*(f"{k.stem}\t{v['num_replaced']}\t{v['orig_time']}\t{v['opt_time']}" for k, v in res.items()), sep="\n", file=open("stats.tsv", "w"))

if __name__ == "__main__":
    run_all_tests(HANDWRITTEN_TESTS_DIR)
    # run_all_tests(BENCHGAME_TESTS_DIR)
