//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef FELIX_STOKESFOTHICKNESSPROBLEM_HPP
#define FELIX_STOKESFOTHICKNESSPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
//#include "PHAL_AlbanyTraits.hpp"
#include "PHAL_SaveStateField.hpp"
#include "PHAL_LoadStateField.hpp"
#include "FELIX_GatherThickness.hpp"
#include "FELIX_GatherVerticallyAveragedVelocity.hpp"

//uncomment the following line if you want debug output to be printed to screen
//#define OUTPUT_TO_SCREEN

namespace FELIX {

  /*!
   * \brief Abstract interface for representing a 1-D finite element
   * problem.
   */
  class StokesFOThickness : public Albany::AbstractProblem {
  public:
  
    //! Default constructor
    StokesFOThickness(const Teuchos::RCP<Teuchos::ParameterList>& params,
		 const Teuchos::RCP<ParamLib>& paramLib,
		 const int numDim_);

    //! Destructor
    ~StokesFOThickness();

    //! Return number of spatial dimensions
    virtual int spatialDimension() const { return numDim; }

    //! Build the PDE instantiations, boundary conditions, and initial solution
    virtual void buildProblem(
      Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
      Albany::StateManager& stateMgr);

    // Build evaluators
    virtual Teuchos::Array< Teuchos::RCP<const PHX::FieldTag> >
    buildEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    //! Each problem must generate it's list of valide parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

  private:

    //! Private to prohibit copying
    StokesFOThickness(const StokesFOThickness&);
    
    //! Private to prohibit copying
    StokesFOThickness& operator=(const StokesFOThickness&);

  public:

    //! Main problem setup routine. Not directly called, but indirectly by following functions
    template <typename EvalT> Teuchos::RCP<const PHX::FieldTag>
    constructEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    void constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs);
    void constructNeumannEvaluators(const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs);

  protected:
    int numDim;
    double gravity;  //gravity
    double rho;  //ice density
    double rho_w;  //water density
    Teuchos::RCP<Albany::Layouts> dl,dl_full;

  };

}

#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Shards_CellTopology.hpp"

#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "Albany_ResponseUtilities.hpp"

#include "FELIX_ThicknessResid.hpp"
#include "FELIX_StokesFOResid.hpp"
#include "FELIX_StokesFOThicknessResid.hpp"
#include "FELIX_ViscosityFO.hpp"
#ifdef CISM_HAS_FELIX
#include "FELIX_CismSurfaceGradFO.hpp"
#endif
#include "FELIX_StokesFOBodyForce.hpp"
#include "PHAL_Neumann.hpp"
#include "PHAL_Source.hpp"


template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
FELIX::StokesFOThickness::constructEvaluators(
  PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
  const Albany::MeshSpecsStruct& meshSpecs,
  Albany::StateManager& stateMgr,
  Albany::FieldManagerChoice fieldManagerChoice,
  const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
//  std::cout << __FILE__<<":"<<__LINE__<<std::endl;

  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::ParameterList;
  using PHX::DataLayout;
  using PHX::MDALayout;
  using std::vector;
  using std::string;
  using std::map;
  using PHAL::AlbanyTraits;
  
  RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
    intrepidBasis = Albany::getIntrepidBasis(meshSpecs.ctd);
  RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&meshSpecs.ctd));
  
  const int numNodes = intrepidBasis->getCardinality();
  const int worksetSize = meshSpecs.worksetSize;
  
  Intrepid::DefaultCubatureFactory<RealType> cubFactory;
  RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, meshSpecs.cubatureDegree);
  
  const int numQPts = cubature->getNumPoints();
  const int numVertices = cellType->getNodeCount();
  int vecDimFO = std::min((int)neq,(int)2);
  std::string elementBlockName = meshSpecs.ebName;

#ifdef OUTPUT_TO_SCREEN
  *out << "Field Dimensions: Workset=" << worksetSize 
       << ", Vertices= " << numVertices
       << ", Nodes= " << numNodes
       << ", QuadPts= " << numQPts
       << ", Dim= " << numDim 
       << ", vecDimFO= " << vecDimFO << std::endl;
#endif
  
   Albany::StateStruct::MeshFieldEntity entity;
   dl = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim, vecDimFO));
   Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);
   dl_full = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim, neq));
   Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils_full(dl_full);
   int offset=0;

   entity= Albany::StateStruct::ElemData;

   // Temporary variable used numerous times below
      RCP<PHX::Evaluator<AlbanyTraits> > ev;
   {
     std::string stateName("temperature");
     RCP<ParameterList> p = stateMgr.registerStateVariable(stateName, dl->cell_scalar2, elementBlockName,true, &entity);
     ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   {
     std::string stateName("flow_factor");
     RCP<ParameterList> p = stateMgr.registerStateVariable(stateName, dl->cell_scalar2, elementBlockName,true, &entity);
     ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }
   std::cout << __FILE__<<":"<<__LINE__<<std::endl;

   entity= Albany::StateStruct::NodalDataToElemNode;

   {
     std::string stateName("surface_height");
     RCP<ParameterList> p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity);
     ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }
#ifdef CISM_HAS_FELIX
   {
     std::string stateName("xgrad_surface_height"); //ds/dx which can be passed from CISM (defined at nodes)
     RCP<ParameterList> p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity);
     ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }
   {
     std::string stateName("ygrad_surface_height"); //ds/dy which can be passed from CISM (defined at nodes)
     RCP<ParameterList> p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity);
     ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }
#endif
   {
     std::string stateName("thickness");
     RCP<ParameterList> p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity);
     ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }
  // std::cout << __FILE__<<":"<<__LINE__<<std::endl;

   {
     std::string stateName("basal_friction");
     RCP<ParameterList> p;

     //Finding whether basal friction is a distributed parameter
     bool isStateAParameter(false);
     const std::string* meshPart;
     const std::string emptyString("");
     if(this->params->isSublist("Distributed Parameters")) {
       Teuchos::ParameterList& dist_params_list =  this->params->sublist("Distributed Parameters");
       Teuchos::ParameterList* param_list;
       int numParams = dist_params_list.get<int>("Number of Parameter Vectors",0);
       for(int p_index=0; p_index< numParams; ++p_index) {
         std::string parameter_sublist_name = Albany::strint("Distributed Parameter", p_index);
         if(dist_params_list.isSublist(parameter_sublist_name)) {
           param_list = &dist_params_list.sublist(parameter_sublist_name);
           if(param_list->get<std::string>("Name", emptyString) == stateName) {
             meshPart = &param_list->get<std::string>("Mesh Part",emptyString);
             isStateAParameter = true;
             break;
           }
         } else {
           if(stateName == dist_params_list.get(Albany::strint("Parameter", p_index), emptyString)) {
             isStateAParameter = true;
             meshPart = &emptyString;
             break;
           }
         }
       }
     }

     if(isStateAParameter) { //basal friction is a distributed parameter
       entity= Albany::StateStruct::NodalDistParameter;
       p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity, *meshPart);
       fm0.template registerEvaluator<EvalT>
           (evalUtils.constructGatherScalarNodalParameter(stateName));
       std::stringstream key; key << stateName <<  "Is Distributed Parameter";
       this->params->set<int>(key.str(), 1);
     } else {
       entity= Albany::StateStruct::NodalDataToElemNode;
       p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity);
       ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
     }
   }
  // std::cout << __FILE__<<":"<<__LINE__<<std::endl;


#if defined(CISM_HAS_FELIX) || defined(MPAS_HAS_FELIX)
   {
    // Here is how to register the field for dirichlet condition.
    std::string stateName("dirichlet_field");
    entity= Albany::StateStruct::NodalDistParameter;
    stateMgr.registerStateVariable(stateName, dl->node_vector, elementBlockName, true, &entity, "");
   }
#endif

  // std::cout << __FILE__<<":"<<__LINE__<<std::endl;

   // Define Field Names

  Teuchos::ArrayRCP<std::string> dof_names(1);
  Teuchos::ArrayRCP<std::string> dof_names_dot(1);
  Teuchos::ArrayRCP<std::string> resid_names(1);
  {

    dof_names[0] = "Velocity";
    resid_names[0] = "StokesFO Residual";
    fm0.template registerEvaluator<EvalT>
    (evalUtils.constructGatherSolutionEvaluator_noTransient(true, dof_names, offset));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFVecInterpolationEvaluator(dof_names[0], offset));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFVecGradInterpolationEvaluator(dof_names[0], offset));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructScatterResidualEvaluator(true, resid_names,offset, "Scatter StokesFO"));
    offset += vecDimFO;
  }

  {

      dof_names[0] = "U";
      resid_names[0] = "ProlongateH Residual";
      fm0.template registerEvaluator<EvalT>
      (evalUtils_full.constructGatherSolutionEvaluator_noTransient(true, dof_names, 0));

      fm0.template registerEvaluator<EvalT>
        (evalUtils_full.constructDOFVecGradInterpolationEvaluator(dof_names[0], 0));

      fm0.template registerEvaluator<EvalT>
        (evalUtils_full.constructScatterResidualEvaluator(true, resid_names,0, "Scatter ProlongateH"));
    }

  /*
  {
    dof_names[0] = "Thickness";
    resid_names[0] = "Thickness Residual";
    fm0.template registerEvaluator<EvalT>
    (evalUtils.constructGatherSolutionEvaluator_noTransient(false, dof_names, offset));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFInterpolationEvaluator(dof_names[0], offset));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0], offset));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructScatterResidualEvaluator(false, resid_names,offset, "Scatter Thickness"));
    offset ++;
  }*/

  {
    dof_names[0] = "Thickness";
   resid_names[0] = "Thickness Residual";


   {
     RCP<ParameterList> p = rcp(new ParameterList("Gather Thickness"));
     p->set<string>("Thickness Name", dof_names[0]);
     p->set<int>("Offset of First DOF", offset);
     ev = rcp(new GatherThickness<EvalT,AlbanyTraits>(*p,dl));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   {
      RCP<ParameterList> p = rcp(new ParameterList("Gather Averaged Velocity"));
      p->set<string>("Averaged Velocity Name", "Averaged Velocity");
      ev = rcp(new GatherVerticallyAveragedVelocity<EvalT,AlbanyTraits>(*p,dl));
      fm0.template registerEvaluator<EvalT>(ev);
   }



   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructScatterResidualEvaluator(false, resid_names,offset, "Scatter Thickness"));
   offset ++;
   }




  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructGatherCoordinateVectorEvaluator());

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructMapToPhysicalFrameEvaluator(cellType, cubature));

  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructComputeBasisFunctionsEvaluator(cellType, intrepidBasis, cubature));

  std::string sh = "surface_height";
  fm0.template registerEvaluator<EvalT>
    (evalUtils.constructDOFGradInterpolationEvaluator_noDeriv(sh));

  std::string th = "thickness";
    fm0.template registerEvaluator<EvalT>
      (evalUtils_full.constructDOFGradInterpolationEvaluator_noDeriv(th));
  std::cout << __FILE__<<":"<<__LINE__<<std::endl;


  { // FO Stokes Resid
    RCP<ParameterList> p = rcp(new ParameterList("StokesFO Resid"));
   
    //Input
    p->set<std::string>("Weighted BF Name", "wBF");
    p->set<std::string>("Weighted Gradient BF Name", "wGrad BF");
    p->set<std::string>("QP Variable Name", "Velocity");
    p->set<std::string>("QP Time Derivative Variable Name", "Velocity_dot");
    p->set<std::string>("Gradient QP Variable Name", "Velocity Gradient");
    p->set<std::string>("Body Force Name", "Body Force");
    p->set<std::string>("FELIX Viscosity QP Variable Name", "FELIX Viscosity");
    
    Teuchos::ParameterList& paramList = params->sublist("Equation Set");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    //Output
    p->set<std::string>("Residual Name", "StokesFO Residual");

    ev = rcp(new FELIX::StokesFOResid<EvalT,AlbanyTraits>(*p, dl));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // FO Stokes Resid
    RCP<ParameterList> p = rcp(new ParameterList("StokesFO Resid"));

    //Input
    p->set<std::string>("H0 Gradient QP Name", "thickness Gradient");
    p->set<std::string>("Gradient QP Variable Name", "U Gradient");
    p->set<std::string>("Weighted Gradient BF Name", "wGrad BF");
    p->set<std::string>("Weighted BF Name", "wBF");

    Teuchos::ParameterList& paramList = params->sublist("Equation Set");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    //Output
    p->set<std::string>("Residual Name", "ProlongateH Residual");

    ev = rcp(new FELIX::StokesFOThicknessResid<EvalT,AlbanyTraits>(*p, dl_full));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // FO Stokes Resid
    RCP<ParameterList> p = rcp(new ParameterList("Thickness Resid"));

    //Input
    p->set<std::string>("Averaged Velocity Variable Name", "Averaged Velocity");
    p->set<std::string>("Thickness Variable Name", "Thickness");
    p->set<std::string>("Old Thickness Name", "thickness");

    Teuchos::ParameterList& paramList = params->sublist("Equation Set");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    RCP<const Albany::MeshSpecsStruct> meshSpecsPtr = Teuchos::rcpFromRef(meshSpecs);
    p->set<Teuchos::RCP<const Albany::MeshSpecsStruct> >("Mesh Specs Struct", meshSpecsPtr);
    p->set<double>("Time Step", this->params->get<double>("Time Step"));

    //Output
    p->set<std::string>("Residual Name", "Thickness Residual");


    ev = rcp(new FELIX::ThicknessResid<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);
  }
 // std::cout << __FILE__<<":"<<__LINE__<<std::endl;


  { // FELIX viscosity
    RCP<ParameterList> p = rcp(new ParameterList("FELIX Viscosity"));

    //Input
    p->set<std::string>("Coordinate Vector Name", "Coord Vec");
    p->set<std::string>("Gradient QP Variable Name", "Velocity Gradient");
    p->set<std::string>("temperature Name", "temperature");
    p->set<std::string>("flow_factor Name", "flow_factor");
    
    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("FELIX Viscosity");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);
  
    //Output
    p->set<std::string>("FELIX Viscosity QP Variable Name", "FELIX Viscosity");

    ev = rcp(new FELIX::ViscosityFO<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);
    
  }
  
  //std::cout << __FILE__<<":"<<__LINE__<<std::endl;


#ifdef CISM_HAS_FELIX
  { // FELIX surface gradient from CISM
    RCP<ParameterList> p = rcp(new ParameterList("FELIX Surface Gradient"));

    //Input
    p->set<std::string>("xgrad_surface_height Name", "xgrad_surface_height");
    p->set<std::string>("ygrad_surface_height Name", "ygrad_surface_height");
    p->set<std::string>("BF Name", "BF");
    
    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("FELIX Surface Gradient");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);
  
    //Output
    p->set<std::string>("FELIX Surface Gradient QP Name", "FELIX Surface Gradient");

    ev = rcp(new FELIX::CismSurfaceGradFO<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);
    
  }
#endif

  { // Body Force
    RCP<ParameterList> p = rcp(new ParameterList("Body Force"));

    //Input
    p->set<std::string>("FELIX Viscosity QP Variable Name", "FELIX Viscosity");
#ifdef CISM_HAS_FELIX
    p->set<std::string>("FELIX Surface Gradient QP Variable Name", "FELIX Surface Gradient");
#endif
    p->set<std::string>("Coordinate Vector Name", "Coord Vec");
    p->set<std::string>("surface_height Gradient Name", "surface_height Gradient");
    
    Teuchos::ParameterList& paramList = params->sublist("Body Force");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);
      
    Teuchos::ParameterList& physParamList = params->sublist("Physical Parameters"); 
    p->set<Teuchos::ParameterList*>("Physical Parameter List", &physParamList);
    
    //Output
    p->set<std::string>("Body Force Name", "Body Force");

    ev = rcp(new FELIX::StokesFOBodyForce<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  RCP<ParameterList> paramList = rcp(new ParameterList("Param List"));
  { // response
    RCP<const Albany::MeshSpecsStruct> meshSpecsPtr = Teuchos::rcpFromRef(meshSpecs);
    paramList->set<RCP<const Albany::MeshSpecsStruct> >("Mesh Specs Struct", meshSpecsPtr);
    paramList->set<RCP<ParamLib> >("Parameter Library", paramLib);
  }


  if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
    PHX::Tag<typename EvalT::ScalarT> resFO_tag("Scatter StokesFO", dl->dummy);
    fm0.requireField<EvalT>(resFO_tag);
    PHX::Tag<typename EvalT::ScalarT> resProH_tag("Scatter ProlongateH", dl_full->dummy);
    fm0.requireField<EvalT>(resProH_tag);
    PHX::Tag<typename EvalT::ScalarT> resThick_tag("Scatter Thickness", dl->dummy);
    fm0.requireField<EvalT>(resThick_tag);

  }
  else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM) {
    
    entity= Albany::StateStruct::NodalDataToElemNode;
 
    {
      std::string stateName("surface_velocity");
      RCP<ParameterList> p = stateMgr.registerStateVariable(stateName, dl->node_vector, elementBlockName,true,&entity);
      ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }

    {
      std::string stateName("surface_velocity_rms");
      RCP<ParameterList> p = stateMgr.registerStateVariable(stateName, dl->node_vector, elementBlockName,true,&entity);
      ev = rcp(new PHAL::LoadStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
     }

    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl);
    return respUtils.constructResponses(fm0, *responseList, paramList, stateMgr);
  }


  return Teuchos::null;
}
#endif // FELIX_STOKESFOPROBLEM_HPP
