// Copyright (C) 2012 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "SimpsonContourLagrangeTriagP1.hh"
#include "Framework/IntegratorImplProvider.hh"
#include "Framework/State.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace COOLFluiD::Framework;
using namespace COOLFluiD::MathTools;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {



   namespace ShapeFunctions {

//////////////////////////////////////////////////////////////////////////////

IntegratorImplProvider<ContourIntegratorImpl,
                       SimpsonContourLagrangeTriagP1>
simpsonContourLagrangeTriagP1Provider;

//////////////////////////////////////////////////////////////////////////////

void SimpsonContourLagrangeTriagP1::setup()
{  
  _coeff.resize(3);
  for (CFuint iFace = 0; iFace < _coeff.size(); ++iFace) {
    _coeff[iFace].resize(3);
    _coeff[iFace][0] = 1./6.;
    _coeff[iFace][1] = 2./3.;
    _coeff[iFace][2] = 1./6.;
  }
  
  _pattern.setNbShapeFunctions(3);
}

//////////////////////////////////////////////////////////////////////////////

void SimpsonContourLagrangeTriagP1::computeSolutionAtQuadraturePoints(
  const std::vector<Framework::State*>& states,
        std::vector<Framework::State*>& values)
{
  CFLogDebugMin("SimpsonContourLagrangeTriagP1::computeSolutionAtQuadraturePoints()\n");
  
  copy(*states[0], *values[0]);
  static_cast<RealVector&>(*values[1]) = (*states[0]) + (*states[1]);
  *values[1] *= 0.5;
  copy(*states[1], *values[2]);
  
  copy(*states[1], *values[3]);
  static_cast<RealVector&>(*values[4]) = (*states[1]) + (*states[2]);
  *values[4] *= 0.5;
  copy(*states[2], *values[5]);
  
  copy(*states[2], *values[6]);
  static_cast<RealVector&>(*values[7]) = (*states[2]) + (*states[0]);
  *values[7] *= 0.5;
  copy(*states[0], *values[8]);
}

//////////////////////////////////////////////////////////////////////////////

void SimpsonContourLagrangeTriagP1::computeFaceJacobianDetAtQuadraturePoints
(const std::vector<Framework::Node*>& nodes,
 std::vector<RealVector>& faceJacobian)
{
  CFLogDebugMin("SimpsonContourLagrangeTriagP1::computeFaceJacobianDetAtQuadraturePoints()\n");
  
  cf_assert(faceJacobian.size() >= 3);
  const CFreal face01 = MathFunctions::getDistance(*nodes[0], *nodes[1]);
  const CFreal face12 = MathFunctions::getDistance(*nodes[1], *nodes[2]);
  const CFreal face20 = MathFunctions::getDistance(*nodes[2], *nodes[0]);
  
  faceJacobian[0][0] = faceJacobian[0][1] = faceJacobian[0][2] = face01;
  faceJacobian[1][0] = faceJacobian[1][1] = faceJacobian[1][2] = face12;
  faceJacobian[2][0] = faceJacobian[2][1] = faceJacobian[2][2] = face20;
}

//////////////////////////////////////////////////////////////////////////////

    } // namespace ShapeFunctions



} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////
