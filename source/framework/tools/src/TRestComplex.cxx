/******************** REST disclaimer ***********************************
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
/// TRestComplex is a class that defines static methods with mathematical
/// operations using complex numbers. This class has been generated by
/// refurbishing the existing ROOT TComplex object and updating the double
/// type by real MPFR types that allow to define an arbitrary precision.
///
/// It is possible to redefine the numbers precision as number of digits
/// using TRestComplex::SetPrecision.
///
/// Code example inside `restRoot`:
///
/// \code
///
/// root [0] TRestComplex::SetPrecision(30)
/// root [1] TRestComplex a
/// (TRestComplex &) (1,0i)
/// root [2] TRestComplex b(1.e-15)
/// (TRestComplex &) (1e-15,0i)
/// root [3] TRestComplex c(0,1)
/// (TRestComplex &) (0,1i)
/// root [4] TRestComplex d(0,1.e-25)
/// (TRestComplex &) (0,1e-25i)
/// root [5] a + b + c + d
/// (TRestComplex) (1.000000000000001,1.0000000000000000000000001i)
/// root [6] TRestComplex e = (a+b)/(c+d)
/// (TRestComplex &) (0,-1.0000000000000009999999999i)
///
/// \endcode
///
///--------------------------------------------------------------------------
///
/// RESTsoft - Software for Rare Event Searches with TPCs
///
/// History of developments:
///
/// 2016-December: First concept.
///				   Javier Galan
///
/// \class      TRestComplex
/// \author     Javier Galan <javier.galan@unizar.es>
///
/// <hr>
///

#include "TRestComplex.h"
#include <iostream>
#include <sstream>

ClassImp(TRestComplex);

////////////////////////////////////////////////////////////////////////////////
/// Standard constructor

TRestComplex::TRestComplex(mpfr::mpreal re, mpfr::mpreal im, Bool_t polar) : fRe(re), fIm(im) {
    if (polar) {
        if (re < 0) {
            re = -re;
        }
        fRe = re * mpfr::cos(im);
        fIm = re * mpfr::sin(im);
    }
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& out, const TRestComplex& c) {
    int pr = out.precision();

    int realExponent = (int)mpfr::abs(mpfr::log(mpfr::abs(c.fRe)) / mpfr::log(10));
    int complexExponent = (int)mpfr::abs(mpfr::log(mpfr::abs(c.fIm)) / mpfr::log(10));

    out.precision(mpfr::bits2digits(mpfr::mpreal::get_default_prec()) - realExponent);
    out << "(" << c.fRe;
    out.precision(mpfr::bits2digits(mpfr::mpreal::get_default_prec()) - complexExponent);
    out << "," << c.fIm << "i)";
    out.precision(pr);
    return out;
}

////////////////////////////////////////////////////////////////////////////////

std::istream& operator>>(std::istream& in, TRestComplex& c) {
    in >> c.fRe >> c.fIm;
    return in;
}

namespace cling {
std::string printValue(TRestComplex* c) {
    std::stringstream s;
    s << *c;
    return s.str();
}
}  // namespace cling
