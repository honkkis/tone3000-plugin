# Real amp kernel equivalence

Run on Apple Silicon (or an ARM Linux host with FMA/NEON):

```sh
git submodule update --init --recursive plugin/NeuralAmpModelerCore
python3 -m pip install numpy scipy matplotlib
python3 script/test-kernel-equivalence.py
```

This standalone test requires no JUCE build. It uses the real trained
`test/files/a2-amp-test.nam` fixture, including both its 3-channel and
8-channel submodels. It compares three implementations using the same model
weights and exact input bytes:

* Planar NEON kernels pinned by this branch.
* Portable A2 reference in the same current core.
* Original portable kernels at `1f42f88535884450104b8711d7595019afa0495b`,
  the revision pinned before the planar integration. The script extracts the
  original sources from the core's Git history into the build directory.

The original sources are built against the pinned Eigen/nlohmann headers.
Double I/O matches the plugin; arithmetic inside the models is float32.
The test refuses to build if the actual planar kernels are unavailable.
It does not silently compare the x86 fallback with itself.

For x86 Linux with an AArch64 cross compiler and QEMU:

```sh
python3 script/test-kernel-equivalence.py \
  --cxx aarch64-linux-gnu-g++ \
  --runner 'qemu-aarch64 -L /usr/aarch64-linux-gnu'
```

Default coverage: 48,017 samples per case; both model widths; fixed blocks
32/64/128/512 plus a mixed schedule containing 1/7/8/15/31/32/33/63/64/127/
128/255/256/511/512; cold, prewarmed, and dirty-then-reset states (30 cases).
The input combines example audio from the core's example WAV with silence,
an impulse, a logarithmic sweep, seeded noise, and overload values.
Use `--samples 192017 --blocks 0,1,7,8,31,32,33,64,127,128,256,512` for a
longer run. `--model` selects another real A2 fixture.

`build/kernel-equivalence/` receives raw double renders, exact input,
`report.json`, `RESULTS.md`, and `comparison.png`. The analysis checks every
output bit (including signed zero), finite values, peak/RMS errors, SNR,
and complex/magnitude STFT differences with Hann windows of 256/1024/4096
samples and 75% overlap. It returns 1 for any difference, even if small,
and 2 for setup/build failures. A null error dBFS or SNR means zero error.

Compiler flags default to `-O3 -ffp-contract=fast`, without fast-math.
Use `--flags` to repeat with the exact flags of another build. Emulated
AArch64/GCC establishes behavior for that build; repeat natively with the
shipping Apple Clang toolchain before treating it as an iPad certification.
No performance claim follows from emulated execution. This test isolates
model kernels; it does not exercise JUCE resampling, EQ, gain or cab IRs.

The committed `results/` directory records the executed run and its provenance.
