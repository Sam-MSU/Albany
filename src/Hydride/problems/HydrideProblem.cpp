/********************************************************************\
*            Albany, Copyright (2012) Sandia Corporation             *
*                                                                    *
* Notice: This computer software was prepared by Sandia Corporation, *
* hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
* the Department of Energy (DOE). All rights in the computer software*
* are reserved by DOE on behalf of the United States Government and  *
* the Contractor as provided in the Contract. You are authorized to  *
* use this computer software for Governmental purposes but it is not *
* to be released or distributed to the public. NEITHER THE GOVERNMENT*
* NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
* ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Glen Hansen, gahanse@sandia.gov                  *
\********************************************************************/


#include "HydrideProblem.hpp"
#include "Albany_InitialCondition.hpp"

#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Shards_CellTopology.hpp"
#include "PHAL_FactoryTraits.hpp"
#include "Albany_Utils.hpp"


Albany::HydrideProblem::
HydrideProblem( const Teuchos::RCP<Teuchos::ParameterList>& params_,
             const Teuchos::RCP<ParamLib>& paramLib_,
             const int numDim_,
             const Teuchos::RCP<const Epetra_Comm>& comm_) :
  Albany::AbstractProblem(params_, paramLib_),
  numDim(numDim_),
  haveNoise(false),
  comm(comm_)
{

  // Compute number of equations
  int num_eq = 0;
  num_eq += numDim; // one displacement equation per dimension
  num_eq += 1; // the equation for concentration c
  num_eq += 1; // the equation for chemical potential difference w
  this->setNumEquations(num_eq);

}

Albany::HydrideProblem::
~HydrideProblem()
{
}

void
Albany::HydrideProblem::
buildProblem(
  Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
  Albany::StateManager& stateMgr)
{
  /* Construct All Phalanx Evaluators */
  TEUCHOS_TEST_FOR_EXCEPTION(meshSpecs.size()!=1,std::logic_error,"Problem supports one Material Block");

  fm.resize(1);
  fm[0]  = Teuchos::rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);
  buildEvaluators(*fm[0], *meshSpecs[0], stateMgr, BUILD_RESID_FM, 
		  Teuchos::null);

  if(meshSpecs[0]->nsNames.size() > 0) // Build a nodeset evaluator if nodesets are present

    constructDirichletEvaluators(meshSpecs[0]->nsNames);

  if(meshSpecs[0]->ssNames.size() > 0) // Build a sideset evaluator if sidesets are present

    constructNeumannEvaluators(meshSpecs[0]);


}

Teuchos::Array<Teuchos::RCP<const PHX::FieldTag> >
Albany::HydrideProblem::
buildEvaluators(
  PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
  const Albany::MeshSpecsStruct& meshSpecs,
  Albany::StateManager& stateMgr,
  Albany::FieldManagerChoice fmchoice,
  const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  // Call constructEvaluators<EvalT>(*rfm[0], *meshSpecs[0], stateMgr);
  // for each EvalT in PHAL::AlbanyTraits::BEvalTypes
  ConstructEvaluatorsOp<HydrideProblem> op(
    *this, fm0, meshSpecs, stateMgr, fmchoice, responseList);
  boost::mpl::for_each<PHAL::AlbanyTraits::BEvalTypes>(op);
  return *op.tags;
}

// Dirichlet BCs
void
Albany::HydrideProblem::constructDirichletEvaluators(const std::vector<std::string>& nodeSetIDs)
{
   // Construct BC evaluators for all node sets and names
   std::vector<string> bcNames(neq);
   bcNames[0] = "X";
   if (numDim>1) bcNames[1] = "Y";
   if (numDim>2) bcNames[2] = "Z";
   bcNames[numDim] = "c";
   bcNames[numDim+1] = "w";

   Albany::BCUtils<Albany::DirichletTraits> bcUtils;
   dfm = bcUtils.constructBCEvaluators(nodeSetIDs, bcNames,
                                          this->params, this->paramLib);
}

// Neumann BCs
void
Albany::HydrideProblem::constructNeumannEvaluators(
        const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs)
{
   // Note: we only enter this function if sidesets are defined in the mesh file
   // i.e. meshSpecs.ssNames.size() > 0

   Albany::BCUtils<Albany::NeumannTraits> neuUtils;

   // Check to make sure that Neumann BCs are given in the input file

   if(!neuUtils.haveBCSpecified(this->params))

      return;

   // Construct BC evaluators for all side sets and names
   // Note that the string index sets up the equation offset, so ordering is important
   std::vector<string> neumannNames(neq + 1);
   Teuchos::Array<Teuchos::Array<int> > offsets;
   offsets.resize(neq+1);

   neumannNames[0] = "Tx";
   offsets[0].resize(1);
   offsets[0][0] = 0;
   offsets[neq].resize(neq);
   offsets[neq][0] = 0;

   if (numDim>1){ 
      neumannNames[1] = "Ty";
      offsets[1].resize(1);
      offsets[1][0] = 1;
      offsets[neq][1] = 1;
   }

   if (numDim>2){
     neumannNames[2] = "Tz";
      offsets[2].resize(1);
      offsets[2][0] = 2;
      offsets[neq][2] = 2;
   }

   neumannNames[numDim] = "cFlux";
   offsets[numDim].resize(1);
   offsets[numDim][0] = numDim;
   offsets[neq][numDim] = numDim;

   neumannNames[numDim + 1] = "wFlux";
   offsets[numDim+1].resize(1);
   offsets[numDim+1][0] = numDim+1;
   offsets[neq][numDim+1] = numDim+1;

   neumannNames[neq] = "all";

   // Construct BC evaluators for all possible names of conditions
   // Should only specify flux vector components (dudx, dudy, dudz), or dudn, not both
   std::vector<string> condNames(3); //dudx, dudy, dudz, dudn, P
   Teuchos::ArrayRCP<string> dof_names(1);
     dof_names[0] = "Displacement";

   // Note that sidesets are only supported for two and 3D currently
   if(numDim == 2)
    condNames[0] = "(dudx, dudy)";
   else if(numDim == 3)
    condNames[0] = "(dudx, dudy, dudz)";
   else
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
       std::endl << "Error: Sidesets only supported in 2 and 3D." << std::endl);

   condNames[1] = "dudn";
   condNames[2] = "P";

   nfm.resize(1); // Elasticity problem only has one element block

   nfm[0] = neuUtils.constructBCEvaluators(meshSpecs, neumannNames, dof_names, true, 0,
                                          condNames, offsets, dl,
                                          this->params, this->paramLib);

}

Teuchos::RCP<const Teuchos::ParameterList>
Albany::HydrideProblem::getValidProblemParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getGenericProblemParams("ValidHydrideProblemParams");

  Teuchos::Array<int> defaultPeriod;

  validPL->set<double>("b", 0.0, "b value in equation 1.1");
  validPL->set<double>("gamma", 0.0, "gamma value in equation 2.2");
  validPL->set<double>("e", 0.0, "e value in equation between 1.2 and 1.3");
  validPL->set<double>("Langevin Noise SD", 0.0, "Standard deviation of the Langevin noise to apply");
  validPL->set<Teuchos::Array<int> >("Langevin Noise Time Period", defaultPeriod, 
    "Time period to apply Langevin noise");
  validPL->set<bool>("Lump Mass", true, "Lump mass matrix in time derivative term");

 validPL->sublist("Elastic Modulus", false, "");
  validPL->sublist("Poissons Ratio", false, "");


  return validPL;
}
