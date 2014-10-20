//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef PHASEPROBLEM_HPP
#define PHASEPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "Albany_ProblemUtils.hpp"

#include "QCAD_MaterialDatabase.hpp"

namespace Albany {

///
/// \brief Definition for the Phase problem
///
class PhaseProblem : public AbstractProblem
{
public:
 
  PhaseProblem(const Teuchos::RCP<Teuchos::ParameterList>& params,
	       const Teuchos::RCP<ParamLib>& param_lib,
	       const int num_dims,
 	       Teuchos::RCP<const Teuchos::Comm<int> >& commT);

// 	       Teuchos::RCP<const Teuchos::Comm<int> >& commT);
//	       const Teuchos::RCP<const Epetra_Comm>& comm_); // DJL changed the agrument from Epetra_Comm to Teuchos::Comm 
                                                              // to match changes made to other Problem classes

  ~PhaseProblem();

  virtual 
  int spatialDimension() const { return num_dims_; }

  virtual
  void buildProblem(
      Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
      StateManager& stateMgr);

  virtual 
  Teuchos::Array< Teuchos::RCP<const PHX::FieldTag> >
  buildEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

  Teuchos::RCP<const Teuchos::ParameterList> 
  getValidProblemParameters() const;

private:

  PhaseProblem(const PhaseProblem&);
    
  PhaseProblem& operator=(const PhaseProblem&);

public:

  template <typename EvalT> 
  Teuchos::RCP<const PHX::FieldTag>
  constructEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

  void constructDirichletEvaluators(
      const std::vector<std::string>& nodeSetIDs);
    
  void constructNeumannEvaluators(
      const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs);

protected:

  int num_dims_;

  Teuchos::RCP<QCAD::MaterialDatabase> material_db_;
 

  Teuchos::RCP<Albany::Layouts> dl_;

};

}

//******************************************************************************

#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Shards_CellTopology.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "Albany_ResponseUtilities.hpp"

#include "RhoCp.hpp"
#include "ThermalCond.hpp"
#include "PhaseSource.hpp"
#include "PhaseResidual.hpp"

template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
Albany::PhaseProblem::constructEvaluators(
  PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
  const Albany::MeshSpecsStruct& meshSpecs,
  Albany::StateManager& stateMgr,
  Albany::FieldManagerChoice fieldManagerChoice,
  const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::ParameterList;
  using PHX::DataLayout;
  using PHX::MDALayout;
  using std::vector;
  using std::string;
  using PHAL::AlbanyTraits;

  const CellTopologyData* const elem_top = &meshSpecs.ctd;

  std::string eb_name = meshSpecs.ebName;
 
  RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
    intrepid_basis = Albany::getIntrepidBasis(*elem_top);

  RCP<shards::CellTopology> elem_type = 
    rcp(new shards::CellTopology (elem_top));

  Intrepid::DefaultCubatureFactory<RealType> cub_factory;

  RCP <Intrepid::Cubature<RealType> > elem_cubature = 
    cub_factory.create(*elem_type, meshSpecs.cubatureDegree);

  const int workset_size = meshSpecs.worksetSize;
  const int num_vertices = elem_type->getNodeCount();
  const int num_nodes = intrepid_basis->getCardinality();
  const int num_qps = elem_cubature->getNumPoints();

  *out << "Field Dimensions: Workset=" << workset_size 
       << ", Vertices= "               << num_vertices
       << ", Nodes= "                  << num_nodes
       << ", QuadPts= "                << num_qps
       << ", Dim= "                    << num_dims_ 
       << std::endl;

  dl_ = rcp(new Albany::Layouts(
        workset_size,num_vertices,num_nodes,num_qps,num_dims_));

  Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl_);

  Teuchos::RCP<PHX::Evaluator<AlbanyTraits> > ev;

  Teuchos::ArrayRCP<string> dof_names(1);
  Teuchos::ArrayRCP<string> dof_names_dot(1);
  Teuchos::ArrayRCP<string> resid_names(1);

  dof_names[0] = "T";
  dof_names_dot[0] = "T_dot";
  resid_names[0] = "T Residual";

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructGatherSolutionEvaluator(
      false,dof_names,dof_names_dot));

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructScatterResidualEvaluator(
      false,resid_names));

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructDOFInterpolationEvaluator(dof_names_dot[0]));

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructGatherCoordinateVectorEvaluator());

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructMapToPhysicalFrameEvaluator(
      elem_type,elem_cubature));

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructComputeBasisFunctionsEvaluator(
      elem_type,intrepid_basis,elem_cubature));


  { // Thermal Conductivity
    RCP<ParameterList> p = rcp(new ParameterList("Thermal Conductivity"));

    Teuchos::ParameterList& param_list =
      material_db_->getElementBlockSublist(eb_name, "Thermal Conductivity");    

    //Input
    p->set<string>("Coordinate Name","Coord Vec");
    p->set<Teuchos::ParameterList*>("Parameter List", &param_list);

    //Output
    p->set<string>("Thermal Conductivity Name", "k");

    ev = rcp(new AMP::ThermalCond<EvalT,AlbanyTraits>(*p,dl_));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Rho Cp
    RCP<ParameterList> p = rcp(new ParameterList("Specific Heat"));

    Teuchos::ParameterList& param_list =
      material_db_->getElementBlockSublist(eb_name, "Rho Cp");    

    //Input
    p->set<string>("Coordinate Name","Coord Vec");
    p->set<Teuchos::ParameterList*>("Parameter List", &param_list);

    //Output
    p->set<string>("Rho Cp Name", "Rho Cp");

    ev = rcp(new AMP::RhoCp<EvalT,AlbanyTraits>(*p,dl_));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Source Function
    RCP<ParameterList> p = rcp(new ParameterList("Source Function"));

    Teuchos::ParameterList& param_list =
      material_db_->getElementBlockSublist(eb_name, "Source"); 

    //Input
    p->set<string>("Coordinate Name","Coord Vec");
    p->set<Teuchos::ParameterList*>("Parameter List", &param_list);

    //Output
    p->set<string>("Source Name", "Source");
    
    ev = rcp(new AMP::PhaseSource<EvalT,AlbanyTraits>(*p,dl_));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Phase Residual
    RCP<ParameterList> p = rcp(new ParameterList("u Resid"));

    //Input
    p->set<string>("Weighted BF Name","wBF");
    p->set<string>("Weighted Gradient BF Name","wGrad BF");
    p->set<string>("Temperature Name","T");
    p->set<string>("Temperature Gradient Name","T Gradient");
    p->set<string>("Temperature Time Derivative Name","T_dot");
    p->set<string>("Thermal Conductivity Name","k");
    p->set<string>("Rho Cp Name","Rho Cp");
    p->set<string>("Source Name","Source");

    //Output
    p->set<string>("Residual Name", "T Residual");

    ev = rcp(new AMP::PhaseResidual<EvalT,AlbanyTraits>(*p,dl_));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
    PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter", dl_->dummy);
    fm0.requireField<EvalT>(res_tag);
    return res_tag.clone();
  }

  else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM) {
    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl_);
    return respUtils.constructResponses(fm0, *responseList, Teuchos::null, stateMgr);
  }

  return Teuchos::null;
}

#endif
