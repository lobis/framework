///______________________________________________________________________________
///______________________________________________________________________________
///             
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestSignalAnalysisProcess.cxx
///
///
///             First implementation of Geant4 analysis process into REST_v2
///             Date : apr/2016
///             Author : J. Galan
///
///_______________________________________________________________________________

#include <TPaveText.h>
#include <TLegend.h>

#include "TRestSignalAnalysisProcess.h"
using namespace std;

int counter = 0;

ClassImp(TRestSignalAnalysisProcess)
    //______________________________________________________________________________
TRestSignalAnalysisProcess::TRestSignalAnalysisProcess()
{
    Initialize();
}

//______________________________________________________________________________
TRestSignalAnalysisProcess::TRestSignalAnalysisProcess( char *cfgFileName )
{
    Initialize();

    if( LoadConfigFromFile( cfgFileName ) ) LoadDefaultConfig( );
}

//______________________________________________________________________________
TRestSignalAnalysisProcess::~TRestSignalAnalysisProcess()
{
    delete fSignalEvent;
}

void TRestSignalAnalysisProcess::LoadDefaultConfig()
{
    SetTitle( "Default config" );
}

//______________________________________________________________________________
void TRestSignalAnalysisProcess::Initialize()
{
    SetSectionName( this->ClassName() );

    fSignalEvent = new TRestSignalEvent();

    fOutputEvent = fSignalEvent;
    fInputEvent = fSignalEvent;

    fCutsEnabled = false;
    fFirstEventTime = -1;
    fPreviousEventTime.clear();

    fDrawRefresh = 0;
    fCanvas = NULL;

    time(&timeStored);
}

void TRestSignalAnalysisProcess::LoadConfig( std::string cfgFilename, std::string name )
{
    if( LoadConfigFromFile( cfgFilename, name ) ) LoadDefaultConfig( );
}

//______________________________________________________________________________
void TRestSignalAnalysisProcess::InitProcess()
{
    fSignalAnalysisObservables = TRestEventProcess::ReadObservables();

    fChannelsHisto = new TH1D( "readoutChannelActivity", "readoutChannelActivity", 128, 0, 128 );

    fReadout = (TRestReadout*) GetReadoutMetadata();

    if( GetVerboseLevel() >= REST_Debug && fCanvas == NULL && fDrawRefresh > 0 )
    {
        cout << "Initializing canvas" << endl;
        fCanvas = new TCanvas( this->GetName(), "Signal analysis", 400, 300);
    }
}

//______________________________________________________________________________
void TRestSignalAnalysisProcess::BeginOfEventProcess() 
{
    fSignalEvent->Initialize();

}

//______________________________________________________________________________
TRestEvent* TRestSignalAnalysisProcess::ProcessEvent( TRestEvent *evInput )
{
    TString obsName;

    TRestSignalEvent *fInputSignalEvent = (TRestSignalEvent *) evInput;

    /// Copying the signal event to the output event

    fSignalEvent->SetID( fInputSignalEvent->GetID() );
    fSignalEvent->SetSubID( fInputSignalEvent->GetSubID() );
    fSignalEvent->SetTimeStamp( fInputSignalEvent->GetTimeStamp() );
    fSignalEvent->SetSubEventTag( fInputSignalEvent->GetSubEventTag() );

    for( int sgnl = 0; sgnl < fInputSignalEvent->GetNumberOfSignals(); sgnl++ )
    {
        fSignalEvent->AddSignal( *fInputSignalEvent->GetSignal( sgnl ) );
    }
    /////////////////////////////////////////////////

    if( fFirstEventTime == -1 )
        fFirstEventTime = fInputSignalEvent->GetTime( );

    Double_t secondsFromStart = fInputSignalEvent->GetTime() - fFirstEventTime;
    obsName = this->GetName() + (TString) ".SecondsFromStart";
    fAnalysisTree->SetObservableValue( obsName, secondsFromStart );

    obsName = this->GetName() + (TString) ".HoursFromStart";
    fAnalysisTree->SetObservableValue( obsName, secondsFromStart/3600. );

    Double_t evTimeDelay = 0;
    if( fPreviousEventTime.size() > 0 )
        evTimeDelay = fSignalEvent->GetTime() - fPreviousEventTime.back();
    obsName = this->GetName() + (TString) ".EventTimeDelay";
    fAnalysisTree->SetObservableValue( obsName, evTimeDelay );

    Double_t meanRate = 0;
    if( fPreviousEventTime.size() == 100 )
        meanRate = 100. / (fSignalEvent->GetTime()-fPreviousEventTime.front());

    obsName = this->GetName() + (TString) ".MeanRate_InHz";
    fAnalysisTree->SetObservableValue( obsName, meanRate );

    Double_t baseLineMean = fSignalEvent->GetBaseLineAverage( fBaseLineRange.X(), fBaseLineRange.Y() );

    obsName = this->GetName() + (TString) ".BaseLineMean";
    fAnalysisTree->SetObservableValue( obsName, baseLineMean );

    // Mean base line cut
    if( fCutsEnabled )
        if( baseLineMean > fMeanBaseLineCutRange.Y() || baseLineMean < fMeanBaseLineCutRange.X() ) return NULL; 

    /////////////////////////////////////////////////

    Double_t baseLineSigma = fSignalEvent->GetBaseLineSigmaAverage( fBaseLineRange.X(), fBaseLineRange.Y() );

    obsName = this->GetName() + (TString) ".BaseLineSigmaMean";
    fAnalysisTree->SetObservableValue( obsName, baseLineSigma );

    // Base line noise cut
    if( fCutsEnabled )
        if( baseLineSigma > fMeanBaseLineSigmaCutRange.Y() || baseLineSigma < fMeanBaseLineSigmaCutRange.X() ) return NULL;

    /////////////////////////////////////////////////

    obsName = this->GetName() + (TString) ".TimeBinsLength";
    Double_t timeDelay = fSignalEvent->GetMaxTime() - fSignalEvent->GetMinTime();
    fAnalysisTree->SetObservableValue( obsName, timeDelay );

    /////////////////////////////////////////////////

    obsName = this->GetName() + (TString) ".NumberOfSignals";
    Double_t nSignals = fSignalEvent->GetNumberOfSignals();
    fAnalysisTree->SetObservableValue( obsName, nSignals );

    // Signals are zero-ed
    fSignalEvent->SubstractBaselines( fBaseLineRange.X(), fBaseLineRange.Y() );

    Int_t from = (Int_t) fIntegralRange.X();
    Int_t to = (Int_t) fIntegralRange.Y();
    Double_t fullIntegral = fSignalEvent->GetIntegral( from, to );
    obsName = this->GetName() + (TString) ".FullIntegral";
    fAnalysisTree->SetObservableValue( obsName, fullIntegral );


    Double_t thrIntegral = fSignalEvent->GetIntegralWithThreshold( from, to, fBaseLineRange.X(), fBaseLineRange.Y(), fPointThreshold, fNPointsOverThreshold, fSignalThreshold );
    obsName = this->GetName() + (TString) ".ThresholdIntegral";
    fAnalysisTree->SetObservableValue( obsName, thrIntegral );

    Double_t integralRatio = (fullIntegral-thrIntegral)/(fullIntegral+thrIntegral);
    obsName = this->GetName() + (TString) ".IntegralBalance";
    fAnalysisTree->SetObservableValue( obsName, integralRatio );

    Double_t maxValue = 0;
    Double_t maxValueIntegral = 0;

    Double_t minPeakTime = 1000; // TODO sustitute this for something better
    Double_t maxPeakTime = 0;
    Double_t peakTimeAverage = 0;

    Int_t nGoodSignals = 0;
    for( int s = 0; s < fSignalEvent->GetNumberOfSignals(); s++ )
    {
        TRestSignal *sgnl = fSignalEvent->GetSignal( s );
        if( sgnl->GetIntegralWithThreshold( from, to, fBaseLineRange.X(), fBaseLineRange.Y(), fPointThreshold, fNPointsOverThreshold, fSignalThreshold ) > 0 )
        {
            Double_t value = fSignalEvent->GetSignal(s)->GetMaxValue();
            if( value > maxValue ) maxValue = value;
            maxValueIntegral += value;

            Double_t peakTime = sgnl->GetMaxPeakTime();
            peakTimeAverage += peakTime;

            if( minPeakTime > peakTime ) minPeakTime = peakTime;
            if( maxPeakTime < peakTime ) maxPeakTime = peakTime;

            nGoodSignals++;
        }
    }

    Double_t ampIntRatio = thrIntegral/maxValueIntegral;
    obsName = this->GetName() + (TString) ".AmplitudeIntegralRatio";
    fAnalysisTree->SetObservableValue( obsName, ampIntRatio );

    if( nGoodSignals > 0 ) peakTimeAverage /= nGoodSignals;

    obsName = this->GetName() + (TString) ".NumberOfGoodSignals";
    fAnalysisTree->SetObservableValue( obsName, nGoodSignals );

    obsName = this->GetName() + (TString) ".MaxPeakAmplitude";
    fAnalysisTree->SetObservableValue( obsName, maxValue );

    obsName = this->GetName() + (TString) ".PeakAmplitudeIntegral";
    fAnalysisTree->SetObservableValue( obsName, maxValueIntegral );

    Double_t amplitudeRatio = maxValueIntegral/maxValue;
    obsName = this->GetName() + (TString) ".AmplitudeRatio";
    fAnalysisTree->SetObservableValue( obsName, amplitudeRatio );

    obsName = this->GetName() + (TString) ".MaxPeakTime";
    fAnalysisTree->SetObservableValue( obsName, maxPeakTime );

    obsName = this->GetName() + (TString) ".MinPeakTime";
    fAnalysisTree->SetObservableValue( obsName, minPeakTime );

    Double_t peakTimeDelay = maxPeakTime - minPeakTime;
    obsName = this->GetName() + (TString) ".MaxPeakTimeDelay";
    fAnalysisTree->SetObservableValue( obsName, peakTimeDelay );

    obsName = this->GetName() + (TString) ".AveragePeakTime";
    fAnalysisTree->SetObservableValue( obsName, peakTimeAverage );


    // Cuts
    if( fCutsEnabled )
    {
        if( nSignals < fMaxNumberOfSignalsCut.X() || nSignals > fMaxNumberOfSignalsCut.Y() )  return NULL;
        if( nGoodSignals < fMaxNumberOfGoodSignalsCut.X() || nGoodSignals > fMaxNumberOfGoodSignalsCut.Y() ) return NULL;
        if( thrIntegral < fThresholdIntegralCut.X() || thrIntegral > fThresholdIntegralCut.Y() ) return NULL;
        if( fullIntegral < fFullIntegralCut.X() || fullIntegral > fFullIntegralCut.Y() ) return NULL;
        if( peakTimeDelay < fPeakTimeDelayCut.X() || peakTimeDelay > fPeakTimeDelayCut.Y() ) return NULL;
    }

    if( fReadout != NULL )
    {
        TRestReadoutModule *mod = fReadout->GetReadoutPlane(0)->GetModule(0);
        for( int s = 0; s < fSignalEvent->GetNumberOfSignals(); s++ )
        {
            Int_t readoutChannel = mod->DaqToReadoutChannel( fSignalEvent->GetSignal(s)->GetID() );
            fChannelsHisto->Fill( readoutChannel );
        }
    }
    else
    {
        if( GetVerboseLevel() >= REST_Warning )
            cout << "TRestSignalAnalysisProcess. Readout not defined!" << endl;
	}


    if( GetVerboseLevel() >= REST_Debug && fDrawRefresh > 0 )
    {
        counter++;
        if( counter > fDrawRefresh )
        {
            counter = 0;
            for( unsigned int i = 0; i < fDrawingObjects.size(); i++ )
                delete fDrawingObjects[i];
            fDrawingObjects.clear();

            TPad *pad2 = DrawSignal(0);

            fCanvas->cd(); 
            pad2->Draw();

            /*
            fCanvas->cd(4); 
            txt->Draw();
            */

            fCanvas->Update();
            if( GetVerboseLevel() >= REST_Debug ) 
            {
                fAnalysisTree->PrintObservables();
                GetChar(); 
            }
        }
    }


    return fSignalEvent;
}

//______________________________________________________________________________
void TRestSignalAnalysisProcess::EndOfEventProcess() 
{
    fPreviousEventTime.push_back( fSignalEvent->GetTimeStamp() );
    if( fPreviousEventTime.size() > 100 ) fPreviousEventTime.erase( fPreviousEventTime.begin() );
}

//______________________________________________________________________________
void TRestSignalAnalysisProcess::EndProcess()
{
    // Function to be executed once at the end of the process 
    // (after all events have been processed)

    //Start by calling the EndProcess function of the abstract class. 
    //Comment this if you don't want it.
    //TRestEventProcess::EndProcess();
    fChannelsHisto->Write();

    if( fCanvas != NULL )
        delete fCanvas;
}

TPad *TRestSignalAnalysisProcess::DrawObservables( )
{
    TPad *pad = new TPad( "Signal", " ", 0, 0, 1, 1 );
    //fDrawingObjects.push_back( (TObject *) pad );
    pad->cd();

    TPaveText *txt = new TPaveText( .05, .1, .95, .8 );
 //   fDrawingObjects.push_back( (TObject *) txt );

    txt->AddText(" ");
    for( unsigned int i = 0; i < fSignalAnalysisObservables.size(); i++ )
    {
        Int_t id = fAnalysisTree->GetObservableID( this->GetName() + (TString) "." + fSignalAnalysisObservables[i] );
        TString valueStr;
        valueStr.Form( " : %lf", fAnalysisTree->GetObservableValue( id ) );
        TString sentence = (TString) fSignalAnalysisObservables[i] + valueStr; 
        txt->AddText( sentence );
    }
    txt->AddText(" ");

    txt->Draw();

    return pad;
}

TPad *TRestSignalAnalysisProcess::DrawSignal( Int_t signal )
{
    TPad *pad = new TPad( "Signal", " ", 0, 0, 1, 1 );
    pad->cd();

    fDrawingObjects.push_back( (TObject *) pad );

    TGraph *gr = new TGraph();
    fDrawingObjects.push_back( (TObject *) gr );

    TRestSignal *sgnl = fSignalEvent->GetSignal( signal );

    for( int n = 0; n < sgnl->GetNumberOfPoints(); n++ )
        gr->SetPoint( n, sgnl->GetTime(n), sgnl->GetData(n) );

    gr->Draw();

    TGraph *gr2 = new TGraph();
    fDrawingObjects.push_back( (TObject *) gr2 );

    gr2->SetLineWidth( 2 );
    gr2->SetLineColor( 2 );

    for( int n = fBaseLineRange.X(); n < fBaseLineRange.Y(); n++ )
        gr2->SetPoint( n - fBaseLineRange.X(), sgnl->GetTime(n), sgnl->GetData(n) );

    gr2->Draw("same");

    vector <Int_t> pOver = sgnl->GetPointsOverThreshold( );

    TGraph *gr3[5];
    Int_t nGraphs = 0;
    gr3[nGraphs] = new TGraph();
    fDrawingObjects.push_back( (TObject *) gr3[nGraphs] );
    gr3[nGraphs]->SetLineWidth( 2 );
    gr3[nGraphs]->SetLineColor( 3 );
    Int_t point = 0;
    Int_t nPoints = pOver.size();
    for( int n = 0; n < nPoints-1; n++ )
    {
        gr3[nGraphs]->SetPoint( point, sgnl->GetTime(pOver[n]), sgnl->GetData(pOver[n]) );
        point++;
        if( pOver[n+1] - pOver[n] > 1 )
        {
            gr3[nGraphs]->Draw("same");
            nGraphs++;
            if( nGraphs > 4 ) cout << "Ngraphs : " << nGraphs << endl;
            point = 0;
            gr3[nGraphs] = new TGraph();
            fDrawingObjects.push_back( (TObject *) gr3[nGraphs] );
            gr3[nGraphs]->SetLineWidth( 2 );
            gr3[nGraphs]->SetLineColor( 3 );
        }
    }
    if( nPoints > 0 )
        gr3[nGraphs]->Draw("same");

    /*
    TLegend *leg = new TLegend(.6,.7,.9,.9);
    fDrawingObjects.push_back( (TObject *) leg );
    leg->AddEntry( gr2, (TString) "Baseline" );
    leg->Draw("same");
    */

    return pad;
}

//______________________________________________________________________________
void TRestSignalAnalysisProcess::InitFromConfigFile( )
{
    fDrawRefresh = StringToDouble( GetParameter( "refreshEvery", "0" ) );

    fBaseLineRange = StringTo2DVector( GetParameter( "baseLineRange", "(5,55)") );
    fIntegralRange = StringTo2DVector( GetParameter( "integralRange", "(10,500)") );
    fPointThreshold = StringToDouble( GetParameter( "pointThreshold", 2 ) );
    fNPointsOverThreshold = StringToInteger( GetParameter( "pointsOverThreshold", 5 ) );
    fSignalThreshold = StringToDouble( GetParameter( "signalThreshold", 5 ) );

    fMeanBaseLineCutRange = StringTo2DVector( GetParameter( "meanBaseLineCutRange", "(0,4096)") );
    fMeanBaseLineSigmaCutRange = StringTo2DVector( GetParameter( "meanBaseLineSigmaCutRange", "(0,4096)") );
    fMaxNumberOfSignalsCut = StringTo2DVector( GetParameter( "maxNumberOfSignalsCut", "(0,20)" ) );
    fMaxNumberOfGoodSignalsCut = StringTo2DVector( GetParameter( "maxNumberOfGoodSignalsCut", "(0,20)" ) );

    fFullIntegralCut  = StringTo2DVector( GetParameter( "fullIntegralCut", "(0,100000)" ) );
    fThresholdIntegralCut  = StringTo2DVector( GetParameter( "thresholdIntegralCut", "(0,100000)" ) );

    fPeakTimeDelayCut  = StringTo2DVector( GetParameter( "peakTimeDelayCut", "(0,20)" ) );
    if( GetParameter( "cutsEnabled", "false" ) == "true" ) fCutsEnabled = true;


}

