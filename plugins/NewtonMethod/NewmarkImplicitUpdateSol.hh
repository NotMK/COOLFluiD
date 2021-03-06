// Copyright (C) 2012 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

// Copyright (C) 2012 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef COOLFluiD_Numerics_NewtonMethod_NewmarkImplicitUpdateSol_hh
#define COOLFluiD_Numerics_NewtonMethod_NewmarkImplicitUpdateSol_hh

//////////////////////////////////////////////////////////////////////////////

#include "NewtonIteratorData.hh"
#include "Framework/DataSocketSink.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace NewtonMethod {

//////////////////////////////////////////////////////////////////////////////

  /// This class represents a NumericalCommand action to be
  /// sent to Domain to be executed in order to setup the MeshData.
  /// @author Thomas Wuilbaut

class NewmarkImplicitUpdateSol : public NewtonIteratorCom {
public:

  /// Defines the Config Option's of this class
  /// @param options a OptionList where to add the Option's
  static void defineConfigOptions(Config::OptionList& options);

  /// Constructor.
  explicit NewmarkImplicitUpdateSol(const std::string& name);

  /// Destructor.
  ~NewmarkImplicitUpdateSol()
  {
  }

  /// Execute Processing actions
  void execute();

  /// Returns the DataSocket's that this command needs as sinks
  /// @return a vector of SafePtr with the DataSockets
  virtual std::vector<Common::SafePtr<Framework::BaseDataSocketSink> > needsSockets();

protected:

  /// socket for Rhs
  Framework::DataSocketSink<
                              CFreal> socket_rhs;

  /// socket for updateCoeff
  Framework::DataSocketSink<
                              CFreal> socket_updateCoeff;

  /// socket for PastStates's
  Framework::DataSocketSink<
                              Framework::State*> socket_pastStates;

  /// socket for PastStates's
  Framework::DataSocketSink<
                              Framework::State*> socket_pastStatesD;

  /// socket for PastStates's
  Framework::DataSocketSink<
                              Framework::State*> socket_pastStatesD2;

  // handle to states
  Framework::DataSocketSink < Framework::State* , Framework::GLOBAL > socket_states;

  /// parameter relaxing convergence
  CFreal _alpha;

  /// parameter relaxing convergence
  CFreal _gamma;

}; // class NewmarkImplicitUpdateSol

//////////////////////////////////////////////////////////////////////////////

    } // namespace NewtonMethod

  } // namespace Numerics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_Numerics_NewtonMethod_NewmarkImplicitUpdateSol_hh
