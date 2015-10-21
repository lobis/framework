
///______________________________________________________________________________
///______________________________________________________________________________
///______________________________________________________________________________
///             
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestTrackEvent.h
///
///             Event class to store DAQ events either from simulation and acquisition 
///
///             oct 2015:   First concept
///                 Created as part of the conceptualization of existing REST 
///                 software.
///                 Javier Gracia
///_______________________________________________________________________________


#ifndef RestDAQ_TRestSignalEvent
#define RestDAQ_TRestSignalEvent

#include <iostream>
using namespace std;

#include <TObject.h>
#include <TArrayD.h>

#include "TRestEvent.h"
#include "TRestSignal.h"
#include "TRestTrack.h"

class TRestTrackEvent: public TRestEvent {

    protected:
 
        Int_t nTracks;       
        vector <TRestTrack> fTrack; //Collection of tracks that define the event
        
    
    public:

        //Setters
        void AddCluster(TRestTrack c){fTrack.push_back(c); nTracks++;}
        void RemoveCluster( ){fTrack.clear(); nTracks--;}  
        //Getters
        Int_t GetNTracks(){return nTracks;}
      
        void Initialize();

        void PrintEvent();
                
        //Construtor
        TRestTrackEvent();
        //Destructor
        virtual ~ TRestTrackEvent();

        ClassDef(TRestTrackEvent, 1);     // REST event superclass
};
#endif
