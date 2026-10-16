# Versioned REST for NAF

This fork incorporates upstream framework master and pins the current component
masters in its Git submodules. Update explicitly and test a new release before
changing the batch default. Do not update submodules with `--remote` during a
build or mutate a prefix used by prepared campaigns.

`build_release.sh /absolute/new/release` builds the complete ROOT/Geant4/Garfield
stack into `release/install`. Stage GoogleTest 1.11 headers under
`release/system-gtest/include/gtest` and its two shared libraries under
`release/system-gtest/lib` before submitting the build; login-host packages are
not guaranteed on compute nodes. The script rejects rebuilding a validated
release. Framework CTest checks cover the setup script, ROOT expressions,
parser ownership and unique workspaces, fatal error exits, and concurrent
run-number allocation.

The latest detector library already includes the channel-specific smearing
correction previously carried as a local NAF patch. Do not reapply it.

Before activation, preserve the validated local gas definitions and tables
inside the new installation and point its dataURL gas entries at that snapshot.
Run `prepare_physics_pilot.py /absolute/release` and prepare its `pilot.sh`
with the explicit new `--rest-install`. This pilot transports 200 photons at
5.9 keV inside the retained detector geometry and checks seed, parent metadata,
saved identities, energies, and complete calibrated native reconstruction.
Require independently verified CERNBox files and a final manifest before
writing `install/validated.json` or changing the default.

`prepare_cernbox.sh` selects the validated installation recorded in
`/afs/desy.de/user/l/lobis/.config/iaxo-cernbox/rest-default.json` and invokes
the shared helper. `IAXO_REST_INSTALL` can select another validated prefix.
Each new preparation hashes the installed files and resolves the installation
path. A worker refuses to start physics if the files change. Existing prepared
campaigns retain their original helper, installation and source streams.

The bundled helper and its tests are mirrored from the thesis
`code/storage/cernbox-batch` implementation. It provides private local HOME,
REST/GDML/download/dictionary caches, ROOT/GSL/data preflight, bounded resources,
a whole-upload network deadline, verified receipt-before-delete uploads, and
upload-only recovery after completed transport. It never automatically reruns
an incomplete simulation.

Use explicit campaign/job run IDs and distinct transport/response seeds. Match
the actual transport threads to requested CPUs. Measure transport, reconstruction
and transfer separately; a transport timer does not reserve enough scheduler
time for the full job. Keep current aggregate quota governors, failure recovery,
scientific closure checks, and the generic eight-job materialization cap.
Preparing a campaign does not reserve storage or authorize its submission.

Campaigns with frozen custom cosmic samplers and physical-parent observers need
their own migration pilot. This release's photon diagnostic does not validate
their source distributions or authorize replacing their runtime in place.
