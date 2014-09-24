#include "Maxwell/Maxwell.hh"
#include "MaxwellProjection.hh"
#include "Environment/ObjectProvider.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace COOLFluiD::Framework;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Physics {

    namespace Maxwell {

//////////////////////////////////////////////////////////////////////////////

Environment::ObjectProvider<MaxwellProjection<DIM_2D>, PhysicalModelImpl, MaxwellModule,1>
Maxwell2DProjectionModelProvider("Maxwell2DProjection");

Environment::ObjectProvider<MaxwellProjection<DIM_3D>, PhysicalModelImpl, MaxwellModule,1>
Maxwell3DProjectionModelProvider("Maxwell3DProjection");

//////////////////////////////////////////////////////////////////////////////

} // namespace Maxwell

} // namespace Physics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////
