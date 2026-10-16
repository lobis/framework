#include <TRestGeant4Event.h>
#include <TRestGeant4Metadata.h>
#include <TRestRun.h>

#include <cmath>
#include <fstream>
#include <set>

void validate_transport() {
    const std::string output = gSystem->Getenv("IAXO_OUTPUT_DIR");
    TRestRun run((output + "/transport.root").c_str());
    auto* metadata = dynamic_cast<TRestGeant4Metadata*>(run.GetMetadataClass("TRestGeant4Metadata"));
    if (!metadata || metadata->GetNumberOfEvents() != 200 || metadata->GetSeed() != 2026101601 ||
        run.GetEntries() <= 0 || run.GetEntries() > 200)
        gSystem->Exit(80);
    std::set<std::pair<int, int>> identifiers;
    for (Long64_t index = 0; index < run.GetEntries(); ++index) {
        run.GetEntry(index);
        auto* event = dynamic_cast<TRestGeant4Event*>(run.GetInputEvent());
        if (!event || event->GetID() < 0 || event->GetID() >= 200 ||
            !identifiers.emplace(event->GetID(), event->GetSubID()).second ||
            !std::isfinite(event->GetTotalDepositedEnergy()) || event->GetTotalDepositedEnergy() <= 0)
            gSystem->Exit(81);
    }
    std::ofstream receipt(output + "/transport-validation.json");
    receipt << "{\"generated\":200,\"seed\":2026101601,\"saved\":" << run.GetEntries() << "}\n";
    receipt.close();
    if (!receipt) gSystem->Exit(82);
}
