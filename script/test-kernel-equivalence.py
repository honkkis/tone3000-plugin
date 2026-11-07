#!/usr/bin/env python3
"""Compare real A2 models on ARM: pre-PR portable, current portable, planar NEON.

Native Apple Silicon: python3 script/test-kernel-equivalence.py
Linux cross-run: --cxx aarch64-linux-gnu-g++ --runner 'qemu-aarch64 -L /usr/aarch64-linux-gnu'
Requires numpy, scipy, matplotlib, C++17 compiler, initialized NAM/Eigen submodule.
Returns 1 for ANY nonfinite output or bit mismatch, 2 for setup/build errors.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shlex
import subprocess
import tarfile
import sys

os.environ.setdefault('MPLCONFIGDIR', '/tmp/kernel-equivalence-matplotlib')
import numpy as np
from scipy import signal
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / 'plugin/NeuralAmpModelerCore'
ORIGINAL = '1f42f88535884450104b8711d7595019afa0495b'


def run(cmd, **kwargs):
    print('+', shlex.join(map(str, cmd)), flush=True)
    return subprocess.run(list(map(str, cmd)), check=True, **kwargs)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def metrics(a, b, rate):
    if a.shape != b.shape or not np.isfinite(a).all() or not np.isfinite(b).all():
        raise ValueError('Missing, unequal-length, or nonfinite render')
    diff = b - a
    rms = float(np.sqrt(np.mean(diff * diff)))
    ref_rms = float(np.sqrt(np.mean(a * a)))
    mismatch = np.flatnonzero(a.view(np.uint64) != b.view(np.uint64))
    result = dict(samples=len(a), bit_mismatches=len(mismatch),
                  first_mismatch=int(mismatch[0]) if len(mismatch) else None,
                  float32_bit_mismatches=int(np.count_nonzero(a.astype('f4').view('u4') != b.astype('f4').view('u4'))),
                  max_abs=float(np.max(np.abs(diff))), rms_error=rms,
                  error_dbfs=20 * np.log10(rms) if rms else None,
                  snr_db=20 * np.log10(ref_rms / rms) if rms and ref_rms else None,
                  reference_rms=ref_rms, stft={})
    for n in [256, 1024, 4096]:
        _, _, za = signal.stft(a, fs=rate, window='hann', nperseg=n, noverlap=3*n//4, boundary=None, padded=False)
        _, _, zb = signal.stft(b, fs=rate, window='hann', nperseg=n, noverlap=3*n//4, boundary=None, padded=False)
        mask = np.abs(za) > max(np.max(np.abs(za)) * 1e-4, 1e-15)
        db_diff = 20*np.log10(np.maximum(np.abs(zb[mask]), 1e-300) / np.abs(za[mask]))
        result['stft'][str(n)] = dict(
            complex_max_abs=float(np.max(np.abs(zb-za))),
            complex_relative_l2=float(np.linalg.norm(zb-za)/max(np.linalg.norm(za),1e-300)),
            magnitude_max_abs=float(np.max(np.abs(np.abs(zb)-np.abs(za)))),
            max_magnitude_db_above_minus80db=float(np.max(np.abs(db_diff))) if db_diff.size else 0.0)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cxx', default=os.environ.get('CXX', 'c++'))
    p.add_argument('--runner', default='')
    p.add_argument('--flags', default='-O3 -ffp-contract=fast')
    p.add_argument('--blocks', default='0,32,64,128,512', help='0 cycles small/odd/tile-edge blocks')
    p.add_argument('--model', type=Path, default=ROOT/'test/files/a2-amp-test.nam')
    p.add_argument('--output', type=Path, default=ROOT/'build/kernel-equivalence')
    p.add_argument('--samples', type=int, default=48017)
    args = p.parse_args()
    if args.samples < 8192: p.error('--samples must be >=8192 for long-history/STFT coverage')
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    for name in ['current-renders', 'original-renders']:
        folder = out/name
        folder.mkdir(exist_ok=True)
        for stale in folder.glob('sub*_*.f64'): stale.unlink()
    model = args.model.resolve()
    rate = json.loads(model.read_text()).get('sample_rate', 48000)
    # Exact input bytes are shared across all three engines. Use the checked-in
    # example audio, plus silence, impulse, sweep, seeded noise and overload.
    audio = CORE/'example_audio/input.wav'
    sr, recorded = wavfile.read(audio)
    if sr != rate: raise ValueError('Recorded input/model sample rate differs')
    if recorded.dtype.kind in 'iu': recorded = recorded.astype('f8') / 2**(np.iinfo(recorded.dtype).bits-1)
    if recorded.ndim == 2: recorded = recorded.mean(axis=1)
    x = np.zeros(args.samples, dtype='<f8'); n = args.samples//2
    active = np.flatnonzero(recorded)
    if not len(active): raise ValueError('Example WAV is entirely silent')
    audio_start = int(active[0])
    x[:n] = np.resize(recorded[audio_start:], n)
    if np.sqrt(np.mean(x[:n]**2)) < 1e-6: raise ValueError('Example audio segment is silent')
    parts = np.array_split(np.arange(n, args.samples), 5)
    x[parts[1][0]] = 1.0
    t = np.arange(len(parts[2])) / rate
    x[parts[2]] = 0.7*signal.chirp(t, f0=30, f1=rate*0.45, t1=max(t[-1],1/rate), method='logarithmic')
    x[parts[3]] = np.random.default_rng(3000).uniform(-1, 1, len(parts[3]))
    x[parts[4]] = np.tile([2.0, -2.0, 0.0, 1e-20], (len(parts[4])+3)//4)[:len(parts[4])]
    x.tofile(out/'input.f64')
    old = out/'original-core'; old.mkdir(exist_ok=True)
    archive = subprocess.check_output(['git','-C',str(CORE),'archive',ORIGINAL,'NAM'])
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar: tar.extractall(old, filter='data')
    include = ['-I'+str(CORE/'Dependencies/eigen'), '-I'+str(CORE/'Dependencies/nlohmann')]
    common = shlex.split(args.cxx) + ['-std=c++17','-DNAM_ENABLE_A2_FAST'] + shlex.split(args.flags)
    for name, source in [('current', CORE), ('original', old)]:
        cmd = common + ['-I'+str(source)] + include
        if name == 'original': cmd += ['-DORIGINAL_CORE']
        files = [ROOT/'test/kernel-equivalence/render.cpp',source/'NAM/dsp.cpp',source/'NAM/wavenet/a2_fast.cpp']
        if name == 'current': files += [source/'NAM/wavenet/a2_planar.cpp']
        run(cmd + list(map(str,files)) + ['-o',str(out/name)])
        result = subprocess.run(shlex.split(args.runner)+[str(out/name),str(model),str(out/'input.f64'),str(out/(name+'-renders')),args.blocks])
        if result.returncode not in (0,1): raise RuntimeError(f'{name} renderer failed: {result.returncode}')
    cases = {}; worst = None; worst_error = -1
    for path in sorted((out/'current-renders').glob('*_planar.f64')):
        key = path.name.removesuffix('_planar.f64')
        planar = np.fromfile(path, '<f8')
        reference = np.fromfile(path.with_name(key+'_reference.f64'), '<f8')
        original = np.fromfile(out/'original-renders'/(key+'_reference.f64'), '<f8')
        if len(planar) != args.samples: raise ValueError('Incomplete render')
        cases[key] = {label: metrics(a,b,rate) for label,a,b in [
            ('planar_vs_current_reference',reference,planar),
            ('planar_vs_pre_pr',original,planar),
            ('current_reference_vs_pre_pr',original,reference)]}
        error = cases[key]['planar_vs_pre_pr']['max_abs']
        if error > worst_error: worst_error=error; worst=(key,original,planar)
    expected = len(json.loads(model.read_text()).get('config',{}).get('submodels',[None])) * len(args.blocks.split(',')) * 3
    if len(cases) != expected: raise ValueError(f'Expected {expected} cases, found {len(cases)}')
    metadata = dict(core_revision=subprocess.check_output(['git','-C',str(CORE),'rev-parse','HEAD'],text=True).strip(),
                    eigen_revision=subprocess.check_output(['git','-C',str(CORE/'Dependencies/eigen'),'rev-parse','HEAD'],text=True).strip(),
                    pre_pr_revision=ORIGINAL, model=str(model.relative_to(ROOT)) if model.is_relative_to(ROOT) else str(model),
                    model_sha256=digest(model), input_sha256=digest(out/'input.f64'), example_audio_sha256=digest(audio),
                    sample_rate=rate, example_audio_start_sample=audio_start, compiler=subprocess.check_output(shlex.split(args.cxx)+['--version'],text=True).splitlines()[0],
                    flags=args.flags, runner=args.runner or 'native ARM', states={'0':'cold','1':'prewarmed','2':'dirty then reset / cached prewarm'},
                    stft='Hann; 75% overlap; no boundary extension or padding; dB mask at -80 dB relative to peak reference bin')
    report = dict(metadata=metadata, cases=cases)
    (out/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    key,a,b = worst; diff=b-a
    fig,axes=plt.subplots(3,1,figsize=(12,9),layout='constrained')
    time=np.arange(len(a))/rate
    axes[0].plot(time,a,label='pre-PR',lw=0.8); axes[0].plot(time,b,label='planar',lw=0.7,alpha=.65)
    axes[0].legend(); axes[0].set_ylabel('Output amplitude')
    axes[1].plot(time,diff,lw=.7); axes[1].set_ylabel('Planar − pre-PR'); axes[1].set_xlabel('Seconds')
    f,t,z=signal.stft(diff,rate,nperseg=1024,noverlap=768,boundary=None,padded=False)
    mesh=axes[2].pcolormesh(t,f,20*np.log10(np.maximum(np.abs(z),1e-15)),vmin=-180,vmax=0,shading='auto')
    axes[2].set(xlabel='Seconds',ylabel='Hz',title='Residual STFT magnitude (display floor −300 dBFS)')
    fig.colorbar(mesh,ax=axes[2],label='dBFS'); fig.suptitle('Real amp kernel comparison: '+key)
    fig.savefig(out/'comparison.png',dpi=160); plt.close(fig)
    lines=['# Kernel equivalence results','',f'Model: `{metadata["model"]}`; {args.samples} samples at {rate} Hz; {len(cases)} cases.',
           f'Core: `{metadata["core_revision"]}`; pre-PR: `{ORIGINAL}`.',
           f'Compiler: {metadata["compiler"]}; flags: `{args.flags}`; runner: `{metadata["runner"]}`.','',
           '| Comparison | Cases with different bits | Peak absolute error | Worst STFT complex difference |',
           '|---|---:|---:|---:|']
    failed=False
    for label in next(iter(cases.values())):
        rows=[c[label] for c in cases.values()]
        count=sum(r['bit_mismatches']>0 for r in rows); failed |= bool(count)
        peak=max(r['max_abs'] for r in rows)
        spectral=max(s['complex_max_abs'] for r in rows for s in r['stft'].values())
        lines.append(f'| {label} | {count}/{len(rows)} | {peak:.9g} | {spectral:.9g} |')
    lines += ['', 'All states: cold start, normal prewarm, dirty state followed by reset (cached prewarm in current core).',
              'Block 0 cycles 1,7,8,15,31,32,33,63,64,127,128,255,256,511,512 frames. All renders include a final partial block.',
              'Input: checked-in example audio followed by silence, impulse, logarithmic sweep, seeded noise, and ±2 overload.',
              'Double I/O matches NamEngine; internal kernels use float32. JSON includes float64/float32 bit mismatches, RMS, SNR and STFT metrics at 256/1024/4096 samples.',
              'Null dBFS/SNR denotes zero error (−∞ dBFS / +∞ SNR). Bit equality is strict, including signed zero.',
              'ARM emulation executes NEON arithmetic but does not certify Apple Clang/iPad behavior or measure performance.',
              'No resampling, EQ, cab IR, or gain processing is included; this isolates kernel output.', '',
              'Result: **'+('FAIL — bit differences detected.' if failed else 'PASS — all compared output bits identical.')+'**']
    (out/'RESULTS.md').write_text('\n'.join(lines)+'\n'); print('\n'.join(lines),flush=True)
    return int(failed)

if __name__ == '__main__':
    try: sys.exit(main())
    except (subprocess.CalledProcessError, RuntimeError, ValueError, OSError) as e:
        print(f'ERROR: {e}',file=sys.stderr); sys.exit(2)
