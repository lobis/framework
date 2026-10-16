#!/usr/bin/env python3
"""Preserve validated NAF gas tables inside a new, unactivated installation."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("install")
    args = parser.parse_args()
    install = Path(args.install).resolve()
    if (install / "validated.json").exists():
        raise RuntimeError("Refuse modification of a validated installation")
    catalog = install / "data/dataURL"
    if not catalog.is_file():
        raise RuntimeError("REST installation is incomplete")
    source = Path("/data/dust/group/iaxo/software/lobis/gasFiles")
    destination = install / "data/naf-gas"
    destination.mkdir(exist_ok=False)
    original = digest(catalog)
    pins = {}
    for path in sorted([source / "gases.rml", *source.glob("*.gas")]):
        before = digest(path)
        target = destination / path.name
        shutil.copyfile(path, target)
        if digest(path) != before or digest(target) != before:
            raise RuntimeError("Gas resource changed while copying: " + str(path))
        pins[path.name] = before
    rows = []
    for line in catalog.read_text().splitlines():
        if "\ttype=GAS_SERVER\t" in line:
            line = line.split("\tvalue=")[0] + "\tvalue=" + str(destination) + "/"
        elif "\ttype=META_RML\t" in line and "\ttag=TRestDetectorGas\t" in line:
            line = (
                line.split("\tvalue=")[0] + "\tvalue=" + str(destination / "gases.rml")
            )
        rows.append(line)
    catalog.write_text("\n".join(rows) + "\n")
    manifest = dict(
        source=str(source),
        source_catalog_sha256=original,
        catalog_sha256=digest(catalog),
        files=pins,
    )
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest))


if __name__ == "__main__":
    main()
