/*************************************************************************
 * This file is part of the REST software framework.                     *
 *                                                                       *
 * Copyright (C) 2016 GIFNA/TREX (University of Zaragoza)                *
 * For more information see http://gifna.unizar.es/trex                  *
 *                                                                       *
 * REST is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * REST is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have a copy of the GNU General Public License along with   *
 * REST in $REST_PATH/LICENSE.                                           *
 * If not, see http://www.gnu.org/licenses/.                             *
 * For the list of contributors see $REST_PATH/CREDITS.                  *
 *************************************************************************/

//////////////////////////////////////////////////////////////////////////
/// The TRestRawSignalChannelActivityProcess allows to generate different
/// histograms in order to monitor the times a channel has observed a signal
/// under certain threshold and number of active channels conditions.
///
/// TODO. Write a more detailed documentation here
///
///--------------------------------------------------------------------------
///
/// RESTsoft - Software for Rare Event Searches with TPCs
///
/// History of developments:
///
/// 2020-August: First implementation of raw signal channel activity process.
///              Cristina Margalejo
///
/// \class      TRestRawSignalChannelActivityProcess
/// \author     Cristina Margalejo
///
/// <hr>
///
#include "TRestRawSignalChannelActivityProcess.h"
using namespace std;

ClassImp(TRestRawSignalChannelActivityProcess);

///////////////////////////////////////////////
/// \brief Default constructor
///
TRestRawSignalChannelActivityProcess::TRestRawSignalChannelActivityProcess() { Initialize(); }

///////////////////////////////////////////////
/// \brief Constructor loading data from a config file
///
/// If no configuration path is defined using TRestMetadata::SetConfigFilePath
/// the path to the config file must be specified using full path, absolute or
/// relative.
///
/// The default behaviour is that the config file must be specified with
/// full path, absolute or relative.
///
/// \param cfgFileName A const char* giving the path to an RML file.
///
TRestRawSignalChannelActivityProcess::TRestRawSignalChannelActivityProcess(char* cfgFileName) {
    Initialize();

    if (LoadConfigFromFile(cfgFileName)) LoadDefaultConfig();
}

///////////////////////////////////////////////
/// \brief Default destructor
///
TRestRawSignalChannelActivityProcess::~TRestRawSignalChannelActivityProcess() { delete fSignalEvent; }

///////////////////////////////////////////////
/// \brief Function to load the default config in absence of RML input
///
void TRestRawSignalChannelActivityProcess::LoadDefaultConfig() { SetTitle("Default config"); }

///////////////////////////////////////////////
/// \brief Function to initialize input/output event members and define the
/// section name
///
void TRestRawSignalChannelActivityProcess::Initialize() {
    SetSectionName(this->ClassName());

    fSignalEvent = new TRestRawSignalEvent();
}

///////////////////////////////////////////////
/// \brief Function to load the configuration from an external configuration
/// file.
///
/// If no configuration path is defined in TRestMetadata::SetConfigFilePath
/// the path to the config file must be specified using full path, absolute or
/// relative.
///
/// \param cfgFileName A const char* giving the path to an RML file.
/// \param name The name of the specific metadata. It will be used to find the
/// correspondig TRestGeant4AnalysisProcess section inside the RML.
///
void TRestRawSignalChannelActivityProcess::LoadConfig(std::string cfgFilename, std::string name) {
    if (LoadConfigFromFile(cfgFilename, name)) LoadDefaultConfig();
}

///////////////////////////////////////////////
/// \brief Process initialization. The ROOT TH1 histograms are created here using
/// the limits defined in the process metadata members.
///
/// The readout histograms will only be created in case an appropiate readout definition
/// is found in the processing chain.
///
void TRestRawSignalChannelActivityProcess::InitProcess() {
    fReadout = GetMetadata<TRestReadout>();

    debug << "TRestRawSignalChannelActivityProcess::InitProcess. Readout pointer : " << fReadout << endl;
    if (GetVerboseLevel() >= REST_Info && fReadout) fReadout->PrintMetadata();

    if (!fReadOnly) {
        fDaqChannelsHisto = new TH1D("daqChannelActivityRaw", "daqChannelActivity", fDaqHistogramChannels,
                                     fDaqStartChannel, fDaqEndChannel);
        if (fReadout) {
            fReadoutChannelsHisto_OneSignal =
                new TH1D("rChannelActivityRaw_1", "readoutChannelActivity", fReadoutHistogramChannels, 0,
                         fReadoutHistogramChannels);
            fReadoutChannelsHisto_OneSignal_High =
                new TH1D("rChannelActivityRaw_1H", "readoutChannelActivity", fReadoutHistogramChannels, 0,
                         fReadoutHistogramChannels);
            fReadoutChannelsHisto_TwoSignals =
                new TH1D("rChannelActivityRaw_2", "readoutChannelActivity", fReadoutHistogramChannels, 0,
                         fReadoutHistogramChannels);
            fReadoutChannelsHisto_TwoSignals_High =
                new TH1D("rChannelActivityRaw_2H", "readoutChannelActivity", fReadoutHistogramChannels, 0,
                         fReadoutHistogramChannels);
            fReadoutChannelsHisto_ThreeSignals =
                new TH1D("rChannelActivityRaw_3", "readoutChannelActivity", fReadoutHistogramChannels, 0,
                         fReadoutHistogramChannels);
            fReadoutChannelsHisto_ThreeSignals_High =
                new TH1D("rChannelActivityRaw_3H", "readoutChannelActivity", fReadoutHistogramChannels, 0,
                         fReadoutHistogramChannels);
            fReadoutChannelsHisto_MultiSignals =
                new TH1D("rChannelActivityRaw_M", "readoutChannelActivity", fReadoutHistogramChannels, 0,
                         fReadoutHistogramChannels);
            fReadoutChannelsHisto_MultiSignals_High =
                new TH1D("rChannelActivityRaw_MH", "readoutChannelActivity", fReadoutHistogramChannels, 0,
                         fReadoutHistogramChannels);
        }
    }
}

///////////////////////////////////////////////
/// \brief The main processing event function
///
TRestEvent* TRestRawSignalChannelActivityProcess::ProcessEvent(TRestEvent* evInput) {
    TString obsName;

    TRestRawSignalEvent* fInputSignalEvent = (TRestRawSignalEvent*)evInput;

    /// Copying the signal event to the output event

    fSignalEvent->SetID(fInputSignalEvent->GetID());
    fSignalEvent->SetSubID(fInputSignalEvent->GetSubID());
    fSignalEvent->SetTimeStamp(fInputSignalEvent->GetTimeStamp());
    fSignalEvent->SetSubEventTag(fInputSignalEvent->GetSubEventTag());

    Int_t N = fInputSignalEvent->GetNumberOfSignals();
    for (int sgnl = 0; sgnl < N; sgnl++) fSignalEvent->AddSignal(*fInputSignalEvent->GetSignal(sgnl));
    /////////////////////////////////////////////////

    Int_t Nlow = 0;
    Int_t Nhigh = 0;
    for (int s = 0; s < fSignalEvent->GetNumberOfSignals(); s++) {
        TRestRawSignal* sgnl = fSignalEvent->GetSignal(s);
        if (sgnl->GetMaxValue() > fHighThreshold) Nhigh++;
        if (sgnl->GetMaxValue() > fLowThreshold) Nlow++;
    }

    for (int s = 0; s < fSignalEvent->GetNumberOfSignals(); s++) {
        TRestRawSignal* sgnl = fSignalEvent->GetSignal(s);
        // Adding signal to the channel activity histogram
        if (!fReadOnly && fReadout) {
            Int_t signalID = fSignalEvent->GetSignal(s)->GetID();

            Int_t p, m, readoutChannel;
            fReadout->GetPlaneModuleChannel(signalID, p, m, readoutChannel);

            if (sgnl->GetMaxValue() > fLowThreshold) {
                if (Nlow == 1) fReadoutChannelsHisto_OneSignal->Fill(readoutChannel);
                if (Nlow == 2) fReadoutChannelsHisto_TwoSignals->Fill(readoutChannel);
                if (Nlow == 3) fReadoutChannelsHisto_ThreeSignals->Fill(readoutChannel);
                if (Nlow > 3 && Nlow < 10) fReadoutChannelsHisto_MultiSignals->Fill(readoutChannel);
            }

            if (sgnl->GetMaxValue() > fHighThreshold) {
                if (Nhigh == 1) fReadoutChannelsHisto_OneSignal_High->Fill(readoutChannel);
                if (Nhigh == 2) fReadoutChannelsHisto_TwoSignals_High->Fill(readoutChannel);
                if (Nhigh == 3) fReadoutChannelsHisto_ThreeSignals_High->Fill(readoutChannel);
                if (Nhigh > 3 && Nhigh < 10) fReadoutChannelsHisto_MultiSignals_High->Fill(readoutChannel);
            }
        }

        if (!fReadOnly) {
            Int_t daqChannel = fSignalEvent->GetSignal(s)->GetID();
            fDaqChannelsHisto->Fill(daqChannel);
        }
    }

    if (GetVerboseLevel() >= REST_Debug) fAnalysisTree->PrintObservables();

    return fSignalEvent;
}

///////////////////////////////////////////////
/// \brief Function to include required actions after all events have been
/// processed. In this process it will take care of writting the histograms
/// to disk.
///
void TRestRawSignalChannelActivityProcess::EndProcess() {
    if (!fReadOnly) {
        fDaqChannelsHisto->Write();
        if (fReadout) {
            fReadoutChannelsHisto_OneSignal->Write();
            fReadoutChannelsHisto_TwoSignals->Write();
            fReadoutChannelsHisto_ThreeSignals->Write();
            fReadoutChannelsHisto_MultiSignals->Write();

            fReadoutChannelsHisto_OneSignal_High->Write();
            fReadoutChannelsHisto_TwoSignals_High->Write();
            fReadoutChannelsHisto_ThreeSignals_High->Write();
            fReadoutChannelsHisto_MultiSignals_High->Write();
        }
    }
}

///////////////////////////////////////////////
/// \brief Function to read input parameters from the RML
/// TRestRawSignalChannelActivityProcess metadata section
///
void TRestRawSignalChannelActivityProcess::InitFromConfigFile() {
    fLowThreshold = StringToDouble(GetParameter("lowThreshold", "25"));
    fHighThreshold = StringToDouble(GetParameter("highThreshold", "50"));

    fDaqHistogramChannels = StringToInteger(GetParameter("daqChannels", "300"));
    fDaqStartChannel = StringToInteger(GetParameter("daqStartCh", "4320"));
    fDaqEndChannel = StringToInteger(GetParameter("daqEndCh", "4620"));
    fReadoutHistogramChannels = StringToInteger(GetParameter("readoutChannels", "128"));
}
