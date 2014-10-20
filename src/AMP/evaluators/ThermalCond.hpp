//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef THERMALCOND_HPP
#define THERMALCOND_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

//------------------------------------------------------------------------------------------
#include "Teuchos_ParameterList.hpp"
#include "Epetra_Vector.h"
#include "Sacado_ParameterAccessor.hpp"
#include "Stokhos_KL_ExponentialRandomField.hpp"
#include "Teuchos_Array.hpp"
//------------------------------------------------------------------------------------------

namespace AMP {
///
/// \brief Thermal Conductivity
///
/// This evaluator computes the thermal conductivity
/// for a phase-change/heat equation problem
///
template<typename EvalT, typename Traits>
class ThermalCond : 
  public PHX::EvaluatorWithBaseImpl<Traits>,
  public PHX::EvaluatorDerived<EvalT, Traits>
{

public:

  ThermalCond(Teuchos::ParameterList& p,
      const Teuchos::RCP<Albany::Layouts>& dl);

  void 
  postRegistrationSetup(typename Traits::SetupData d,
      PHX::FieldManager<Traits>& vm);

  void 
  evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> coord_;

  PHX::MDField<ScalarT,Cell,Node> k_;

  unsigned int num_qps_;
  unsigned int num_dims_;
  unsigned int num_nodes_;
  unsigned int workset_size_;

  //! Constant value
  ScalarT constant_value;

  Teuchos::RCP<const Teuchos::ParameterList>
    getValidThermalCondParameters() const;

//! Convenience function to initialize constant Thermal conductivity
  void init_constant(ScalarT value, Teuchos::ParameterList& p);

  
};
}

#endif
