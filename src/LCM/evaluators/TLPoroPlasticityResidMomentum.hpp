//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef TLPOROPLASTICITYRESIDMOMENTUM_HPP
#define TLPOROPLASTICITYRESIDMOMENTUM_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

namespace LCM {
/** \brief

    This evaluator calculate residual of the mass balance equation
    for the poromechanics problem.

*/

template<typename EvalT, typename Traits>
class TLPoroPlasticityResidMomentum : public PHX::EvaluatorWithBaseImpl<Traits>,
		        public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  TLPoroPlasticityResidMomentum(const Teuchos::ParameterList& p);

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> TotalStress;
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> defgrad;
  PHX::MDField<ScalarT,Cell,QuadPoint> J;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> wGradBF;

  PHX::MDField<ScalarT,Cell,QuadPoint,Dim> uDotDot;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint> wBF;

  // Output:
  PHX::MDField<ScalarT,Cell,Node,Dim> ExResidual;

  std::size_t numNodes;
  std::size_t numQPs;
  std::size_t numDims;
  bool enableTransient;

  // Work space FCs
  Intrepid::FieldContainer<ScalarT> F_inv;
  Intrepid::FieldContainer<ScalarT> F_invT;
  Intrepid::FieldContainer<ScalarT> JF_invT;
  Intrepid::FieldContainer<ScalarT> P;

};
}

#endif
