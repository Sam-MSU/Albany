//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

//IK, 9/13/14: only Epetra is SG and MP

#ifndef QCAD_POISSONSOURCENEUMANN_HPP
#define QCAD_POISSONSOURCENEUMANN_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

#include "Albany_ProblemUtils.hpp"
#include "Sacado_ParameterAccessor.hpp"
#include "PHAL_AlbanyTraits.hpp"

#include "QCAD_MaterialDatabase.hpp"


namespace QCAD {

/** \brief Neumann boundary condition evaluator

*/


template<typename EvalT, typename Traits>
class PoissonSourceNeumannBase : 
    public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits>,
    public Sacado::ParameterAccessor<EvalT, SPL_Traits> {

public:

  enum SIDE_TYPE {OTHER, LINE, TRI, QUAD}; // to calculate areas for pressure bc

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  PoissonSourceNeumannBase(const Teuchos::ParameterList& p);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d) = 0;

  ScalarT& getValue(const std::string &n);

  //! Public Universal Constants
  /***** define universal constants as double constants *****/
  static const double kbBoltz; // Boltzmann constant in [eV/K]
  static const double eps0; // vacuum permittivity in [C/(V.cm)]
  static const double eleQ; // electron elemental charge in [C]
  static const double m0;   // vacuum electron mass in [kg]
  static const double hbar; // reduced planck constant in [J.s]
  static const double pi;   // pi constant (unitless)


protected:

  const Teuchos::RCP<Albany::Layouts>& dl;
  const Teuchos::RCP<const Albany::MeshSpecsStruct>& meshSpecs;

  int  cellDims,  numQPs, numNodes;
  Teuchos::Array<int> offset;
  int numDOFsSet;

  double energy_unit_in_eV;
  double temperature, V0;
  std::vector<double> prefactors;
  std::vector<ScalarT> phiOffsets;
  Teuchos::RCP<MaterialDatabase> materialDB;

  bool responseOnly; //flag for evaluator being called in response field manager

  //The following are for the basal BC 
  /*std::string betaName; //name of function betaXY to be used
  double L;           //length scale for ISMIP-HOM Test cases 
  MeshScalarT betaXY; //function of x and y to multiply scalar values of beta read from input file
  enum BETAXY_NAME {CONSTANT, EXPTRIG, ISMIP_HOM_TEST_C, ISMIP_HOM_TEST_D, CONFINEDSHELF, CIRCULARSHELF, DOMEUQ, SCALAR_FIELD, LATERAL_BACKPRESSURE, FELIX_XZ_MMS};
  BETAXY_NAME beta_type;*/
 
  //The following are for the lateral BC 
  /*double g; 
  double rho; 
  double rho_w;  */

 // Should only specify flux vector components (dudx, dudy, dudz), dudn, or pressure P

   // dudn for 2D Thomas-Fermi poisson source
  void calc_dudn_2DThomasFermi(Intrepid::FieldContainer<ScalarT> & qp_data_returned,
			       const Intrepid::FieldContainer<MeshScalarT>& phys_side_cub_points,
			       const Intrepid::FieldContainer<ScalarT>& dof_side,
			       const Intrepid::FieldContainer<MeshScalarT>& jacobian_side_refcell,
			       const shards::CellTopology & celltopo,
			       const int cellDims,
			       int local_side_id, int iSideset);

  ScalarT getReferencePotential();


   // Do the side integration
  void evaluateNeumannContribution(typename Traits::EvalData d);

  // Input:
  //! Coordinate vector at vertices
  PHX::MDField<MeshScalarT,Cell,Vertex,Dim> coordVec;
  PHX::MDField<ScalarT,Cell,Node> dof;
  PHX::MDField<ScalarT,Cell,Node,VecDim> dofVec;
  PHX::MDField<ScalarT,Cell,Node> beta_field;
  PHX::MDField<ScalarT,Cell,Node> thickness_field;
  PHX::MDField<ScalarT,Cell,Node> elevation_field;
  Teuchos::RCP<shards::CellTopology> cellType;
  Teuchos::ArrayRCP<Teuchos::RCP<shards::CellTopology> > sideType;
  Teuchos::RCP<Intrepid::Cubature<RealType> > cubatureCell;
  Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::Cubature<RealType> > > cubatureSide;

  // The basis
  Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > intrepidBasis;

  // Temporary FieldContainers
  Intrepid::FieldContainer<RealType> cubPointsSide;
  Intrepid::FieldContainer<RealType> refPointsSide;
  Intrepid::FieldContainer<RealType> cubWeightsSide;
  Intrepid::FieldContainer<MeshScalarT> physPointsSide;
  Intrepid::FieldContainer<MeshScalarT> jacobianSide;
  Intrepid::FieldContainer<MeshScalarT> jacobianSide_det;

  Intrepid::FieldContainer<MeshScalarT> physPointsCell;

  Intrepid::FieldContainer<MeshScalarT> weighted_measure;
  Intrepid::FieldContainer<RealType> basis_refPointsSide;
  Intrepid::FieldContainer<MeshScalarT> trans_basis_refPointsSide;
  Intrepid::FieldContainer<MeshScalarT> weighted_trans_basis_refPointsSide;

  Intrepid::FieldContainer<ScalarT> dofCell;
  Intrepid::FieldContainer<ScalarT> dofSide;

  Intrepid::FieldContainer<ScalarT> dofCellVec;
  Intrepid::FieldContainer<ScalarT> dofSideVec;
  
  Intrepid::FieldContainer<ScalarT> data;

  // Output:
  Intrepid::FieldContainer<ScalarT>   neumann;
  PHX::MDField<ScalarT,Cell,Node> surfaceElectronDensity; // electron density in [cm-2]

  Teuchos::Array<std::string> sideSetIDs;
  //Teuchos::Array<RealType> inputValues;
  //std::string inputConditions;
  //std::string name;

  Teuchos::Array<SIDE_TYPE> side_type;
};

template<typename EvalT, typename Traits> class PoissonSourceNeumann;

// **************************************************************
// **************************************************************
// * Specializations
// **************************************************************
// **************************************************************


// **************************************************************
// Residual 
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::Residual,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::Residual, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::Residual::ScalarT ScalarT;
};

// **************************************************************
// Jacobian
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::Jacobian,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::Jacobian, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::Jacobian::ScalarT ScalarT;
};

// **************************************************************
// Tangent
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::Tangent,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::Tangent, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::Tangent::ScalarT ScalarT;
};

// **************************************************************
// Distributed Parameter Derivative
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::DistParamDeriv,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::DistParamDeriv, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::DistParamDeriv::ScalarT ScalarT;
};

// **************************************************************
// Stochastic Galerkin Residual 
// **************************************************************
#ifdef ALBANY_SG_MP
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::SGResidual,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::SGResidual, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::SGResidual::ScalarT ScalarT;
};

// **************************************************************
// Stochastic Galerkin Jacobian
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::SGJacobian,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::SGJacobian, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::SGJacobian::ScalarT ScalarT;
};

// **************************************************************
// Stochastic Galerkin Tangent
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::SGTangent,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::SGTangent, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::SGTangent::ScalarT ScalarT;
};

// **************************************************************
// Multi-point Residual 
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::MPResidual,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::MPResidual, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::MPResidual::ScalarT ScalarT;
};

// **************************************************************
// Multi-point Jacobian
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::MPJacobian,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::MPJacobian, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::MPJacobian::ScalarT ScalarT;
};

// **************************************************************
// Multi-point Tangent
// **************************************************************
template<typename Traits>
class PoissonSourceNeumann<PHAL::AlbanyTraits::MPTangent,Traits>
  : public PoissonSourceNeumannBase<PHAL::AlbanyTraits::MPTangent, Traits>  {
public:
  PoissonSourceNeumann(Teuchos::ParameterList& p);
  void evaluateFields(typename Traits::EvalData d);
private:
  typedef typename PHAL::AlbanyTraits::MPTangent::ScalarT ScalarT;
};
#endif //ALBANY_SG_MP



}

#endif
