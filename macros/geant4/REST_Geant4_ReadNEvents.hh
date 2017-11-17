#include "TRestTask.h"
#include "TRestG4Metadata.h"
#include "TRestG4Event.h"

Int_t REST_Geant4_ReadNEvents( TString fName, int n1, int n2 )
{
    TRestRun *run = new TRestRun();

    string fname = fName.Data();
    if( !fileExists( fname ) ) { cout << "WARNING. Input file does not exist" << endl; exit(1); }
    else run->OpenInputFile( fName );

    run->OpenInputFile( fName );

    run->PrintInfo();

    /////////////////////////////

    /////////////////////////////
    // Reading events


    TRestG4Event *ev = new TRestG4Event();

    run->SetInputEvent( ev );
    for(int i=n1; i<n2+1;i++)
    {
        run->GetEntry ( i );

        // Do what ever you want with g4Event 
        ev->PrintEvent();
    }

    delete ev;


    /////////////////////////////

    delete run;

    return 0;
}
