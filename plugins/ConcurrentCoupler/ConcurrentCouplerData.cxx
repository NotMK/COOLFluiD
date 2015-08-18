#include "Framework/MethodCommandProvider.hh"
#include "Framework/PhysicalModel.hh"
#include "Framework/ConvectiveVarSet.hh"
#include "Framework/SubSystemStatus.hh"
#include "Framework/NamespaceSwitcher.hh"

#include "ConcurrentCoupler/ConcurrentCoupler.hh"
#include "ConcurrentCoupler/ConcurrentCouplerData.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace ConcurrentCoupler {

//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider<NullMethodCommand<ConcurrentCouplerData>, 
		      ConcurrentCouplerData, ConcurrentCouplerModule> 
nullCouplerComProvider("Null");

//////////////////////////////////////////////////////////////////////////////

void ConcurrentCouplerData::defineConfigOptions(Config::OptionList& options)
{  
}
      
//////////////////////////////////////////////////////////////////////////////

ConcurrentCouplerData::ConcurrentCouplerData(SafePtr<Method> owner) : 
  CouplerData(owner),
  _stdTrsGeoBuilder(),
  _faceTrsGeoBuilder(),
  _spaceMethod()
{
  //  addConfigOptionsTo(this);
}      
      
//////////////////////////////////////////////////////////////////////////////

ConcurrentCouplerData::~ConcurrentCouplerData()
{
}

//////////////////////////////////////////////////////////////////////////////

void ConcurrentCouplerData::configure ( Config::ConfigArgs& args )
{
  CFAUTOTRACE;
  
  CouplerData::configure(args);
}
      
//////////////////////////////////////////////////////////////////////////////

void ConcurrentCouplerData::setup()
{
  // set up the GeometricEntity builders
  _stdTrsGeoBuilder.setup();
  _faceTrsGeoBuilder.setup();
}
      
//////////////////////////////////////////////////////////////////////////////

    } // namespace SubSystem

  } // namespace Numerics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////