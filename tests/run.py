from pathlib import Path
import subprocess as sp
import logging

logging.basicConfig(level=logging.DEBUG, style="{", format="{asctime} - {levelname} - {message}")


TEST_DIR = Path(__file__).parent
HANDWRITTEN_TESTS_DIR = TEST_DIR / "handwritten"
assert HANDWRITTEN_TESTS_DIR.is_dir(), f"handwritten tests not found at {HANDWRITTEN_TESTS_DIR}"
BENCHGAME_TESTS_DIR = TEST_DIR / "benchgame"
assert BENCHGAME_TESTS_DIR.is_dir(), f"benchgame tests not found at {BENCHGAME_TESTS_DIR}"

def run_test(name: str, path: Path):
    src_file = path / f"{name}.c"
    assert src_file.exists(), f"{src_file} not found"
    target = f"{path.name}/{name}"
    logging.info(f"Running test for {src_file}")
    sp.check_call(["make", "run"], env={"TARGET": target}, cwd=TEST_DIR)


def run_all_tests(path: Path):
    logging.info(f"Running tests in {path}")
    for src_file in path.iterdir():
        if src_file.suffix != ".c":
            continue
        run_test(src_file.stem, path)

if __name__ == "__main__":
    run_all_tests(HANDWRITTEN_TESTS_DIR)
