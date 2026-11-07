# Kernel equivalence results

Model: `test/files/a2-amp-test.nam`; 48017 samples at 48000 Hz; 30 cases.
Core: `552c5ab54cb8c2027ba9567c3ae974884e5b68c5`; pre-PR: `1f42f88535884450104b8711d7595019afa0495b`.
Compiler: aarch64-linux-gnu-g++ (Debian 14.2.0-19) 14.2.0; flags: `-O3 -ffp-contract=fast`; runner: `/tmp/kernel-toolchain/usr/bin/qemu-aarch64 -L /tmp/kernel-toolchain/usr/aarch64-linux-gnu`.

| Comparison | Cases with different bits | Peak absolute error | Worst STFT complex difference |
|---|---:|---:|---:|
| planar_vs_current_reference | 0/30 | 0 | 0 |
| planar_vs_pre_pr | 0/30 | 0 | 0 |
| current_reference_vs_pre_pr | 0/30 | 0 | 0 |

All states: cold start, normal prewarm, dirty state followed by reset (cached prewarm in current core).
Block 0 cycles 1,7,8,15,31,32,33,63,64,127,128,255,256,511,512 frames. All renders include a final partial block.
Input: checked-in example audio followed by silence, impulse, logarithmic sweep, seeded noise, and ±2 overload.
Double I/O matches NamEngine; internal kernels use float32. JSON includes float64/float32 bit mismatches, RMS, SNR and STFT metrics at 256/1024/4096 samples.
Null dBFS/SNR denotes zero error (−∞ dBFS / +∞ SNR). Bit equality is strict, including signed zero.
ARM emulation executes NEON arithmetic but does not certify Apple Clang/iPad behavior or measure performance.
No resampling, EQ, cab IR, or gain processing is included; this isolates kernel output.

Result: **PASS — all compared output bits identical.**
