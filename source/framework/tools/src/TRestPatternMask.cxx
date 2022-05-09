/*************************************************************************
 * This file is part of the REST software framework.                     *
 *                                                                       *
 * Copyright (C) 2016 GIFNA/TREX (University of Zaragoza)                *
 * For more information see https://gifna.unizar.es/trex                 *
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
 * If not, see https://www.gnu.org/licenses/.                            *
 * For the list of contributors see $REST_PATH/CREDITS.                  *
 *************************************************************************/

/////////////////////////////////////////////////////////////////////////
/// Write the class description Here
///
/// ### Parameters
/// Describe any parameters this process receives:
/// * **parameter1**: This parameter ...
/// * **parameter2**: This parameter is ...
///
///
/// ### Examples
/// Give examples of usage and RML descriptions that can be tested.
/// \code
///     <WRITE A CODE EXAMPLE HERE>
/// \endcode
///
/// ### Running pipeline example
/// Add the examples to a pipeline to guarantee the code will be running
/// on future framework upgrades.
///
///
/// Please, add any figure that may help to illustrate the process
///
/// \htmlonly <style>div.image img[src="trigger.png"]{width:500px;}</style> \endhtmlonly
/// ![An illustration of the trigger definition](trigger.png)
///
/// The png image should be uploaded to the ./images/ directory
///
///----------------------------------------------------------------------
///
/// REST-for-Physics - Software for Rare Event Searches Toolkit
///
/// History of developments:
///
/// YEAR-Month: First implementation of TRestPatternMask
/// WRITE YOUR FULL NAME
///
/// \class TRestPatternMask
/// \author: TODO. Write full name and e-mail:        jgalan
///
/// <hr>
///

#include "TAxis.h"
#include "TColor.h"
#include "TGraph.h"
#include "TH1F.h"
#include "TRandom3.h"

#include "TRestPatternMask.h"

ClassImp(TRestPatternMask);

///////////////////////////////////////////////
/// \brief Default constructor
///
TRestPatternMask::TRestPatternMask() : TRestMetadata() { Initialize(); }

/////////////////////////////////////////////
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
/// \param name The name of the specific metadata. It will be used to find the
/// corresponding TRestAxionMagneticField section inside the RML.
///
TRestPatternMask::TRestPatternMask(const char* cfgFileName, std::string name) : TRestMetadata(cfgFileName) {
    // Initialize();

    if (GetVerboseLevel() >= REST_Info) PrintMetadata();
}

///////////////////////////////////////////////
/// \brief Default destructor
///
TRestPatternMask::~TRestPatternMask() {}

///////////////////////////////////////////////
/// \brief It produces an effective mask rotation and translation for the
/// point x,y.
///
/// Remark: The window is centered, it is just the pattern that we want
/// to shift.
///
void TRestPatternMask::RotateAndTranslate(Double_t& x, Double_t& y) {
    TVector2 pos(x, y);

    pos -= fOffset;
    pos = pos.Rotate(-fRotationAngle);

    x = pos.X();
    y = pos.Y();
}

///////////////////////////////////////////////
/// \brief Returns true if the pattern was hitted.
///
bool TRestPatternMask::HitsPattern(Double_t x, Double_t y) {
    if (GetRegion(x, y) > 0) return true;
    return false;
}

/////////////////////////////////////////////
/// \brief Prints on screen the information about the metadata members of TRestAxionSolarFlux
///
void TRestPatternMask::PrintMetadata() {
    TRestMetadata::PrintMetadata();

    metadata << " - Type : " << fPatternType << endl;
    metadata << " - Offset : (" << fOffset.X() << ", " << fOffset.Y() << ") mm" << endl;
    metadata << " - Rotation angle : " << fRotationAngle * 180. / TMath::Pi() << " degrees" << endl;
    metadata << "----" << endl;
}

/////////////////////////////////////////////
/// \brief TOBE written
///
TCanvas* TRestPatternMask::DrawMonteCarlo(Int_t nSamples) {
    if (fCanvas != NULL) {
        delete fCanvas;
        fCanvas = NULL;
    }
    fCanvas = new TCanvas("canv", "This is the canvas title", 1400, 1200);
    fCanvas->Draw();

    TPad* pad1 = new TPad("pad1", "This is pad1", 0.01, 0.02, 0.99, 0.97);
    pad1->Draw();

    fCanvas->SetRightMargin(0.09);
    fCanvas->SetLeftMargin(0.15);
    fCanvas->SetBottomMargin(0.15);

    /// Generating Montecarlo
    std::map<int, std::vector<TVector2> > points;

    TRandom3* rnd = new TRandom3(0);

    for (int n = 0; n < nSamples; n++) {
        Double_t x = 3 * (rnd->Rndm() - 0.5) * fMaskRadius + fOffset.X();
        Double_t y = 3 * (rnd->Rndm() - 0.5) * fMaskRadius + fOffset.Y();

        Int_t id = GetRegion(x, y);

        if (points.count(id) == 0) {
            std::vector<TVector2> a;
            a.push_back(TVector2(x, y));
            points[id] = a;
        } else {
            points[id].push_back(TVector2(x, y));
        }
    }

    std::vector<EColor> colors{kOrange, kGreen, kPink};

    std::vector<TGraph*> gridGraphs;
    Int_t nGraphs = 0;
    for (const auto& x : points) {
        std::string grname = "gr" + IntegerToString(nGraphs);
        TGraph* gr = new TGraph();
        gr->SetName(grname.c_str());

        for (int n = 0; n < x.second.size(); n++) {
            gr->SetPoint(gr->GetN(), x.second[n].X(), x.second[n].Y());
        }

        if (nGraphs == 0) {
            gr->SetLineColor(kBlack);
            gr->SetMarkerColor(kBlack);
            gr->SetMarkerSize(0.6);
            gr->SetLineWidth(2);
        } else {
            gr->SetMarkerColor((nGraphs * 2) % 29 + 20);
            gr->SetLineColor((nGraphs * 2) % 29 + 20);
            gr->SetMarkerSize(0.6);
            gr->SetLineWidth(2);
        }

        gr->SetMarkerStyle(20);
        gridGraphs.push_back(gr);
        nGraphs++;
    }

    gridGraphs[0]->GetXaxis()->SetLimits(fOffset.X() - 1.75 * fMaskRadius, fOffset.X() + 1.75 * fMaskRadius);
    gridGraphs[0]->GetHistogram()->SetMaximum(fOffset.Y() + 1.75 * fMaskRadius);
    gridGraphs[0]->GetHistogram()->SetMinimum(fOffset.Y() - 1.75 * fMaskRadius);

    gridGraphs[0]->GetXaxis()->SetTitle("X [mm]");
    gridGraphs[0]->GetXaxis()->SetTitleSize(0.05);
    gridGraphs[0]->GetXaxis()->SetLabelSize(0.05);
    gridGraphs[0]->GetYaxis()->SetTitle("Y[mm]");
    gridGraphs[0]->GetYaxis()->SetTitleOffset(1.5);
    gridGraphs[0]->GetYaxis()->SetTitleSize(0.05);
    gridGraphs[0]->GetYaxis()->SetLabelSize(0.05);
    pad1->SetLogy();
    gridGraphs[0]->Draw("AP");
    for (int n = 1; n < nGraphs; n++) gridGraphs[n]->Draw("P");

    return fCanvas;
}
