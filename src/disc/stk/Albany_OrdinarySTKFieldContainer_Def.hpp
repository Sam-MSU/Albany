//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <iostream>

#include "Albany_OrdinarySTKFieldContainer.hpp"

#include "Albany_Utils.hpp"
#include <stk_mesh/base/GetBuckets.hpp>

#ifdef ALBANY_SEACAS
#include <stk_io/IossBridge.hpp>
#endif

template<bool Interleaved>
Albany::OrdinarySTKFieldContainer<Interleaved>::OrdinarySTKFieldContainer(
   const Teuchos::RCP<Teuchos::ParameterList>& params_,
   stk::mesh::fem::FEMMetaData* metaData_,
   stk::mesh::BulkData* bulkData_,
   const int neq_, 
   const AbstractFieldContainer::FieldContainerRequirements& req,
   const int numDim_,
   const Teuchos::RCP<Albany::StateInfoStruct>& sis)
    : GenericSTKFieldContainer<Interleaved>(params_, metaData_, bulkData_, neq_, numDim_),
      buildSurfaceHeight(false),
      buildTemperature(false),
      buildBasalFriction(false)
{

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  if(std::find(req.begin(), req.end(), "Surface Height") != req.end()){
      buildSurfaceHeight = true;
  }

  if(std::find(req.begin(), req.end(), "Temperature") != req.end()){
      buildTemperature = true;
  }

  if(std::find(req.begin(), req.end(), "Basal Friction") != req.end()){
	  buildBasalFriction = true;
  }

  //Start STK stuff
  this->coordinates_field = & metaData_->declare_field< VFT >( "coordinates" );
  solution_field = & metaData_->declare_field< VFT >(
    params_->get<string>("Exodus Solution Name", "solution"));

#ifdef ALBANY_LCM
  residual_field = & metaData_->declare_field< VFT >(
    params_->get<string>("Exodus Residual Name", "residual"));
#endif

#ifdef ALBANY_FELIX
  if(buildSurfaceHeight)
    this->surfaceHeight_field = & metaData_->declare_field< SFT >("surface_height");
  if(buildTemperature)
    this->temperature_field = & metaData_->declare_field< SFT >("temperature");
  if(buildBasalFriction)
      this->basalFriction_field = & metaData_->declare_field< SFT >("basal_friction");
#endif

  stk::mesh::put_field( *this->coordinates_field , metaData_->node_rank() , metaData_->universal_part(), numDim_ );
  stk::mesh::put_field( *solution_field , metaData_->node_rank() , metaData_->universal_part(), neq_ );

#ifdef ALBANY_LCM
  stk::mesh::put_field( *residual_field , metaData_->node_rank() , metaData_->universal_part() , neq_ );
#endif

#ifdef ALBANY_FELIX
  if(buildSurfaceHeight)
    stk::mesh::put_field( *this->surfaceHeight_field , metaData_->node_rank() , metaData_->universal_part());
  if(buildTemperature)
    stk::mesh::put_field( *this->temperature_field , metaData_->element_rank() , metaData_->universal_part());
  if(buildBasalFriction)
      stk::mesh::put_field( *this->basalFriction_field , metaData_->node_rank() , metaData_->universal_part());//*metaData_->get_part("basalside","Mpas Interface"));
#endif
  
#ifdef ALBANY_SEACAS
  stk::io::set_field_role(*this->coordinates_field, Ioss::Field::MESH);
  stk::io::set_field_role(*solution_field, Ioss::Field::TRANSIENT);
#ifdef ALBANY_LCM
  stk::io::set_field_role(*residual_field, Ioss::Field::TRANSIENT);
#endif

#ifdef ALBANY_FELIX
  // ATTRIBUTE writes only once per file, but somehow did not work on restart.
  //stk::io::set_field_role(*surfaceHeight_field, Ioss::Field::ATTRIBUTE);
  if(buildSurfaceHeight)
     stk::io::set_field_role(*this->surfaceHeight_field, Ioss::Field::TRANSIENT);
  if(buildTemperature)
     stk::io::set_field_role(*this->temperature_field, Ioss::Field::TRANSIENT);
  if(buildBasalFriction)
       stk::io::set_field_role(*this->basalFriction_field, Ioss::Field::TRANSIENT);
#endif
#endif

  this->buildStateStructs(sis);

  initializeSTKAdaptation();

}

template<bool Interleaved>
Albany::OrdinarySTKFieldContainer<Interleaved>::~OrdinarySTKFieldContainer(){
}

template<bool Interleaved>
void Albany::OrdinarySTKFieldContainer<Interleaved>::initializeSTKAdaptation(){

  typedef typename AbstractSTKFieldContainer::IntScalarFieldType ISFT;

    this->proc_rank_field = & this->metaData->template declare_field< ISFT >( "proc_rank" );
    this->refine_field = & this->metaData->template declare_field< ISFT >( "refine_field" );
    // Processor rank field, a scalar
    stk::mesh::put_field( *this->proc_rank_field , this->metaData->element_rank() , this->metaData->universal_part());
    stk::mesh::put_field( *this->refine_field , this->metaData->element_rank() , this->metaData->universal_part());
#ifdef ALBANY_SEACAS
    stk::io::set_field_role(*this->proc_rank_field, Ioss::Field::MESH);
    stk::io::set_field_role(*this->refine_field, Ioss::Field::MESH);
#endif

}

template<bool Interleaved>
void Albany::OrdinarySTKFieldContainer<Interleaved>::fillSolnVector(Epetra_Vector &soln, 
       stk::mesh::Selector &sel, const Teuchos::RCP<Epetra_Map>& node_map){

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector all_elements;
  stk::mesh::get_buckets(sel, this->bulkData->buckets(this->metaData->node_rank()), all_elements);
  this->numNodes = node_map->NumMyElements(); // Needed for the getDOF function to work correctly
                                        // This is either numOwnedNodes or numOverlapNodes, depending on
                                        // which map is passed in

  for (stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket & bucket = **it;

    this->fillVectorHelper(soln, solution_field, node_map, bucket, 0);

  }

}

template<bool Interleaved>
void Albany::OrdinarySTKFieldContainer<Interleaved>::saveSolnVector(const Epetra_Vector &soln, 
       stk::mesh::Selector &sel, const Teuchos::RCP<Epetra_Map>& node_map){

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector all_elements;
  stk::mesh::get_buckets(sel, this->bulkData->buckets(this->metaData->node_rank()), all_elements);
  this->numNodes = node_map->NumMyElements(); // Needed for the getDOF function to work correctly
                                        // This is either numOwnedNodes or numOverlapNodes, depending on
                                        // which map is passed in

  for (stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket & bucket = **it;

    this->saveVectorHelper(soln, solution_field, node_map, bucket, 0);

  }

}

template<bool Interleaved>
void Albany::OrdinarySTKFieldContainer<Interleaved>::saveResVector(const Epetra_Vector &res,
       stk::mesh::Selector &sel, const Teuchos::RCP<Epetra_Map>& node_map){

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector all_elements;
  stk::mesh::get_buckets(sel, this->bulkData->buckets(this->metaData->node_rank()), all_elements);
  this->numNodes = node_map->NumMyElements(); // Needed for the getDOF function to work correctly
                                        // This is either numOwnedNodes or numOverlapNodes, depending on
                                        // which map is passed in

  for (stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket & bucket = **it;

    this->saveVectorHelper(res, residual_field, node_map, bucket, 0);

  }

}
