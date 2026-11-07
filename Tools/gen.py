"""Generate props with Blender and print only what matters.

Usage (from the repo root):
    python Tools/gen.py <module> [<module> ...] [--force] [--no-preview]
    python Tools/gen.py --all [--jobs N]

<module> is a file name under Tools/Blender/assets/ without .py. Each prop
gets its FBX and <Name>.prop.json under Content/Props/<Family>/ and its
preview sheet under captures/prop/previews/. The output is the generator's
[spec], [export], [preview] and [zfight] lines (faces of two materials
overlapping in one plane), any traceback, and one [gen] line per prop. The
exit code is 0 only when every prop passed.
"""
import argparse
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'Tools' / 'Blender' / 'assets'
GENERATE = ROOT / 'Tools' / 'Blender' / 'generate.py'
PREVIEWS = 'captures/prop/previews'
DEFAULT_BLENDER = Path(
    r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe')
KEPT = ('[spec]', '[export]', '[preview]', '[zfight]')


def find_blender():
    override = os.environ.get('BLENDER')
    if override:
        return override
    if DEFAULT_BLENDER.exists():
        return str(DEFAULT_BLENDER)
    found = shutil.which('blender')
    if found:
        return found
    raise SystemExit('Blender was not found; set the BLENDER variable')


def filtered(output):
    """The tagged lines, and everything from a traceback to its last line."""
    lines = []
    in_traceback = False
    for line in output.splitlines():
        if line.startswith('Traceback'):
            in_traceback = True
        if in_traceback:
            if line.startswith(('Blender quit', 'Blender ')):
                in_traceback = False
                continue
            lines.append(line)
        elif line.startswith(KEPT) or 'Error:' in line:
            lines.append(line)
    return lines


def generate(blender, module, force, preview):
    command = [blender, '--background', '--factory-startup',
               '--python-exit-code', '1', '--python', str(GENERATE), '--',
               '--asset', module]
    if preview:
        command += ['--preview', PREVIEWS]
    if force:
        command.append('--force')
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            text=True, encoding='utf-8', errors='replace')
    lines = filtered(result.stdout + result.stderr)
    status = 'ok' if result.returncode == 0 else (
        f'FAILED (exit {result.returncode})')
    lines.append(f'[gen] {module}: {status}')
    return result.returncode, lines


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('modules', nargs='*')
    parser.add_argument('--all', action='store_true',
                        help='every module under Tools/Blender/assets/')
    parser.add_argument('--force', action='store_true',
                        help='export even when the input hash is unchanged')
    parser.add_argument('--no-preview', action='store_true')
    parser.add_argument('--jobs', type=int, default=1,
                        help='props generated at the same time')
    args = parser.parse_args()

    modules = list(args.modules)
    if args.all:
        modules = sorted(path.stem for path in ASSETS.glob('*.py')
                         if not path.stem.startswith('_'))
    if not modules:
        parser.error('name at least one module, or pass --all')
    for module in modules:
        if not (ASSETS / f'{module}.py').exists():
            parser.error(f'no module Tools/Blender/assets/{module}.py')

    blender = find_blender()
    failed = 0
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        runs = pool.map(
            lambda module: generate(blender, module, args.force,
                                    not args.no_preview), modules)
        for code, lines in runs:
            print('\n'.join(lines), flush=True)
            failed += code != 0
    if len(modules) > 1:
        print(f'[gen] {len(modules) - failed} of {len(modules)} passed')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
