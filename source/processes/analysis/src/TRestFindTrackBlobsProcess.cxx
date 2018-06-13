///______________________________________________________________________________
///______________________________________________________________________________
///             
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestFindTrackBlobsProcess.cxx
///
///
///             First implementation of Geant4 analysis process into REST_v2
///             Date : mar/2016
///             Author : J. Galan
///
///_______________________________________________________________________________


#include "TRestFindTrackBlobsProcess.h"
using namespace std;


ClassImp(TRestFindTrackBlobsProcess)
    //______________________________________________________________________________
TRestFindTrackBlobsProcess::TRestFindTrackBlobsProcess()
{
    Initialize();
}

//______________________________________________________________________________
TRestFindTrackBlobsProcess::TRestFindTrackBlobsProcess( char *cfgFileName )
{
    Initialize();

    if( LoadConfigFromFile( cfgFileName ) ) LoadDefaultConfig( );
}

//______________________________________________________________________________
TRestFindTrackBlobsProcess::~TRestFindTrackBlobsProcess()
{
    delete fInputTrackEvent;
    delete fOutputTrackEvent;
}

void TRestFindTrackBlobsProcess::LoadDefaultConfig()
{
    SetTitle( "Default config" );
}

//______________________________________________________________________________
void TRestFindTrackBlobsProcess::Initialize()
{
    SetSectionName( this->ClassName() );

    fInputTrackEvent = new TRestTrackEvent();
    fOutputTrackEvent = new TRestTrackEvent();

    fOutputEvent = fOutputTrackEvent;
    fInputEvent  = fInputTrackEvent;

    fHitsToCheckFraction = 0.2;
}

void TRestFindTrackBlobsProcess::LoadConfig( std::string cfgFilename, std::string name )
{
    if( LoadConfigFromFile( cfgFilename, name ) ) LoadDefaultConfig( );
}

//______________________________________________________________________________
void TRestFindTrackBlobsProcess::InitProcess()
{
    std::vector <string> fObservables;
    fObservables = TRestEventProcess::ReadObservables();

    for( unsigned int i = 0; i < fObservables.size(); i++ )
    {
        if( fObservables[i].find( "Q1_R") != string::npos )
            fQ1_Observables.push_back( fObservables[i] );
        if( fObservables[i].find( "Q2_R") != string::npos )
            fQ2_Observables.push_back( fObservables[i] );

        if( fObservables[i].find( "Q1_X_R") != string::npos )
            fQ1_X_Observables.push_back( fObservables[i] );
        if( fObservables[i].find( "Q2_X_R") != string::npos )
            fQ2_X_Observables.push_back( fObservables[i] );

        if( fObservables[i].find( "Q1_Y_R") != string::npos )
            fQ1_Y_Observables.push_back( fObservables[i] );
        if( fObservables[i].find( "Q2_Y_R") != string::npos )
            fQ2_Y_Observables.push_back( fObservables[i] );
    }

    for( unsigned int i = 0; i < fQ1_Observables.size(); i++ )
    {
        Double_t r1 = atof ( fQ1_Observables[i].substr( 4, fQ1_Observables[i].length() - 4 ).c_str() );
        fQ1_Radius.push_back( r1 );
    }

    for( unsigned int i = 0; i < fQ2_Observables.size(); i++ )
    {
        Double_t r2 = atof ( fQ2_Observables[i].substr( 4, fQ2_Observables[i].length() - 4 ).c_str() );
        fQ2_Radius.push_back( r2 );
    }

    for( unsigned int i = 0; i < fQ1_X_Observables.size(); i++ )
    {
        Double_t r1 = atof ( fQ1_X_Observables[i].substr( 6, fQ1_X_Observables[i].length() - 6 ).c_str() );
        fQ1_X_Radius.push_back( r1 );
    }

    for( unsigned int i = 0; i < fQ2_X_Observables.size(); i++ )
    {
        Double_t r2 = atof ( fQ2_X_Observables[i].substr( 6, fQ2_X_Observables[i].length() - 6 ).c_str() );
        fQ2_X_Radius.push_back( r2 );
    }

    for( unsigned int i = 0; i < fQ1_Y_Observables.size(); i++ )
    {
        Double_t r1 = atof ( fQ1_Y_Observables[i].substr( 6, fQ1_Y_Observables[i].length() - 6 ).c_str() );
        fQ1_Y_Radius.push_back( r1 );
    }

    for( unsigned int i = 0; i < fQ2_Y_Observables.size(); i++ )
    {
        Double_t r2 = atof ( fQ2_Y_Observables[i].substr( 6, fQ2_Y_Observables[i].length() - 6 ).c_str() );
        fQ2_Y_Radius.push_back( r2 );
    }
}

//______________________________________________________________________________
void TRestFindTrackBlobsProcess::BeginOfEventProcess() 
{
    fOutputTrackEvent->Initialize();
}

//______________________________________________________________________________
TRestEvent* TRestFindTrackBlobsProcess::ProcessEvent( TRestEvent *evInput )
{
    fInputTrackEvent = (TRestTrackEvent *) evInput;

    // Copying the input tracks to the output track
    for( int tck = 0; tck < fInputTrackEvent->GetNumberOfTracks(); tck++ )
        fOutputTrackEvent->AddTrack( fInputTrackEvent->GetTrack(tck) ); 


    vector <TRestTrack *> tracks;

    TRestTrack *tXYZ = fInputTrackEvent->GetMaxEnergyTrack( );
    if( tXYZ )
        tracks.push_back( tXYZ );

    TRestTrack *tX = fInputTrackEvent->GetMaxEnergyTrack("X");
    if( tX )
        tracks.push_back( tX );

    TRestTrack *tY = fInputTrackEvent->GetMaxEnergyTrack("Y");
    if( tY )
        tracks.push_back( tY );
    
    Double_t x1 = 0, y1 = 0, z1 = 0;
    Double_t x2 = 0, y2 = 0, z2 = 0;

    Double_t x1_X = 0, x2_X = 0;
    Double_t z1_X = 0, z2_X = 0;

    Double_t y1_Y = 0, y2_Y = 0;
    Double_t z1_Y = 0, z2_Y = 0;

    for( unsigned int t = 0; t < tracks.size(); t++ )
    {
        TRestHits *hits = (TRestHits *) tracks[t]->GetVolumeHits();

        Int_t nHits = hits->GetNumberOfHits();

        Int_t nCheck = (Int_t) (nHits * fHitsToCheckFraction);

        Int_t hit1 = hits->GetMostEnergeticHitInRange( 0, nCheck );
        Int_t hit2 = hits->GetMostEnergeticHitInRange( nHits - nCheck, nHits );

        if( tracks[t]->isXYZ() )
        {
            // The blob with z-coordinate closer to z=0 is stored in x1,y1,z1 
            if( fabs( hits->GetZ( hit1 ) ) < fabs( hits->GetZ( hit2 ) ) ) 
            {
                x1 = hits->GetX( hit1 );
                y1 = hits->GetY( hit1 );
                z1 = hits->GetZ( hit1 );

                x2 = hits->GetX( hit2 );
                y2 = hits->GetY( hit2 );
                z2 = hits->GetZ( hit2 );
            }
            else
            {
                x2 = hits->GetX( hit1 );
                y2 = hits->GetY( hit1 );
                z2 = hits->GetZ( hit1 );

                x1 = hits->GetX( hit2 );
                y1 = hits->GetY( hit2 );
                z1 = hits->GetZ( hit2 );
            }
        }

        if( tracks[t]->isXZ() )
        {
            // The blob with z-coordinate closer to z=0 is stored in x1,y1,z1 
            if( fabs( hits->GetZ( hit1 ) ) < fabs( hits->GetZ( hit2 ) ) ) 
            {
                x1_X = hits->GetX( hit1 );
                z1_X = hits->GetZ( hit1 );

                x2_X = hits->GetX( hit2 );
                z2_X = hits->GetZ( hit2 );
            }
            else
            {
                x2_X = hits->GetX( hit1 );
                z2_X = hits->GetZ( hit1 );

                x1_X = hits->GetX( hit2 );
                z1_X = hits->GetZ( hit2 );
            }
        }

        if( tracks[t]->isYZ() )
        {
            // The blob with z-coordinate closer to z=0 is stored in x1,y1,z1 
            if( fabs( hits->GetZ( hit1 ) ) < fabs( hits->GetZ( hit2 ) ) ) 
            {
                y1_Y = hits->GetY( hit1 );
                z1_Y = hits->GetZ( hit1 );

                y2_Y = hits->GetY( hit2 );
                z2_Y = hits->GetZ( hit2 );
            }
            else
            {
                y2_Y = hits->GetY( hit1 );
                z2_Y = hits->GetZ( hit1 );

                y1_Y = hits->GetY( hit2 );
                z1_Y = hits->GetZ( hit2 );
            }
        }
    }

    TString obsName;

    obsName = this->GetName() + (TString) ".x1";
    fAnalysisTree->SetObservableValue( obsName, x1 );

    obsName = this->GetName() + (TString) ".y1";
    fAnalysisTree->SetObservableValue( obsName, y1 );

    obsName = this->GetName() + (TString) ".z1";
    fAnalysisTree->SetObservableValue( obsName, z1 );

    /////

    obsName = this->GetName() + (TString) ".x2";
    fAnalysisTree->SetObservableValue( obsName, x2 );

    obsName = this->GetName() + (TString) ".y2";
    fAnalysisTree->SetObservableValue( obsName, y2 );

    obsName = this->GetName() + (TString) ".z2";
    fAnalysisTree->SetObservableValue( obsName, z2 );

    Double_t dist = (x1-x2) * (x1-x2) + (y1-y2) * (y1-y2) + (z1-z2) * (z1-z2);
    dist = TMath::Sqrt( dist );

    obsName = this->GetName() + (TString) ".distance";
    fAnalysisTree->SetObservableValue( obsName, dist );

    /////

    obsName = this->GetName() + (TString) ".x1_X";
    fAnalysisTree->SetObservableValue( obsName, x1_X );

    obsName = this->GetName() + (TString) ".z1_X";
    fAnalysisTree->SetObservableValue( obsName, z1_X );

    /////

    obsName = this->GetName() + (TString) ".x2_X";
    fAnalysisTree->SetObservableValue( obsName, x2_X );

    obsName = this->GetName() + (TString) ".z2_X";
    fAnalysisTree->SetObservableValue( obsName, z2_X );

    /////

    obsName = this->GetName() + (TString) ".y1_Y";
    fAnalysisTree->SetObservableValue( obsName, y1_Y );

    obsName = this->GetName() + (TString) ".z1_Y";
    fAnalysisTree->SetObservableValue( obsName, z1_Y );

    /////

    obsName = this->GetName() + (TString) ".y2_Y";
    fAnalysisTree->SetObservableValue( obsName, y2_Y );

    obsName = this->GetName() + (TString) ".z2_Y";
    fAnalysisTree->SetObservableValue( obsName, z2_Y );

    

    for( unsigned int t = 0; t < tracks.size(); t++ )
    {
        // We get the hit blob energy from the origin track (not from the reduced track)
        Int_t longTrackId = tracks[t]->GetTrackID();
        TRestTrack *originTrack = fInputTrackEvent->GetOriginTrackById( longTrackId );
        TRestHits *originHits = (TRestHits *) ( originTrack->GetVolumeHits() );

        if( tracks[t]->isXYZ() )
        {
            for( unsigned int n = 0; n < fQ1_Observables.size(); n++ )
            {
                Double_t q = originHits->GetEnergyInSphere( x1, y1, z1, fQ1_Radius[n] );

                obsName = this->GetName() + (TString) "." + (TString) fQ1_Observables[n];
                fAnalysisTree->SetObservableValue( obsName, q );
            }

            for( unsigned int n = 0; n < fQ2_Observables.size(); n++ )
            {
                Double_t q = originHits->GetEnergyInSphere( x2, y2, z2, fQ2_Radius[n] );

                obsName = this->GetName() + (TString) "." + (TString) fQ2_Observables[n];
                fAnalysisTree->SetObservableValue( obsName, q );
            }
        }

        if( tracks[t]->isXZ() )
        {
            for( unsigned int n = 0; n < fQ1_X_Observables.size(); n++ )
            {
                Double_t q = originHits->GetEnergyInSphere( x1_X, 0, z1_X, fQ1_X_Radius[n] );

                obsName = this->GetName() + (TString) "." + (TString) fQ1_X_Observables[n];
                fAnalysisTree->SetObservableValue( obsName, q );
            }

            for( unsigned int n = 0; n < fQ2_X_Observables.size(); n++ )
            {
                Double_t q = originHits->GetEnergyInSphere( x2_X, 0, z2_X, fQ2_X_Radius[n] );

                obsName = this->GetName() + (TString) "." + (TString) fQ2_X_Observables[n];
                fAnalysisTree->SetObservableValue( obsName, q );
            }
        }

        if( tracks[t]->isYZ() )
        {
            for( unsigned int n = 0; n < fQ1_Y_Observables.size(); n++ )
            {
                Double_t q = originHits->GetEnergyInSphere( 0, y1_Y, z1_Y, fQ1_Y_Radius[n] );

                obsName = this->GetName() + (TString) "." + (TString) fQ1_Y_Observables[n];
                fAnalysisTree->SetObservableValue( obsName, q );
            }

            for( unsigned int n = 0; n < fQ2_Y_Observables.size(); n++ )
            {
                Double_t q = originHits->GetEnergyInSphere( 0, y2_Y, z2_Y, fQ2_Y_Radius[n] );

                obsName = this->GetName() + (TString) "." + (TString) fQ2_Y_Observables[n];
                fAnalysisTree->SetObservableValue( obsName, q );
            }
        }
    }

    return fOutputTrackEvent;
}

//______________________________________________________________________________
void TRestFindTrackBlobsProcess::EndOfEventProcess() 
{
}

//______________________________________________________________________________
void TRestFindTrackBlobsProcess::EndProcess()
{
    // Function to be executed once at the end of the process 
    // (after all events have been processed)

    //Start by calling the EndProcess function of the abstract class. 
    //Comment this if you don't want it.
    //TRestEventProcess::EndProcess();
}

//______________________________________________________________________________
void TRestFindTrackBlobsProcess::InitFromConfigFile( )
{
    fHitsToCheckFraction = StringToDouble( GetParameter( "hitsToCheckFraction", "0.2" ) );
}

