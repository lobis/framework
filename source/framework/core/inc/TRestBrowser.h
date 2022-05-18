#ifndef RestCore_TRestBrowser
#define RestCore_TRestBrowser

#include <iostream>

#include "TApplication.h"
#include "TBrowser.h"
#include "TCanvas.h"
#include "TGButton.h"
#include "TGComboBox.h"
#include "TGDimension.h"
#include "TGFileDialog.h"
#include "TGFrame.h"
#include "TGLabel.h"
#include "TGNumberEntry.h"
#include "TGTextEntry.h"
#include "TObject.h"
#include "TROOT.h"
#include "TRestEventViewer.h"
#include "TRestRun.h"
#include "TSystem.h"

/// Event browser for different input file
class TRestBrowser {
   protected:
#ifndef __CINT__
    TGMainFrame* frmMain = nullptr;                 //!
    TGVerticalFrame* fVFrame = nullptr;             //! < The main vertical frame for browser controlling
    TGLabel* fEventRowLabel = nullptr;              //!
    TGLabel* fEventIdLabel = nullptr;               //!
    TGLabel* fEventSubIdLabel = nullptr;            //!
    TGNumberEntry* fEventRowNumberBox = nullptr;    //! For row number.
    TGNumberEntry* fEventIdNumberBox = nullptr;     //! For Event number.
    TGNumberEntry* fEventSubIdNumberBox = nullptr;  //! For sub Event number.

    TGLabel* fEventTypeLabel = nullptr;        //!
    TGComboBox* fEventTypeComboBox = nullptr;  //!

    TGLabel* fPlotOptionLabel = nullptr;        //!
    TGTextEntry* fPlotOptionTextBox = nullptr;  //! TextBox for plot options
    TGTextButton* fButOptPrev = nullptr;        //! Previous plot option
    TGTextButton* fButOptRefresh = nullptr;     //! Refresh plot
    TGTextButton* fButOptNext = nullptr;        //! Next plot option

    TGPictureButton* fMenuOpen = nullptr;  //! Open file button
    TGTextButton* fExit = nullptr;         //! Exit button

    TCanvas* fCanDefault = nullptr;  //!
    Int_t fEventRow = 0;             //!
    Int_t fEventId = 0;              //!
    Int_t fEventSubId = 0;           //!

    TBrowser* b = nullptr;  //!
    TRestRun* r = nullptr;  //!
#endif

   private:
#ifndef __CINT__
    Bool_t pureAnalysis;
    TString fInputFileName;

    TRestEventViewer* fEventViewer = nullptr;  //!

    void SetViewer(TRestEventViewer* eV);
    void SetViewer(TString viewerName);
    void SetButtons();
    Bool_t LoadEventId(Int_t id, Int_t subid = -1);
    Bool_t LoadEventEntry(Int_t n);
#endif

   public:
    // tool method
    void Initialize(TString opt = "FI");
    void InitFromConfigFile();
    Bool_t OpenFile(TString filename);

    // setters
    void SetInputEvent(TRestEvent*);
    inline void SetWindowName(const TString& windowName) { frmMain->SetWindowName(windowName.Data()); }

    // getters
    inline TRestEventViewer* GetViewer() const { return fEventViewer; }

    // actions
    void LoadFileAction();
    void ExitAction();

    void RowValueChangedAction(Long_t val);
    void IdValueChangedAction(Long_t val);

    void EventTypeChangedAction(Int_t id);

    void NextPlotOptionAction();
    void PreviousPlotOptionAction();
    void PlotAction();

    // Constructors
    TRestBrowser();
    TRestBrowser(TString viewerName);

    // Destructor
    ~TRestBrowser();
};
#endif
