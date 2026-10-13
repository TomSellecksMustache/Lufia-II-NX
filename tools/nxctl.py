#!/usr/bin/env python3
"""Fail-closed milestone runner for Lufia-II-NX.

All private ROM inputs, fetched upstream repositories, generated C and build
products remain OUTSIDE the Git repo. The runner cannot magically port a game;
it refuses playable releases until a real bridge and hardware testing exist.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[1]
CACHE = pathlib.Path(os.environ.get('LUFIA2NX_CACHE', '~/.cache/lufia2nx')).expanduser().resolve()
STATE = CACHE / 'private_state.json'
EXPECTED_SHA = '7c34ecb16c10f551120ed7b86cfbc947042f479b52ee74bb3c40e92fdd192b3a'
EXPECTED_SIZE = 2621440
UPSTREAM = 'https://github.com/Cellenseres/Lufia2SNESRecomp.git'
REPO = 'TomSellecksMustache/Lufia-II-NX'
STEPS = [
    ('0.0.1', 'Repository scaffold; MIT original files; safety scanning'),
    ('0.0.2', 'Private ROM fingerprint validation and local storage isolation'),
    ('0.1.0', 'Native Switch graphics/controller/audio diagnostic NRO'),
    ('0.2.0', 'Build existing native Lufia II desktop recompilation on Ubuntu'),
    ('0.3.0', 'Verify Switch face buttons and D-pad on hardware'),
    ('0.4.0', 'Verify SD layout and user-supplied ROM discovery'),
    ('0.5.0', 'Verify Switch video and audio diagnostics on hardware'),
    ('0.6.0', 'Game-engine integration: actual intro/title with audio/input'),
    ('0.7.0', 'Playable battle, dungeon, save/reload hardware validation'),
    ('0.8.0', 'Stable portable release: handheld/docked and regression tests'),
]
VERSIONS = [v for v, _ in STEPS]
BAD_SUFFIX = {'.sfc', '.smc', '.swc', '.fig', '.rom', '.srm', '.sav', '.nsp', '.nro', '.elf', '.nso', '.vpk', '.iso', '.bin', '.tga', '.png', '.jpg', '.jpeg', '.wav', '.ogg', '.mp3', '.7z', '.zip'}
BAD_SEGMENTS = {'build', '.cache', 'roms', 'generated', 'assets-src', 'private', 'src-gen', 'lufia2-gen'}

class GateError(RuntimeError):
    pass

# WYZE_NX_BRANDING_060: only these owned publisher images are permitted.
APPROVED_BRANDING = {
  'assets/wyzecorp-logo.png': 'ab2bfd07fac8a476aac8a7534bbf446113dcb901459bf28a805f0acbd73e85d8',
  'assets/wyzecorp-publisher.png': '5d72a053570e36a31637978231a2390875977f2f1d2f1f890c8c7b986f6cdfa8',
}
# SINX_V060_BRANDING_AUDIT: canonical user artwork only; historical JPG never restored.
APPROVED_BRANDING.update({
    'assets/SiNXstrals.png': '4c16ad3cf6bd8956bd1cdbf3673e871ff8f001e800526b65e47ac123bec14dcc',
    'assets/sinxstrals-v051-icon.jpg': 'HISTORICAL_ONLY',
})
def approved_branding(rel, file):
    name=rel.as_posix()
    if name == 'assets/sinxstrals-v051-icon.jpg':
        raise GateError('Legacy JPG prohibited in current tree')
    if name not in APPROVED_BRANDING: return False
    if file.is_symlink() or not file.is_file() or file.stat().st_size > 64000:
        raise GateError(f'Invalid branding asset: {name}')
    if hashlib.sha256(file.read_bytes()).hexdigest() != APPROVED_BRANDING[name]:
        raise GateError(f'Changed branding asset: {name}')
    return True

def run(*args, cwd=None, capture=False):
    print('+', ' '.join(str(a) for a in args), flush=True)
    kwargs = dict(cwd=str(cwd or ROOT), check=True)
    if capture: kwargs.update(stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return subprocess.run(list(map(str, args)), **kwargs)

def command(*args, cwd=None):
    return run(*args, cwd=cwd, capture=True).stdout.strip()

def load_state():
    if not STATE.exists(): return {}
    return json.loads(STATE.read_text())

def save_state(data):
    CACHE.mkdir(parents=True, exist_ok=True)
    tmp = STATE.with_suffix('.tmp')
    tmp.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')
    tmp.chmod(0o600)
    tmp.replace(STATE)

def set_rom(path):
    rom = pathlib.Path(path).expanduser().resolve(strict=True)
    if ROOT == rom or ROOT in rom.parents:
        raise GateError('ROM location is inside publishable repository. Move it OUTSIDE before proceeding.')
    if rom.stat().st_size != EXPECTED_SIZE:
        raise GateError(f'ROM size mismatch: expected {EXPECTED_SIZE}, received {rom.stat().st_size}')
    h = hashlib.sha256()
    with rom.open('rb') as f:
        for buf in iter(lambda: f.read(1024 * 1024), b''): h.update(buf)
    if h.hexdigest() != EXPECTED_SHA: raise GateError('ROM checksum mismatch: unsupported version')
    state = load_state(); state['rom_path'] = str(rom); save_state(state)
    print('ROM: SHA-256 verified; bytes remain in original local file (not copied).')
    return rom

def get_rom():
    p = os.environ.get('LUFIA2_ROM') or load_state().get('rom_path')
    if not p: raise GateError('Set ROM with: python3 tools/nxctl.py rom /absolute/path/game.sfc')
    return set_rom(p)

def git_files():
    proc = run('git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard', capture=True)
    return [pathlib.Path(s) for s in proc.stdout.split('\0') if s]

def audit():
    # Audit EVERY file eligible for commit, not just staged files.
    tracked = git_files()
    if not tracked: raise GateError('Git index/files absent: initialize repository first')
    for rel in tracked:
        if approved_branding(rel, ROOT / rel): continue
        if rel.suffix.lower() in BAD_SUFFIX or any(x.lower() in BAD_SEGMENTS for x in rel.parts):
            raise GateError(f'Forbidden publishable path: {rel}')
        f = ROOT / rel
        if f.is_symlink(): raise GateError(f'Symlinks prohibited (could disclose ROM): {rel}')
        if not f.is_file(): continue
        if f.stat().st_size > 1024 * 1024:
            raise GateError(f'Tracked file too large; inspect for assets/ROM: {rel}')
        b = f.read_bytes()
        if b'\0' in b: raise GateError(f'Binary file in publishable source: {rel}')
        if len(b) == EXPECTED_SIZE and hashlib.sha256(b).hexdigest() == EXPECTED_SHA:
            raise GateError(f'Original ROM present in source: {rel}')
    # Historical Git objects are immutable: detect any forbidden additions in prior commits too.
    try:
        history = command('git', 'log', '--all', '--pretty=format:', '--name-only')
        for name in history.splitlines():
            path = pathlib.Path(name.strip())
            if path.as_posix() in APPROVED_BRANDING: continue
            if name.strip() and (path.suffix.lower() in BAD_SUFFIX or any(x.lower() in BAD_SEGMENTS for x in path.parts)):
                raise GateError(f'Forbidden historical commit path: {name}; create a clean repo (do not force-push)')
    except subprocess.CalledProcessError:
        pass
    print(f'Source audit: PASS ({len(tracked)} publishable files, no ROM/binary/assets)')

def dependency_check():
    if not shutil.which('make'): raise GateError('Missing make (sudo apt install make)')
    dkp = pathlib.Path(os.environ.get('DEVKITPRO', '/opt/devkitpro'))
    if not (dkp / 'libnx/switch_rules').exists():
        raise GateError('Switch toolchain missing. Run ./scripts/install-toolchain.sh first.')
    # sdl2-config is normally installed by the devkitPro switch-sdl2 package.
    for p in [dkp / 'portlibs/switch/bin/sdl2-config', dkp / 'tools/bin/sdl2-config']:
        if p.exists(): return dkp, p
    binary = shutil.which('sdl2-config')
    if not binary: raise GateError('Switch SDL2 port missing. Install via dkp-pacman -S switch-sdl2')
    if 'devkitpro' not in str(pathlib.Path(binary).resolve()):
        raise GateError('sdl2-config points to HOST SDL2, not Switch. Add /opt/devkitpro/portlibs/switch/bin to PATH')
    return dkp, pathlib.Path(binary)

def build_switch():
    dkp, sdl = dependency_check()
    env = os.environ.copy()
    env['DEVKITPRO'] = str(dkp)
    env['DEVKITA64'] = str(dkp / 'devkitA64')
    env['PATH'] = str(sdl.parent) + ':' + str(dkp / 'tools/bin') + ':' + str(dkp / 'devkitA64/bin') + ':' + env['PATH']
    # The source checkout is publishable, but ALL compiled output is ignored.
    sw = CACHE / 'switch-worktree'
    if sw.exists(): shutil.rmtree(sw)
    shutil.copytree(ROOT / 'ports/switch', sw, ignore=shutil.ignore_patterns('build', '*.elf', '*.nro', '*.nacp', '*.o', '*.d'))
    p = subprocess.run(['make', '-C', str(sw), '-j', str(min(8, os.cpu_count() or 2))], env=env)
    if p.returncode: raise GateError('Switch build failed; no release/tag created')
    nro = sw / 'lufia2nx.nro'
    if not nro.is_file() or nro.stat().st_size < 1024:
        raise GateError('No valid-size .nro produced')
    out = CACHE / 'artifacts'; out.mkdir(parents=True, exist_ok=True)
    shutil.copy2(nro, out / 'lufia2nx-shell.nro')
    print(f'Native Switch diagnostic built: {out / "lufia2nx-shell.nro"}')
    return out / 'lufia2nx-shell.nro'

def desktop():
    rom = get_rom()
    src = CACHE / 'upstream/Lufia2SNESRecomp'
    src.parent.mkdir(parents=True, exist_ok=True)
    if not (src / '.git').exists():
        run('git', 'clone', '--recursive', UPSTREAM, src)
    run('git', 'submodule', 'update', '--init', '--recursive', cwd=src)
    print('Upstream snapshot:', command('git', 'rev-parse', 'HEAD', cwd=src))
    bld = CACHE / 'desktop-build'
    run('cmake', '-S', src, '-B', bld, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', f'-DLUFIA2_ROM={rom}')
    run('cmake', '--build', bld, '--parallel', str(min(8, os.cpu_count() or 2)))
    exe = bld / 'Lufia2Recomp'
    if not exe.exists(): raise GateError('Upstream desktop executable missing after build')
    print(f'Desktop native recompilation built: {exe}')
    return exe

def hardware_gate(version, prompts):
    state = load_state()
    confirmations = state.setdefault('hardware', {})
    if version in confirmations:
        print('Hardware confirmation on file:', version, confirmations[version]['date'])
        return
    if not sys.stdin.isatty():
        raise GateError(f'Real hardware test for {version} requires an interactive terminal; cannot automatically pass.')
    print('\nHARDWARE TEST', version, '\nThis is a user-attested test, NOT automated proof.\n')
    for p in prompts:
        answer = input(p + ' [type YES]: ').strip()
        if answer != 'YES': raise GateError(f'Hardware gate not passed for {version}')
    confirmations[version] = {'date': datetime.now(timezone.utc).isoformat(), 'tests': prompts}
    save_state(state)

def bridge_guard():
    stub = (ROOT / 'ports/switch/source/game_bridge.c').read_text()
    if 'Unimplemented on purpose' in stub or 'return false; }' in stub:
        raise GateError('No real game core! nxgame_* bridge is still the diagnostic stub. This CANNOT be auto-generated from a desktop build.')
    # Even changed source is not evidence of correctness: hardware gate follows.

def tests():
    run(sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-v')
    audit()

def stage_checks(v):
    tests()
    if v >= '0.0.2': get_rom()
    if v in ('0.1.0', '0.3.0', '0.4.0', '0.5.0', '0.6.0', '0.7.0', '0.8.0'):
        build_switch()
    if v == '0.2.0':
        desktop()
        hardware_gate(v, ['Have you run the desktop executable with your verified ROM and reached the title screen?',
                          'Can you enter gameplay and hear sound on the desktop version?'])
    if v == '0.3.0':
        hardware_gate(v, ['Does the diagnostic launch on a homebrew-capable Switch?',
                          'Do all D-pad and A/B/X/Y/L/R indicators react correctly?'])
    if v == '0.4.0':
        hardware_gate(v, ['Is the ROM on your Switch SD card OUTSIDE the published code?',
                          'Does the diagnostic detect the local ROM without packing it inside the NRO?'])
    if v == '0.5.0':
        hardware_gate(v, ['Is the diagnostic image correctly shown handheld AND docked?',
                          'Does holding B produce the diagnostic tone without audio glitches?'])
    if v in ('0.6.0', '0.7.0', '0.8.0'):
        bridge_guard()
        if v == '0.6.0':
            hardware_gate(v, ['Does Lufia II (not the diagnostic screen) boot to the genuine title?',
                              'Do title input and original music function on Switch?'])
        if v == '0.7.0':
            hardware_gate(v, ['Can you enter a dungeon and finish a battle on Switch?',
                              'Does in-game SAVE then complete EXIT and RESTART reload correctly?',
                              'Is every menu usable without touchscreen input?'])
        if v == '0.8.0':
            hardware_gate(v, ['Have you tested the game in handheld and docked modes?',
                              'Have you completed an extended playthrough without crashes?',
                              'Have you verified sound, controls, saves and shutdown after a cold boot?'])

def publish_version(v, do_publish, attach_shell):
    if v not in VERSIONS: raise GateError(f'Unknown milestone: {v}')
    tag = 'v' + v
    tags = command('git', 'tag', '--list', tag)
    if tag in tags.splitlines():
        raise GateError(f'{tag} already tagged. Releasing again would rewrite history.')
    title = dict(STEPS)[v]
    version_file = ROOT / 'VERSION'; version_file.write_text(v + '\n')
    notes = ROOT / 'docs/RELEASE_NOTES.md'
    notes.write_text(f'# {tag}\n\n{title}\n\n')
    tests()
    # Deliberately add ONLY the whitelisted release files; no recursive add.
    run('git', 'add', '--', 'VERSION', 'docs/RELEASE_NOTES.md')
    audit()
    run('git', 'commit', '--allow-empty', '-m', f'release: {tag} - {title}')
    run('git', 'tag', '-a', tag, '-m', f'{tag}: {title}')
    if do_publish:
        run('git', 'push', 'origin', 'HEAD')
        run('git', 'push', 'origin', tag)
        release_args = ['gh', 'release', 'create', tag, '--repo', REPO,
                        '--title', f'{tag} - {title}', '--notes-file', str(notes)]
        if v != '0.8.0': release_args.append('--prerelease')
        # Only the ROM-FREE DIAGNOSTIC is eligible for binary GitHub attachment.
        if attach_shell and v in ('0.1.0','0.3.0','0.4.0','0.5.0'):
            release_args.append(str(CACHE / 'artifacts/lufia2nx-shell.nro'))
        run(*release_args)
    print(f'Completed {tag}; ' + ('pushed/released.' if do_publish else 'LOCAL ONLY.'))

def status():
    try: tags = command('git', 'tag', '--list', 'v*').splitlines()
    except subprocess.CalledProcessError: tags = []
    print('Project:', REPO)
    print('Local releases:', ', '.join(tags) or 'none')
    for ver, desc in STEPS:
        print(('DONE' if 'v'+ver in tags else 'TODO').ljust(5), ver, desc)
    print('Cache:', CACHE)
    print('ROM:', 'configured locally' if load_state().get('rom_path') else 'not configured')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='cmd', required=True)
    q=sub.add_parser('rom'); q.add_argument('path')
    sub.add_parser('audit');sub.add_parser('test');sub.add_parser('status')
    sub.add_parser('build-switch');sub.add_parser('build-desktop')
    q=sub.add_parser('run-desktop')
    q=sub.add_parser('milestone');q.add_argument('version', choices=VERSIONS)
    q.add_argument('--publish', action='store_true')
    q.add_argument('--attach-shell', action='store_true')
    q=sub.add_parser('iterate');q.add_argument('--until', choices=VERSIONS, default='0.8.0')
    q.add_argument('--publish', action='store_true')
    q.add_argument('--attach-shell', action='store_true')
    q=sub.add_parser('install-sd');q.add_argument('sd_mount', help='Mounted SD card root directory')
    q.add_argument('--copy-rom', action='store_true', help='Explicit permission to copy ROM ONLY to personal SD card')
    args=p.parse_args()
    if args.cmd=='rom': set_rom(args.path)
    elif args.cmd=='audit': audit()
    elif args.cmd=='test': tests()
    elif args.cmd=='status': status()
    elif args.cmd=='build-switch': build_switch()
    elif args.cmd=='build-desktop': desktop()
    elif args.cmd=='run-desktop':
        exe = CACHE / 'desktop-build/Lufia2Recomp'
        if not exe.exists(): exe=desktop()
        run(exe, get_rom(), cwd=exe.parent)
    elif args.cmd=='install-sd':
        m=pathlib.Path(args.sd_mount).expanduser().resolve(strict=True)
        if not m.is_dir(): raise GateError('SD mount must be a directory')
        source=CACHE/'artifacts/lufia2nx-shell.nro'
        if not source.exists(): raise GateError('Build diagnostic NRO before SD install')
        target=m/'switch/Lufia-II-NX';target.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target/'lufia2nx.nro')
        (target/'saves').mkdir(exist_ok=True)
        if args.copy_rom:
            rom=get_rom()
            shutil.copy2(rom, target/'lufia2.sfc')
        print('Installed diagnostic to', target, '(ROM only copied if --copy-rom was specified)')
    elif args.cmd in ('milestone','iterate'):
        vers=[args.version] if args.cmd=='milestone' else VERSIONS[:VERSIONS.index(args.until)+1]
        for v in vers:
            if 'v'+v in command('git','tag','--list','v*').splitlines():
                print('Skipping existing',v); continue
            # Previous stages must be present, in order.
            before=VERSIONS[:VERSIONS.index(v)]
            missing=[b for b in before if 'v'+b not in command('git','tag','--list','v*').splitlines()]
            if missing: raise GateError('Earlier milestones missing: '+', '.join(missing))
            print('\n========== GATE', v, '==========\n')
            stage_checks(v)
            publish_version(v, args.publish, args.attach_shell)

if __name__=='__main__':
    try:main()
    except (GateError, subprocess.CalledProcessError, OSError) as exc:
        print('STOPPED SAFELY:',exc,file=sys.stderr)
        sys.exit(1)
