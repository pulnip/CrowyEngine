"""Build one prop from its module under assets/ and export it as FBX.

Run headless from the repo root (Tools/gen.py wraps this call):

    blender --background --factory-startup --python-exit-code 1 \\
        --python Tools/Blender/generate.py -- \\
        --asset <name> [--preview DIR] [--force]

The FBX goes to Content/Props/<Family>/<Name>.fbx and the card's moving
parts to <Name>.prop.json beside it. The run fails (exit code 1, no export)
when the card is incomplete, the build misses it, or the module has no
build function. The export is skipped when the input hash matches the
sidecar; --force overrides that. Faces of two materials that overlap in one
plane (they would flicker) are listed as [zfight] lines; they do not stop
the export.
"""
import argparse
import importlib.util
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from propkit import CONTENT, ROOT  # noqa: E402
from propkit import shop  # noqa: E402
from propkit import spec as spec_check  # noqa: E402
from propkit.export import (  # noqa: E402
    export_fbx, input_hash, is_up_to_date, prop_file_of, write_prop_file)
from propkit.kit import MeshBuilder  # noqa: E402
from propkit.overlap import describe, fights, solids_of  # noqa: E402
from propkit.preview import render_sheet  # noqa: E402
from propkit.scene import reset_scene  # noqa: E402

MODELS = CONTENT / 'Props'


def parse_args():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--asset', required=True,
                        help='module name under Tools/Blender/assets/')
    parser.add_argument('--preview', help='directory for the preview sheet')
    parser.add_argument('--force', action='store_true',
                        help='export even when the input hash is unchanged')
    return parser.parse_args(argv)


def asset_path(name):
    return HERE / 'assets' / f'{name}.py'


def load_asset(name):
    path = asset_path(name)
    if not path.exists():
        raise SystemExit(f'no asset module {path}')
    module_spec = importlib.util.spec_from_file_location(
        f'assets.{name}', path)
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    return module


def fail(card, failures):
    print(f"[spec] {card.get('name', '?')}")
    for failure in failures:
        print(f'[spec] FAIL {failure}')
    sys.exit(1)


def main():
    args = parse_args()
    asset = load_asset(args.asset)
    card = asset.SPEC
    scene = reset_scene()

    failures = spec_check.check_card(card)
    if not hasattr(asset, 'build'):
        failures.append('the module has no build function')
    if failures:
        fail(card, failures)

    kit = MeshBuilder()
    asset.build(kit)
    failures = spec_check.check_build(card, kit)
    if failures:
        fail(card, failures)

    materials = shop.materials(card['palette'])
    objects = kit.build(card['name'], card['palette'], materials)
    owners = [part or card['name'] for _, _, _, part in kit.faces]
    sites = fights([(owner, material, [kit.vertices[i] for i in indices])
                    for owner, (indices, material, _, _)
                    in zip(owners, kit.faces)],
                   solids_of(kit.vertices,
                             [indices for indices, _, _, _ in kit.faces]))
    for pair, site in sorted(sites.items(), key=lambda item: -item[1][0]):
        print(f"[zfight] {' / '.join(dict.fromkeys(pair))}: "
              f'{describe(*site)}')

    failures = spec_check.check(card, objects, kit)
    for failure in failures:
        print(f'[spec] FAIL {failure}')
    if not failures:
        export(args, card, objects)
    # The preview adds a camera and helpers that would leak into the FBX, so
    # it always runs after the export.
    if args.preview:
        sheet = ROOT / args.preview / f"{card['name']}.png"
        render_sheet(scene, objects, materials, sheet)
    if failures:
        sys.exit(1)


def export(args, card, objects):
    out = MODELS / card['family'] / f"{card['name']}.fbx"
    digest = input_hash(asset_path(args.asset))
    shown = out.relative_to(ROOT).as_posix()
    if not args.force and is_up_to_date(out, digest):
        print(f'[export] unchanged {shown} ({digest[:12]})')
        return
    export_fbx(out, objects, digest)
    write_prop_file(out, card)
    print(f'[export] {shown} ({digest[:12]})')
    print(f'[export] {prop_file_of(out).relative_to(ROOT).as_posix()}')


if __name__ == '__main__':
    main()
