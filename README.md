# Partial Redundancy Elimination (PRE) using Lazy Code Motion (LCM)

Implements the LCM algorithm introduced by Knoop, Rüthing, and Steffen.

## Testing
`tests` directory contains tests for the LCM PRE pass.

### Running
* Use `python run.py` to run all tests.
    * You'll need `pandas` installed - needed for generating stats CSVs.
* To run an individual test, use `TARGET=handwritten/test1 make run` (change the `TARGET` as appropriate)

### Source
* The `handwritten` tests represent some common CFG patterns meant to test the effectiveness of LCM PRE.
* The `benchgame` directory contains some real-world C programs
    * taken from [The Computer Language 24.04 Benchmarks Game](https://benchmarksgame-team.pages.debian.net/benchmarksgame/index.html)
    * and from [LLVM Single Source](https://github.com/llvm/llvm-test-suite/tree/main/SingleSource) test suite
