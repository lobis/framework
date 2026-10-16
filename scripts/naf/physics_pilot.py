#!/usr/bin/env python3
"""Finite photon transport plus the retained calibrated native response.

Inputs are prepared and checksum-pinned separately. This diagnostic contributes
no production exposure and does not invoke a campaign's custom cosmic sampler.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", required=True)
    args = parser.parse_args()
    inputs = Path(args.inputs).resolve()
    output = Path(os.environ["IAXO_OUTPUT_DIR"]).resolve()
    install = Path(os.environ["REST_PATH"])
    pins = json.loads((inputs / "inputs.json").read_text())
    provenance = output / "provenance"
    provenance.mkdir()
    for name, expected in pins["files"].items():
        path = inputs / name
        if digest(path) != expected:
            raise RuntimeError("Changed pilot input: " + name)
        shutil.copyfile(path, provenance / name)
    shutil.copyfile(inputs / "inputs.json", provenance / "inputs.json")
    env = dict(
        os.environ,
        LEAD_SOURCE_SEED=str(pins["source_seed"]),
        CXX="/usr/bin/g++",
        CLING_CXX_PATH="/usr/bin/g++",
        CLING_STANDARD_PCH="none",
    )
    env.pop("LD_PRELOAD", None)

    def run(command, name, timeout):
        with (output / name).open("w") as log:
            subprocess.run(
                command,
                env=env,
                cwd=output,
                stdout=log,
                stderr=log,
                check=True,
                timeout=timeout,
            )

    run(
        [
            str(install / "bin/restG4"),
            str(inputs / "simulation.rml"),
            "--threads",
            "0",
            "--events",
            str(pins["parents"]),
            "--seed",
            str(pins["source_seed"]),
            "--runNumber",
            "20261016",
            "--output",
            str(output / "transport.root"),
        ],
        "transport.log",
        600,
    )
    # The macro checks generated-parent metadata, seed, saved IDs and energies.
    run(
        [
            str(install / "bin/restRoot"),
            "-b",
            "-q",
            str(inputs / "validate_transport.C"),
        ],
        "metadata.log",
        180,
    )
    call = (
        str(inputs / "ReplayExistingNative.C")
        + "("
        + ",".join(
            json.dumps(str(value))
            for value in (
                output / "transport.root",
                inputs / "native-reference.root",
                output / "native.csv",
            )
        )
    )
    call += "," + str(pins["response_seed"]) + ",-1,1.75)"
    run([str(install / "bin/restRoot"), "-b", "-q", call], "response.log", 1200)
    metadata = json.loads((output / "transport-validation.json").read_text())
    response = json.loads((output / "native.csv.replay.json").read_text())
    if not (
        response["complete"]
        and response["full_input_processed"]
        and response["saved_input_subevents"] == metadata["saved"]
        and response["processed_input_subevents"] == metadata["saved"]
        and response["reconstructed_subevents"] > 0
    ):
        raise RuntimeError(
            "Native response did not process the complete saved photon sample"
        )
    completion = dict(
        complete=True,
        diagnostic_only=True,
        rest_install=str(install),
        transport=metadata,
        response=response,
        input_sha256=pins["files"],
    )
    (output / "completion.json").write_text(json.dumps(completion, indent=2) + "\n")
    print(
        json.dumps(
            dict(
                complete=True,
                generated=metadata["generated"],
                saved=metadata["saved"],
                reconstructed=response["reconstructed_subevents"],
            )
        )
    )


if __name__ == "__main__":
    main()
