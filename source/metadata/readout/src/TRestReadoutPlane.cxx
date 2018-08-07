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
 ///
 /// This class stores the readout plane geometrical description, plane 
 /// position, orientation, and cathode position. It contains
 /// a vector of TRestReadoutModule with the readout modules that are
 /// implemented in the readout plane.
 /// 
 ///--------------------------------------------------------------------------
 ///
 /// RESTsoft - Software for Rare Event Searches with TPCs
 ///
 /// History of developments:
 ///
 /// 2016-mar:  First concept.
 ///            Javier Galan
 ///
 /// \class      TRestReadoutPlane
 /// \author     Javier Galan
 ///
 /// <hr>
 ///

#include "TRestReadoutPlane.h"
#include "TCanvas.h"
using namespace std;

const int debug = 0;

ClassImp(TRestReadoutPlane)
///////////////////////////////////////////////
/// \brief Default TRestReadoutPlane constructor
/// 
TRestReadoutPlane::TRestReadoutPlane()
{
	Initialize();
}

///////////////////////////////////////////////
/// \brief Default TRestReadoutPlane destructor
/// 
TRestReadoutPlane::~TRestReadoutPlane()
{
}

///////////////////////////////////////////////
/// \brief TRestReadoutPlane initialization
/// 
void TRestReadoutPlane::Initialize()
{
	fCathodePosition = TVector3(0, 0, 0);
	fPosition = TVector3(0, 0, 0);
	fPlaneVector = TVector3(0, 0, 0);

	fNModules = 0;
	fReadoutModules.clear();
}

///////////////////////////////////////////////
/// \brief Returns the total number of channels in the readout plane
/// 
Int_t TRestReadoutPlane::GetNumberOfChannels()
{
	Int_t nChannels = 0;
	for (int md = 0; md < GetNumberOfModules(); md++)
		nChannels += fReadoutModules[md].GetNumberOfChannels();
	return nChannels;
}

///////////////////////////////////////////////
/// \brief Calculates the drift distance between readout plane and cathode
/// 
void TRestReadoutPlane::SetDriftDistance()
{
	Double_t tDriftDistance = this->GetDistanceTo(this->GetCathodePosition());
	this->SetTotalDriftDistance(tDriftDistance);
}

///////////////////////////////////////////////
/// \brief Returns a pointer to a module using its internal module id
/// 
TRestReadoutModule *TRestReadoutPlane::GetModuleByID(Int_t modID)
{

	for (int md = 0; md < GetNumberOfModules(); md++)
		if (fReadoutModules[md].GetModuleID() == modID)
			return &fReadoutModules[md];

	cout << "REST ERROR (GetReadoutModuleByID) : Module ID : " << modID << " was not found" << endl;
	return NULL;
}

///////////////////////////////////////////////
/// \brief Returns a pointer to a channel using its internal channel and module ids
/// 
TRestReadoutChannel *TRestReadoutPlane::GetChannelByID(Int_t modID, Int_t chID)
{
	TRestReadoutModule *module = GetModuleByID(modID);

	return module->GetChannelByID(chID);
	//for( int ch = 0; ch < module->GetNumberOfChannels(); ch++ )
	//    if( module->GetChannel( ch )->GetID() == chID )
	//        return module->GetChannel( ch );

	//cout << "REST ERROR (GetReadoutChannelByID) : Channel ID " << chID << " Module ID : " << modID << " was not found" << endl;
	//return NULL;
}

///////////////////////////////////////////////
/// \brief Returns the X coordinate of a given channel in a given module using
/// their internal module and channel ids.
/// 
/// This method evaluates if the channel is a strip or a pixel. If it is a 
/// strip and the X-axis is not localized, this function returns NaN.
///
/// \param modID Internal module id. As defined on the readout.
/// \param chID Internal channel id. As defined on the readout.
///
/// \return The value of the X-coordinate relative to the readout position
Double_t TRestReadoutPlane::GetX(Int_t modID, Int_t chID)
{
	Double_t x = numeric_limits<Double_t>::quiet_NaN();

	TRestReadoutModule *rModule = GetModuleByID(modID);
	if (rModule == NULL)return x;

	Double_t xOrigin = rModule->GetModuleOriginX();

	TRestReadoutChannel *rChannel = rModule->GetChannelByID(chID);
	if (rChannel == NULL)return x;

	if (rChannel->GetType() == Channel_NoType) {

		if (rChannel->GetNumberOfPixels() == 1)
		{
			Double_t sX = (*rChannel)[0].GetSize().X();
			Double_t sY = (*rChannel)[0].GetSize().Y();

			if (sX > 2 * sY) return x;

			x = xOrigin + (*rChannel)[0].GetCenter().X();
		}
		else if (rChannel->GetNumberOfPixels() > 1)
		{
			Int_t nPix = rChannel->GetNumberOfPixels();
			// We check the origin of consecutive pixels to check if it goes X or Y direction.
			// Perhaps more complex readouts need some changes here

			Double_t x1 = (*rChannel)[0].GetCenter().X();
			Double_t x2 = (*rChannel)[nPix - 1].GetCenter().X();

			Double_t y1 = (*rChannel)[0].GetCenter().Y();
			Double_t y2 = (*rChannel)[nPix - 1].GetCenter().Y();

			/*
			cout << "Pix id : " << rChannel->GetPixel(0)->GetID() << " X1 : " << x1 << endl;
			cout << "Pix id : " << rChannel->GetPixel(1)->GetID() << " X2 : " << x2 << endl;
			cout << "Pix id : " << rChannel->GetPixel(0)->GetID() << " Y1 : " << y1 << endl;
			cout << "Pix id : " << rChannel->GetPixel(1)->GetID() << " Y2 : " << y2 << endl;
			*/

			Double_t deltaX, deltaY;

			if (x2 - x1 > 0) deltaX = x2 - x1;
			else deltaX = x1 - x2;

			if (y2 - y1 > 0) deltaY = y2 - y1;
			else deltaY = y1 - y2;

			if (deltaY > deltaX) x = xOrigin + (*rChannel)[0].GetCenter().X();
		}
	}
	else if (rChannel->GetType() == Channel_Pixel) {
		x = rModule->GetPixelCenter(chID, 0).X();
	}
	else if (rChannel->GetType() == Channel_Y) {
		if (rModule->GetModuleRotation() == 0 || rModule->GetModuleRotation() == 180)
			x = rModule->GetPixelCenter(chID, 0).X();
	}
	else if (rChannel->GetType() == Channel_X) {
		if (rModule->GetModuleRotation() == 90 || rModule->GetModuleRotation() == 270)
			x = rModule->GetPixelCenter(chID, 0).Y();
	}
	return x;
}

///////////////////////////////////////////////
/// \brief Returns the Y coordinate of a given channel in a given module using
/// their internal module and channel ids.
/// 
/// This method evaluates if the channel is a strip or a pixel. If it is a 
/// strip and the Y-axis is not localized, this function returns NaN.
///
/// \param modID Internal module id. As defined on the readout.
/// \param chID Internal channel id. As defined on the readout.
///
/// \return The value of the X-coordinate relative to the readout position
Double_t TRestReadoutPlane::GetY(Int_t modID, Int_t chID)
{
	Double_t y = numeric_limits<Double_t>::quiet_NaN();

	TRestReadoutModule *rModule = GetModuleByID(modID);
	if (rModule == NULL)return y;

	Double_t yOrigin = rModule->GetModuleOriginY();

	TRestReadoutChannel *rChannel = rModule->GetChannelByID(chID);
	if (rChannel == NULL)return y;

	if (rChannel->GetType() == Channel_NoType) {
		if (rChannel->GetNumberOfPixels() == 1)
		{
			Double_t sX = (*rChannel)[0].GetSize().X();
			Double_t sY = (*rChannel)[0].GetSize().Y();

			if (sY > 2 * sX) return y;

			y = yOrigin + (*rChannel)[0].GetCenter().Y();
		}
		else if (rChannel->GetNumberOfPixels() > 1)
		{
			Int_t nPix = rChannel->GetNumberOfPixels();
			// We check the origin of consecutive pixels to check if it goes X or Y direction.
			// Perhaps more complex readouts need some changes here

			Double_t x1 = (*rChannel)[0].GetCenter().X();
			Double_t x2 = (*rChannel)[nPix - 1].GetCenter().X();

			Double_t y1 = (*rChannel)[0].GetCenter().Y();
			Double_t y2 = (*rChannel)[nPix - 1].GetCenter().Y();

			/*
			cout << "Pix id : " << rChannel->GetPixel(0)->GetID() << " X1 : " << x1 << endl;
			cout << "Pix id : " << rChannel->GetPixel(1)->GetID() << " X2 : " << x2 << endl;
			cout << "Pix id : " << rChannel->GetPixel(0)->GetID() << " Y1 : " << y1 << endl;
			cout << "Pix id : " << rChannel->GetPixel(1)->GetID() << " Y2 : " << y2 << endl;
			*/

			Double_t deltaX, deltaY;

			if (x2 - x1 > 0) deltaX = x2 - x1;
			else deltaX = x1 - x2;

			if (y2 - y1 > 0) deltaY = y2 - y1;
			else deltaY = y1 - y2;

			if (deltaY < deltaX) y = yOrigin + (*rChannel)[0].GetCenter().Y();
		}
	}
	else if (rChannel->GetType() == Channel_Pixel) {
		y = rModule->GetPixelCenter(chID, 0).Y();
	}
	else if (rChannel->GetType() == Channel_Y) {
		if (rModule->GetModuleRotation() == 0 || rModule->GetModuleRotation() == 180)
			y = rModule->GetPixelCenter(chID, 0).Y();
	}
	else if (rChannel->GetType() == Channel_X) {
		if (rModule->GetModuleRotation() == 90 || rModule->GetModuleRotation() == 270)
			y = rModule->GetPixelCenter(chID, 0).X();
	}

	return y;
}

///////////////////////////////////////////////
/// \brief Finds the readout channel for a given module stored in a given 
/// module index stored in the readout plane (internal readout plane module id).
/// 
/// \param absX It is the x absolut physical position 
/// \param absY It is the y absolut physical position 
/// \return The corresponding channel id
Int_t TRestReadoutPlane::FindChannel(Int_t module, Double_t absX, Double_t absY)
{
	Double_t modX = absX - fPosition.X();
	Double_t modY = absY - fPosition.Y();

	// TODO : check first if (modX,modY) is inside the module.
	// If not return error.
	// FindChannel will take a long time to search for the channel if it is not there.
	// It will be faster

	return GetModuleByID(module)->FindChannel(modX, modY);
}

///////////////////////////////////////////////
/// \brief Returns the perpendicular distance to the readout plane of a given *x*, *y*, *z* position
/// 
Double_t TRestReadoutPlane::GetDistanceTo(Double_t x, Double_t y, Double_t z)
{
	return GetDistanceTo(TVector3(x, y, z));
}


///////////////////////////////////////////////
/// \brief Returns the perpendicular distance to the readout plane of a given TVector3 position
/// 
Double_t TRestReadoutPlane::GetDistanceTo(TVector3 pos)
{
	return (pos - GetPosition()).Dot(GetPlaneVector());
}

///////////////////////////////////////////////
/// \brief This method determines if a given *x*, *y*, *z* coordinates are inside the
/// readout plane definition. The z-coordinate must be found in between the cathode
/// and the readout plane. The *x* and *y* values must be found inside one of the
/// readout modules defined inside the readout plane.
///
/// \return the module *id* where the hit is found. If no module *id* is found it
/// returns -1.
///
Int_t TRestReadoutPlane::isInsideDriftVolume(Double_t x, Double_t y, Double_t z)
{
	TVector3 pos = TVector3(x, y, z);

	return isInsideDriftVolume(pos);
}

///////////////////////////////////////////////
/// \brief This method determines if a given position,
/// is inside the readout plane definition. The z-coordinate must be found in between 
/// the cathode and the readout plane. The *x* and *y* values must be found inside 
/// one of the readout modules defined inside the readout plane.
///
/// \param pos A TVector3 definning the position.
///
/// \return the module *id* where the hit is found. If no module *id* is found it
/// returns -1.
///
Int_t TRestReadoutPlane::isInsideDriftVolume(TVector3 pos)
{
	TVector3 posNew = TVector3(pos.X() - fPosition.X(), pos.Y() - fPosition.Y(), pos.Z());

	Double_t distance = GetDistanceTo(posNew);

	if (distance > 0 && distance < fTotalDriftDistance)
	{
		for (int m = 0; m < GetNumberOfModules(); m++)
			if (fReadoutModules[m].isInside(posNew.X(), posNew.Y())) return fReadoutModules[m].GetModuleID();
	}

	return -1;
}

///////////////////////////////////////////////
/// \brief Prints information with details of the readout plane and modules 
/// defined inside the readout plane.
/// 
void TRestReadoutPlane::Print(Int_t fullDetail)
{
	cout << "-- Readout plane : " << GetID() << endl;
	cout << "----------------------------------------------------------------" << endl;
	cout << "-- Position : X = " << fPosition.X() << " mm, " << " Y : " << fPosition.Y() << " mm, Z : " << fPosition.Z() << " mm" << endl;
	cout << "-- Vector : X = " << fPlaneVector.X() << " mm, " << " Y : " << fPlaneVector.Y() << " mm, Z : " << fPlaneVector.Z() << " mm" << endl;
	cout << "-- Cathode Position : X = " << fCathodePosition.X() << " mm, " << " Y : " << fCathodePosition.Y() << " mm, Z : " << fCathodePosition.Z() << " mm" << endl;
	cout << "-- Total drift distance : " << fTotalDriftDistance << " mm" << endl;
	cout << "-- Charge collection : " << fChargeCollection << endl;
	cout << "-- Total modules : " << GetNumberOfModules() << endl;
	cout << "-- Total channels : " << GetNumberOfChannels() << endl;
	cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;

	for (int i = 0; i < GetNumberOfModules(); i++)
		fReadoutModules[i].Print(fullDetail);

}

///////////////////////////////////////////////
/// \brief Draws the readout plane using GetReadoutHistogram. 
///
void TRestReadoutPlane::Draw()
{
	TCanvas*c1 = new TCanvas();

	this->GetReadoutHistogram()->Draw();
	c1->Print("/tmp/readout.png");
	//this->GetReadoutHistogram()->Print();
	cout << "Press a key to continue..." << endl;
	getchar();
	delete c1;
}

///////////////////////////////////////////////
/// \brief Creates and resturns a TH2Poly object with the 
/// readout pixel description.
///
TH2Poly *TRestReadoutPlane::GetReadoutHistogram()
{

	Double_t x[4];
	Double_t y[4];

	double xmin, xmax, ymin, ymax;

	GetBoundaries(xmin, xmax, ymin, ymax);

	TH2Poly *readoutHistogram = new TH2Poly("ReadoutHistogram", "ReadoutHistogram", xmin, xmax, ymin, ymax);

	for (int mdID = 0; mdID < this->GetNumberOfModules(); mdID++)
	{
		TRestReadoutModule *module = &fReadoutModules[mdID];

		int nChannels = module->GetNumberOfChannels();

		for (int ch = 0; ch < nChannels; ch++)
		{
			TRestReadoutChannel *channel = &(*module)[ch];
			Int_t nPixels = channel->GetNumberOfPixels();

			for (int px = 0; px < nPixels; px++)
			{
				for (int v = 0; v < 4; v++)
				{
					x[v] = module->GetPixelVertex(&(*module)[ch][px], v).X();
					y[v] = module->GetPixelVertex(&(*module)[ch][px], v).Y();
				}

				readoutHistogram->AddBin(4, x, y);
			}

		}
	}

	readoutHistogram->SetStats(0);

	return readoutHistogram;
}

///////////////////////////////////////////////
/// \brief Finds the xy boundaries of the readout plane delimited by the modules
///
void TRestReadoutPlane::GetBoundaries(double &xmin, double &xmax, double &ymin, double &ymax)
{

	Double_t x[4];
	Double_t y[4];

	xmin = 1E9, xmax = -1E9, ymin = 1E9, ymax = -1E9;

	for (int mdID = 0; mdID < this->GetNumberOfModules(); mdID++)
	{
		TRestReadoutModule *module = &fReadoutModules[mdID];

		for (int v = 0; v < 4; v++)
		{
			x[v] = module->GetVertex(v).X();
			y[v] = module->GetVertex(v).Y();

			if (x[v] < xmin) xmin = x[v];
			if (y[v] < ymin) ymin = y[v];
			if (x[v] > xmax) xmax = x[v];
			if (y[v] > ymax) ymax = y[v];

		}
	}
}

