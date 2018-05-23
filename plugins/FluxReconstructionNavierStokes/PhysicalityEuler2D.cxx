#include "Framework/CFSide.hh"
#include "Framework/MethodCommandProvider.hh"
#include "Framework/MeshData.hh"

#include "MathTools/MathFunctions.hh"

#include "NavierStokes/Euler2DVarSet.hh"
#include "NavierStokes/EulerTerm.hh"

#include "FluxReconstructionMethod/FluxReconstructionElementData.hh"

#include "FluxReconstructionNavierStokes/FluxReconstructionNavierStokes.hh"
#include "FluxReconstructionNavierStokes/PhysicalityEuler2D.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Common;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::MathTools;
using namespace COOLFluiD::Common;
using namespace COOLFluiD::Physics::NavierStokes;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace FluxReconstructionMethod {
    
//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider<PhysicalityEuler2D, FluxReconstructionSolverData, FluxReconstructionNavierStokesModule>
    PhysicalityEuler2DFRProvider("PhysicalityEuler2D");

//////////////////////////////////////////////////////////////////////////////

void PhysicalityEuler2D::defineConfigOptions(Config::OptionList& options)
{
  options.addConfigOption< CFreal >("MinDensity","Minimum allowable value for density (only used for Cons).");
  options.addConfigOption< CFreal >("MinPressure","Minimum allowable value for pressure.");
  options.addConfigOption< CFreal >("MinTemperature","Minimum allowable value for temperature (only used for Puvt).");
  options.addConfigOption< bool >("CheckInternal","Boolean to tell wether to also check internal solution for physicality.");
}

//////////////////////////////////////////////////////////////////////////////

PhysicalityEuler2D::PhysicalityEuler2D(const std::string& name) :
  BasePhysicality(name),
  m_minDensity(),
  m_minPressure(),
  m_minTemperature(),
  m_eulerVarSet(CFNULL),
  m_eulerVarSetMS(CFNULL),
  m_gammaMinusOne(),
  m_solPhysData(),
  m_cellAvgState(),
  m_cellAvgSolCoefs(),
  m_nbSpecies()
{
  addConfigOptionsTo(this);

  m_minDensity = 1e-2;
  setParameter( "MinDensity", &m_minDensity );

  m_minPressure = 1e-2;
  setParameter( "MinPressure", &m_minPressure );
  
  m_minTemperature = 1e-2;
  setParameter( "MinTemperature", &m_minTemperature );
  
  m_checkInternal = false;
  setParameter( "CheckInternal", &m_checkInternal );
}

//////////////////////////////////////////////////////////////////////////////

PhysicalityEuler2D::~PhysicalityEuler2D()
{
}

//////////////////////////////////////////////////////////////////////////////

void PhysicalityEuler2D::configure ( Config::ConfigArgs& args )
{
  BasePhysicality::configure(args);
}

//////////////////////////////////////////////////////////////////////////////

bool PhysicalityEuler2D::checkPhysicality()
{
  bool physical = true;
  const CFuint nbDims = PhysicalModelStack::getActive()->getDim();
  const bool Puvt = getMethodData().getUpdateVarStr() == "Puvt";
  const bool Cons = getMethodData().getUpdateVarStr() == "Cons";
  const bool RhoivtTv = getMethodData().getUpdateVarStr() == "RhoivtTv";
  const bool hasArtVisc = getMethodData().hasArtificialViscosity();
  DataHandle< CFreal > posPrev = socket_posPrev.getDataHandle();
  const CFuint cellID = m_cell->getID();
  
  for (CFuint iFlx = 0; iFlx < m_maxNbrFlxPnts; ++iFlx)
  { 
    if (Puvt)
    {
      if (m_cellStatesFlxPnt[iFlx][0] < m_minPressure || m_cellStatesFlxPnt[iFlx][3] < m_minTemperature)
      {
	physical = false;
      }
      
      if (hasArtVisc)
      {
	posPrev[cellID] = min(m_cellStatesFlxPnt[iFlx][0]/m_minPressure,posPrev[cellID]);
	posPrev[cellID] = min(m_cellStatesFlxPnt[iFlx][3]/m_minTemperature,posPrev[cellID]);
      }
    }
    else if(Cons)
    {
      CFreal rho  = m_cellStatesFlxPnt[iFlx][0];
      CFreal rhoU = m_cellStatesFlxPnt[iFlx][1];
      CFreal rhoV = m_cellStatesFlxPnt[iFlx][2];
      CFreal rhoE = m_cellStatesFlxPnt[iFlx][3];
      CFreal press = m_gammaMinusOne*(rhoE - 0.5*(rhoU*rhoU+rhoV*rhoV)/rho);
      
      if (rho < m_minDensity || press < m_minPressure)
      {
	physical = false;
      }
      
      if (hasArtVisc)
      {
	posPrev[cellID] = min(rho/m_minDensity,posPrev[cellID]);
	posPrev[cellID] = min(press/m_minPressure,posPrev[cellID]);
      }
    }
    else if(RhoivtTv)
    {
      for (CFuint i = 0 ; i<m_nbSpecies ; ++i){
	CFreal rho  = m_cellStatesFlxPnt[iFlx][i];
	if (rho < m_minDensity)
	  {
	    physical = false;
	    break;
	  }	
      }
      for(CFuint i = m_nbSpecies+nbDims-1 ; i<m_nbrEqs; ++i){
	CFreal T = m_cellStatesFlxPnt[iFlx][i];
	if( T <  m_minTemperature ){
	  physical = false;
	  break;
	}	
      }
      
    }
  }
  
  if (physical && m_checkInternal)
  {
    for (CFuint iSol = 0; iSol < m_nbrSolPnts; ++iSol)
    {
      if (Puvt)
      {
	if ((*((*m_cellStates)[iSol]))[0] < m_minPressure || (*((*m_cellStates)[iSol]))[3] < m_minTemperature)
        {
	  physical = false;
        }
      }
      else if(Cons)
      {
        CFreal rho  = (*((*m_cellStates)[iSol]))[0];
	
	if (rho < m_minDensity)
        {
	  physical = false;
        }
        else
	{
	  CFreal rhoU = (*((*m_cellStates)[iSol]))[1];
          CFreal rhoV = (*((*m_cellStates)[iSol]))[2];
          CFreal rhoE = (*((*m_cellStates)[iSol]))[3];
	  CFreal press = m_gammaMinusOne*(rhoE - 0.5*(rhoU*rhoU+rhoV*rhoV)/rho);
	  
	  if (press < m_minPressure)
	  {
	    physical = false;
	  }
	}
      }
      else if (RhoivtTv){
	for (CFuint i = 0 ; i<m_nbSpecies ; ++i){
	  if((*((*m_cellStates)[iSol]))[i] < m_minDensity){
	    physical = false;
	    break;
	  }
	}
	for(CFuint i = m_nbSpecies+nbDims-1 ; i<m_nbrEqs; ++i){
	  if((*((*m_cellStates)[iSol]))[i] < m_minTemperature){
	    physical = false;
	    break;
	  }
	}
      }
    }
  }
  
  return physical;
  
}
//////////////////////////////////////////////////////////////////////////////

void PhysicalityEuler2D::enforcePhysicality()
{
  const bool Puvt = getMethodData().getUpdateVarStr() == "Puvt";
  const bool Cons = getMethodData().getUpdateVarStr() == "Cons";
  const bool RhoivtTv = getMethodData().getUpdateVarStr() == "RhoivtTv";
  bool needsLim = false;
  const CFuint nbDims = PhysicalModelStack::getActive()->getDim();

  m_cellAvgState =0.;
  for (CFuint iSol = 0; iSol < m_nbrSolPnts; ++iSol)
  {
    m_cellAvgState += (*m_cellAvgSolCoefs)[iSol]*(*(*m_cellStates)[iSol]);
  }
  if (Puvt)
  {
    if (m_cellAvgState[0] < m_minPressure || m_cellAvgState[3] < m_minTemperature) needsLim = true;
  }
  else if (Cons)
  {
    CFreal rho  = m_cellAvgState[0];
    
    if (rho < m_minDensity) 
    {
      needsLim = true;
    }
    else
    {
      CFreal rhoU = m_cellAvgState[1];
      CFreal rhoV = m_cellAvgState[2];
      CFreal rhoE = m_cellAvgState[3];
      CFreal press = m_gammaMinusOne*(rhoE - 0.5*(rhoU*rhoU+rhoV*rhoV)/rho);
      
      if (press < m_minPressure) needsLim = true;
    }
  }
  else if (RhoivtTv){
    for (CFuint i = 0 ; i<m_nbSpecies ; ++i){
      if(m_cellAvgState[i] < m_minDensity){
	needsLim = true;
      }
    }
    for(CFuint i = m_nbSpecies+nbDims-1 ; i<m_nbrEqs; ++i){
      if(m_cellAvgState[i] < m_minTemperature){
	needsLim = true;
      }
    } 
  }
  
  if (needsLim)
  {    
    for (CFuint iSol = 0; iSol < m_nbrSolPnts; ++iSol)
    {
      for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
      {
	(*((*m_cellStates)[iSol]))[iEq] -= m_cellAvgState[iEq];
      }
    }
    
    if (Puvt)
    {
      if (m_cellAvgState[0] < m_minPressure)
      {
        m_cellAvgState[0] = m_minPressure;
      }
      if (m_cellAvgState[3] < m_minTemperature)
      {
	m_cellAvgState[3] = m_minTemperature;
      }
    }
    else if (Cons)
    {
      if (m_cellAvgState[0] < m_minDensity)
      {
        m_cellAvgState[0] = m_minDensity;
      }
      
      CFreal rho = m_cellAvgState[0];
      CFreal rhoU = m_cellAvgState[1];
      CFreal rhoV = m_cellAvgState[2];
      CFreal rhoE = m_cellAvgState[3];
      CFreal press = m_gammaMinusOne*(rhoE - 0.5*(rhoU*rhoU+rhoV*rhoV)/rho);
      
      if (press < m_minPressure)
      {
	m_cellAvgState[3] = m_minPressure/m_gammaMinusOne + 0.5*(rhoU*rhoU+rhoV*rhoV)/rho;
      }
    }
    else if (RhoivtTv){
      for (CFuint i = 0 ; i<m_nbSpecies ; ++i){
	if(m_cellAvgState[i] < m_minDensity){
	  m_cellAvgState[i] = m_minDensity;
	}
      }
      for(CFuint i = m_nbSpecies+nbDims-1 ; i<m_nbrEqs; ++i){
	if(m_cellAvgState[i] < m_minTemperature){
	  m_cellAvgState[i] = m_minTemperature;
	}
      }
    }


    
    for (CFuint iSol = 0; iSol < m_nbrSolPnts; ++iSol)
    {
      for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
      {
	(*((*m_cellStates)[iSol]))[iEq] += m_cellAvgState[iEq];
      }
    }
    
    // compute the states in the nodes
    computeFlxPntStates(m_cellStatesFlxPnt);
  }
  
  needsLim = false;
   RealVector needsLimFlags(m_nbrEqs);
  for (CFuint iFlx = 0; iFlx < m_maxNbrFlxPnts; ++iFlx)
  { 
    needsLimFlags = 0.;
    if (Puvt)
    {
      if (m_cellStatesFlxPnt[iFlx][0] < m_minPressure || m_cellStatesFlxPnt[iFlx][3] < m_minTemperature)
      {
	needsLim = true;
      }
    }
    else if (Cons)
    {
      CFreal rho  = m_cellStatesFlxPnt[iFlx][0];
      
      if (rho < m_minDensity)
      {
	needsLim = true;
      }
      else
      {
	CFreal rhoU = m_cellStatesFlxPnt[iFlx][1];
        CFreal rhoV = m_cellStatesFlxPnt[iFlx][2];
        CFreal rhoE = m_cellStatesFlxPnt[iFlx][3];
	CFreal press = m_gammaMinusOne*(rhoE - 0.5*(rhoU*rhoU+rhoV*rhoV)/rho);
	
	if (press < m_minPressure)
	{
	  needsLim = true;
	}
      }
    }
    else if (RhoivtTv){
      for (CFuint i = 0 ; i<m_nbSpecies ; ++i){
	if (m_cellStatesFlxPnt[iFlx][i] < m_minDensity){
	  needsLim = true;
	  needsLimFlags[i] = 1.;
	}	
      }
      for(CFuint i = m_nbSpecies+nbDims-1 ; i<m_nbrEqs; ++i){
	if(m_cellStatesFlxPnt[iFlx][i] <  m_minTemperature ){
	  needsLim = true;
	  needsLimFlags[i] = 1.;
	}	
      }
      
    }
    
    if (needsLim)
    {
      //CFLog(NOTICE, "Limiting pressure in cell " << m_cell->getID() << "\n");
      CFreal phi = 1.0;
      
      for (CFuint iScale = 0; iScale < 10; ++iScale)
      {
        phi /= 2.0;
        for (CFuint iSol = 0; iSol < m_nbrSolPnts; ++iSol)
        {
	  for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
	  {
	    if( needsLimFlags[iEq] == 1. || !RhoivtTv){
  	    (*((*m_cellStates)[iSol]))[iEq] = (1.0-phi)*m_cellAvgState[iEq] + phi*((*((*m_cellStates)[iSol]))[iEq]);
	    }
	  }
        }
        
        // compute the states in the nodes
        computeFlxPntStates(m_cellStatesFlxPnt);
	
	needsLim = false;
	
	if (Puvt)
        {
          if (m_cellStatesFlxPnt[iFlx][0] < m_minPressure || m_cellStatesFlxPnt[iFlx][3] < m_minTemperature)
          {
	    needsLim = true;
          }
        }
        else if (Cons)
        {
          CFreal rho  = m_cellStatesFlxPnt[iFlx][0];
      
          if (rho < m_minDensity)
          {
	    needsLim = true;
          }
          else
          {
	    CFreal rhoU = m_cellStatesFlxPnt[iFlx][1];
            CFreal rhoV = m_cellStatesFlxPnt[iFlx][2];
            CFreal rhoE = m_cellStatesFlxPnt[iFlx][3];
	    CFreal press = m_gammaMinusOne*(rhoE - 0.5*(rhoU*rhoU+rhoV*rhoV)/rho);
	
	    if (press < m_minPressure)
	    {
	      needsLim = true;
	    }
          }
        }
	else if (RhoivtTv){
	  for (CFuint i = 0 ; i<m_nbSpecies ; ++i){
	    if (m_cellStatesFlxPnt[iFlx][i] < m_minDensity){
	      needsLim = true;
	      needsLimFlags[i] = 1.;
	    }	
	  }
	  for(CFuint i = m_nbSpecies+nbDims-1 ; i<m_nbrEqs; ++i){
	    if(m_cellStatesFlxPnt[iFlx][i] <  m_minTemperature ){
	      needsLim = true;
	      needsLimFlags[i] = 1.;
	    }	
	  } 
	}
	if(!needsLim) {
	  break;
	}
      }
      
      if (needsLim)
	{
	  for (CFuint iSol = 0; iSol < m_nbrSolPnts; ++iSol)
	    {
	      for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
		{
		  (*((*m_cellStates)[iSol]))[iEq] = m_cellAvgState[iEq];
		}
	    }
	}
    }
  }
  
  if (m_checkInternal)
  {
    needsLim = false;
    needsLimFlags = 0.;
    for (CFuint iSol = 0; iSol < m_nbrSolPnts; ++iSol)
    { 
      if (Puvt)
      {
        if ((*((*m_cellStates)[iSol]))[0] < m_minPressure || (*((*m_cellStates)[iSol]))[3] < m_minTemperature)
        {
	  needsLim = true;
        }
      }
      else if (Cons)
      {
        CFreal rho  = (*((*m_cellStates)[iSol]))[0];
      
        if (rho < m_minDensity)
        {
	  needsLim = true;
        }
        else
        {
	  CFreal rhoU = (*((*m_cellStates)[iSol]))[1];
          CFreal rhoV = (*((*m_cellStates)[iSol]))[2];
          CFreal rhoE = (*((*m_cellStates)[iSol]))[3];
	  CFreal press = m_gammaMinusOne*(rhoE - 0.5*(rhoU*rhoU+rhoV*rhoV)/rho);
	
	  if (press < m_minPressure)
	  {
	    needsLim = true;
	  }
        }
      }
      else if (RhoivtTv){
	for (CFuint i = 0 ; i<m_nbSpecies ; ++i){
	  if ((*((*m_cellStates)[iSol]))[i] < m_minDensity){
	    needsLim = true;
	    needsLimFlags[i] = 1.;
	  }	
	}
	for(CFuint i = m_nbSpecies+nbDims-1 ; i<m_nbrEqs; ++i){
	  if((*((*m_cellStates)[iSol]))[i] <  m_minTemperature ){
	    needsLim = true;
	    needsLimFlags[i] = 1.;
	  }	
	} 
      }
      
      if (needsLim)
      {
        //CFLog(NOTICE, "Limiting pressure in cell " << m_cell->getID() << "\n");
	CFreal phi = 1.0;
	
        for (CFuint iScale = 0; iScale < 10; ++iScale)
        {
          phi /= 2.0;
          for (CFuint jSol = 0; jSol < m_nbrSolPnts; ++jSol)
          {
	    for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
	    {
	      if( needsLimFlags[iEq] == 1. || !RhoivtTv){

	      (*((*m_cellStates)[jSol]))[iEq] = (1.0-phi)*m_cellAvgState[iEq] + phi*(*((*m_cellStates)[jSol]))[iEq];
	      }
	    }
          }
          
          needsLim = false;
	
	  if (Puvt)
          {
            if ((*((*m_cellStates)[iSol]))[0] < m_minPressure || (*((*m_cellStates)[iSol]))[3] < m_minTemperature)
            {
	      needsLim = true;
            }
          }
          else if (Cons)
          {
            CFreal rho  = (*((*m_cellStates)[iSol]))[0];
        
            if (rho < m_minDensity)
            {
	      needsLim = true;
            }
            else
            {
	      CFreal rhoU = (*((*m_cellStates)[iSol]))[1];
              CFreal rhoV = (*((*m_cellStates)[iSol]))[2];
              CFreal rhoE = (*((*m_cellStates)[iSol]))[3];
	      CFreal press = m_gammaMinusOne*(rhoE - 0.5*(rhoU*rhoU+rhoV*rhoV)/rho);
	
	      if (press < m_minPressure)
	      {
	        needsLim = true;
	      }
            }
          }
	  else if (RhoivtTv){
	    for (CFuint i = 0 ; i<m_nbSpecies ; ++i){
	      if ((*((*m_cellStates)[iSol]))[i] < m_minDensity){
		needsLim = true;
		needsLimFlags[i] = 1.;
	      }	
	    }
	    for(CFuint i = m_nbSpecies+nbDims-1 ; i<m_nbrEqs; ++i){
	      if((*((*m_cellStates)[iSol]))[i] <  m_minTemperature ){
		needsLim = true;
		needsLimFlags[i] = 1.;
	      }	
	    } 
	  }
	  
	  if(!needsLim)
	  {
	    break;
	  }
        }
        
        if (needsLim)
        {
	  for (CFuint jSol = 0; jSol < m_nbrSolPnts; ++jSol)
          {
	    for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
	    {
  	      (*((*m_cellStates)[jSol]))[iEq] = m_cellAvgState[iEq];
	    }
          }
        }
      }
    }
  }
  
//   // only needed to plot the physicality check!!!!!
//   for (CFuint iSol = 0; iSol < m_nbrSolPnts; ++iSol)
//   {
//     (*((*m_cellStates)[iSol]))[0] = 10.0;
//   }
}

//////////////////////////////////////////////////////////////////////////////

void PhysicalityEuler2D::setup()
{
  CFAUTOTRACE;

  // setup parent class
  BasePhysicality::setup();
  
  // get the local FR data
  vector< FluxReconstructionElementData* >& frLocalData = getMethodData().getFRLocalData();
  const bool RhoivtTv = getMethodData().getUpdateVarStr() == "RhoivtTv";

  m_cellAvgSolCoefs = frLocalData[0]->getCellAvgSolCoefs();
  
  // get Euler 2D varset
if(!RhoivtTv){
  m_eulerVarSet = getMethodData().getUpdateVar().d_castTo<Euler2DVarSet>();
  if (m_eulerVarSet.isNull())
  {
    throw Common::ShouldNotBeHereException (FromHere(),"Update variable set is not Euler2DVarSet in PhysicalityEuler2DFluxReconstruction!");
  }
  
  // get gamma-1
  m_gammaMinusOne = m_eulerVarSet->getModel()->getGamma()-1.0;
  
  m_eulerVarSet->getModel()->resizePhysicalData(m_solPhysData);
}
else{
  m_eulerVarSetMS = PhysicalModelStack::getActive()-> getImplementor()->getConvectiveTerm().d_castTo< MultiScalarTerm< EulerTerm > >();
  if (m_eulerVarSetMS.isNull())
  {
    throw Common::ShouldNotBeHereException (FromHere(),"Update variable set is not Euler2DVarSetMS in PhysicalityEuler2DFluxReconstruction!");
  }

  m_nbSpecies = m_eulerVarSetMS->getNbScalarVars(0);
}

  m_cellAvgState.resize(m_nbrEqs);
}

//////////////////////////////////////////////////////////////////////////////

void PhysicalityEuler2D::unsetup()
{
  CFAUTOTRACE;

  // unsetup parent class
  BasePhysicality::unsetup();
}

//////////////////////////////////////////////////////////////////////////////

  } // namespace FluxReconstructionMethod

} // namespace COOLFluiD