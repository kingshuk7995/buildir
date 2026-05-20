# Benchmark Results: Sequential vs Parallel Builds

To evaluate the improvements and raw capabilities of the `buildir` process pool and parsing logic, we created a realistic benchmarking script (`benchmark/generate.py`).

## Methodology
- **Scenario:** The experiment generates a synthetic C++ project consisting of 30 independent C++ source files, each including `<bits/stdc++.h>` to simulate a heavy compilation workload.
- **Commands:** Each target performs an actual compilation using `g++ -c -O2`, and we enforce `CCACHE_DISABLE=1` to ensure compilation happens from scratch.
- **Tooling:** The `time` command was used to measure elapsed real, user, and sys time.

## Execution Results

We tested three distinct scenarios using the newly improved `buildir` engine (incorporating implicit dependency nodes and optimized `posix_spawn` workers):

### 1. Sequential Execution (`-j 1`)
Forcing a single thread builds the project strictly one target after the other.
```text
real    0m23.820s
user    0m21.232s
sys     0m2.260s
```
**Observation:** Compiling 30 files sequentially takes ~23.8 seconds, heavily utilizing a single CPU core for roughly 0.7-0.8 seconds per file.

### 2. Parallel Execution (`-j 8`)
Using 8 concurrent workers.
```text
real    0m4.052s
user    0m26.305s
sys     0m3.031s
```
**Observation:** The tasks were distributed across 8 worker processes. We see a nearly 6x speedup ($23.8s \rightarrow 4.0s$). The user CPU time scales up since all 8 cores are being utilized simultaneously for actual heavy C++ compilation.

### 3. Parallel Auto (`-j 0` - Hardware Concurrency)
Allowing the system to detect and use maximum available hardware threads.
```text
real    0m3.409s
user    0m45.498s
sys     0m11.301s
```
**Observation:** The system leveraged the maximum available hardware cores. Due to the high parallelization, system overhead and CPU contention slightly increase the total user CPU time, but the overall wall-clock (`real`) time goes down further to 3.4 seconds, nearly a 7x speedup compared to sequential.
