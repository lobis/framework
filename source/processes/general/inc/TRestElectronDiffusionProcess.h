///______________________________________________________________________________
///______________________________________________________________________________
///______________________________________________________________________________
///             
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestElectronDiffusionProcess.h
///
///_______________________________________________________________________________


#ifndef RestCore_TRestElectronDiffusionProcess
#define RestCore_TRestElectronDiffusionProcess

#include <TRestGas.h>
#include <TRestG4Event.h>
#include <TRestHitsEvent.h>

#include "TRestEventProcess.h"

class TRestElectronDiffusionProcess:public TRestEventProcess {
    private:
        TRestG4Event *fG4Event;
        TRestHitsEvent *fHitsEvent;

        Double_t fCathodePosition;
        Double_t fAnodePosition;

        Double_t fMaxPosition;
        Double_t fMinPosition;

        Double_t fElectricField;
        Double_t fResolution;

        void InitFromConfigFile();

        void Initialize();

        void LoadDefaultConfig();

    protected:
        //add here the members of your event process
        TRestGas *fGas;


    public:
        void InitProcess();
        void BeginOfEventProcess(); 
        TRestEvent *ProcessEvent( TRestEvent *eventInput );
        void EndOfEventProcess(); 
        void EndProcess();

        void LoadConfig( string cfgFilename );

        void PrintMetadata() { 

            BeginPrintProcess();

            cout << " cathode : " << fCathodePosition << endl;
            cout << " anode : " << fAnodePosition << endl;
            cout << " eField : " << fElectricField << endl;
            cout << " resolution : " << fResolution << endl;

            EndPrintProcess();

        }

        TRestMetadata *GetProcessMetadata( ) { return fGas; }

        TString GetProcessName() { return (TString) "electronDiffusion"; }

	Double_t GetCathodePosition() { return fCathodePosition; }
	Double_t GetElectricField() { return fElectricField; }

        //Constructor
        TRestElectronDiffusionProcess();
        TRestElectronDiffusionProcess( char *cfgFileName );
        //Destructor
        ~TRestElectronDiffusionProcess();

        ClassDef(TRestElectronDiffusionProcess, 1);      // Template for a REST "event process" class inherited from TRestEventProcess
};
#endif

