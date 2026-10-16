#!/usr/bin/env python3
"""Freeze the small fork-validation pilot; this command does not submit jobs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('release')
    args = parser.parse_args()
    release = Path(args.release).resolve()
    inputs = release / 'pilot-inputs'
    inputs.mkdir(exist_ok=False)
    old = Path('/afs/desy.de/user/l/lobis/lead-thickness-rerun-20261015-v4')
    cells = json.loads(Path('/afs/desy.de/user/l/lobis/naf-neutrons-500-20261015/pilot-cells.json').read_text())
    cell = next(item for item in cells if item['arm'] == 'closed-000')
    for kind in ('geometry', 'rml'):
        if digest(cell[kind + '_path']) != cell[kind + '_sha256']:
            raise RuntimeError('Changed validated detector input: ' + kind)
    reference = Path('/data/dust/user/lobis/environmental-shield-large-20260926/native-reference.root')
    if digest(reference) != 'f4709ce0426cafca83f5660f6e22e74a0f855fbab13009926d46f8c4b10781bd':
        raise RuntimeError('Changed calibrated native-response reference')
    shutil.copyfile(cell['geometry_path'], inputs / 'geometry.gdml')
    shutil.copyfile(reference, inputs / 'native-reference.root')
    shutil.copyfile(old / 'ReplayExistingNative.C', inputs / 'ReplayExistingNative.C')
    scripts = Path(__file__).resolve().parent
    for name in ('physics_pilot.py', 'validate_transport.C'):
        shutil.copyfile(scripts / name, inputs / name)
    tree = ET.parse(cell['rml_path'])
    metadata = tree.getroot().find('TRestGeant4Metadata')
    for item in metadata.findall('parameter'):
        if item.get('name') == 'gdmlFile':
            item.set('value', str(inputs / 'geometry.gdml'))
    metadata.remove(metadata.find('generator'))
    generator = ET.SubElement(metadata, 'generator', type='point', position='(0,0,-1.75)', units='mm')
    source = ET.SubElement(generator, 'source', particle='gamma')
    ET.SubElement(source, 'angularDist', type='flux', direction='(0,0,1)')
    ET.SubElement(source, 'energyDist', type='mono', energy='5.9', units='keV')
    tree.write(inputs / 'simulation.rml', encoding='utf-8', xml_declaration=True)
    pins = dict(parents=200, source_seed=2026101601, response_seed=2026101611,
                diagnostic_only=True, files={p.name: digest(p) for p in inputs.iterdir() if p.is_file()})
    (inputs / 'inputs.json').write_text(json.dumps(pins, indent=2) + '\n')
    script = release / 'pilot.sh'
    script.write_text('#!/bin/bash\nset -euo pipefail\nexec python3 ' + str(inputs / 'physics_pilot.py') +
                      ' --inputs ' + str(inputs) + '\n')
    print(json.dumps(dict(script=str(script), inputs=str(inputs), input_sha256=pins['files'])))


if __name__ == '__main__':
    main()
