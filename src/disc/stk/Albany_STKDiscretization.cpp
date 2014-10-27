//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#include <limits>

#include "Albany_Utils.hpp"
#include "Albany_STKDiscretization.hpp"
#include "Albany_NodalGraphUtils.hpp"
#include "Albany_STKNodeFieldContainer.hpp"

#include <string>
#include <iostream>
#include <fstream>

#include <Shards_BasicTopologies.hpp>
#include "Shards_CellTopology.hpp"
#include "Shards_CellTopologyData.h"

#include <Intrepid_CellTools.hpp>
#include <Intrepid_Basis.hpp>

#include <stk_util/parallel/Parallel.hpp>

#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/GetBuckets.hpp>
#include <stk_mesh/base/FieldData.hpp>
#include <stk_mesh/base/Selector.hpp>

#include <PHAL_Dimension.hpp>

#include <stk_mesh/fem/FEMHelpers.hpp>

#ifdef ALBANY_SEACAS
#include <Ionit_Initializer.h>
#include <netcdf.h>

#ifdef ALBANY_PAR_NETCDF
extern "C" {
#include <netcdf_par.h>
}
#endif
#endif

#include <algorithm>
#ifdef ALBANY_EPETRA
#include "Epetra_Export.h"
#include "EpetraExt_MultiVectorOut.h"
#include "Petra_Converters.hpp"
#endif

const double pi = 3.1415926535897932385;

//uncomment the following line if you want debug output to be printed to screen
//#define OUTPUT_TO_SCREEN

Albany::STKDiscretization::STKDiscretization(Teuchos::RCP<Albany::AbstractSTKMeshStruct> stkMeshStruct_,
					     const Teuchos::RCP<const Teuchos_Comm>& commT_,
                         const Teuchos::RCP<Piro::MLRigidBodyModes>& rigidBodyModes_) :

  out(Teuchos::VerboseObjectBase::getDefaultOStream()),
  previous_time_label(-1.0e32),
  metaData(*stkMeshStruct_->metaData),
  bulkData(*stkMeshStruct_->bulkData),
  commT(commT_),
  rigidBodyModes(rigidBodyModes_),
  neq(stkMeshStruct_->neq),
  stkMeshStruct(stkMeshStruct_),
  interleavedOrdering(stkMeshStruct_->interleavedOrdering)
{
#ifdef ALBANY_EPETRA
  comm = Albany::createEpetraCommFromTeuchosComm(commT_);
#endif
  Albany::STKDiscretization::updateMesh();

}

Albany::STKDiscretization::~STKDiscretization()
{
#ifdef ALBANY_SEACAS
  if (stkMeshStruct->exoOutput || stkMeshStruct->cdfOutput) delete mesh_data;

  if (stkMeshStruct->cdfOutput)
      if (netCDFp)
    if (const int ierr = nc_close (netCDFp))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "close returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

#endif

  for (int i=0; i< toDelete.size(); i++) delete [] toDelete[i];
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_Map>
Albany::STKDiscretization::getMap() const
{
  Teuchos::RCP<const Epetra_Map> map = Petra::TpetraMap_To_EpetraMap(mapT, comm);
  return map;
}
#endif

Teuchos::RCP<const Tpetra_Map>
Albany::STKDiscretization::getMapT() const
{
  return mapT;
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_Map>
Albany::STKDiscretization::getOverlapMap() const
{
  Teuchos::RCP<const Epetra_Map> overlap_map = Petra::TpetraMap_To_EpetraMap(overlap_mapT, comm);
  return overlap_map;
}
#endif

Teuchos::RCP<const Tpetra_Map>
Albany::STKDiscretization::getOverlapMapT() const
{
  return overlap_mapT;
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_CrsGraph>
Albany::STKDiscretization::getJacobianGraph() const
{
  Teuchos::RCP<const Epetra_CrsGraph> graph= Petra::TpetraCrsGraph_To_EpetraCrsGraph(graphT, comm);
  return graph;
}
#endif

Teuchos::RCP<const Tpetra_CrsGraph>
Albany::STKDiscretization::getJacobianGraphT() const
{
  return graphT;
}

#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_CrsGraph>
Albany::STKDiscretization::getOverlapJacobianGraph() const
{
  Teuchos::RCP<const Epetra_CrsGraph> overlap_graph= Petra::TpetraCrsGraph_To_EpetraCrsGraph(overlap_graphT, comm);
  return overlap_graph;
}
#endif

Teuchos::RCP<const Tpetra_CrsGraph>
Albany::STKDiscretization::getOverlapJacobianGraphT() const
{
  return overlap_graphT;
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_Map>
Albany::STKDiscretization::getNodeMap() const
{
  Teuchos::RCP<const Epetra_Map> node_map = Petra::TpetraMap_To_EpetraMap(node_mapT, comm);
  return node_map;
}
#endif

Teuchos::RCP<const Tpetra_Map>
Albany::STKDiscretization::getNodeMapT() const
{
  return node_mapT;
}


const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<LO> > > >::type&
Albany::STKDiscretization::getWsElNodeEqID() const
{
  return wsElNodeEqID;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> > >::type&
Albany::STKDiscretization::getWsElNodeID() const
{
  return wsElNodeID;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type&
Albany::STKDiscretization::getCoords() const
{
  return coords;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type&
Albany::STKDiscretization::getSurfaceHeight() const
{
  return sHeight;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type&
Albany::STKDiscretization::getTemperature() const
{
  return temperature;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type&
Albany::STKDiscretization::getBasalFriction() const
{
  return basalFriction;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type&
Albany::STKDiscretization::getThickness() const
{
  return thickness;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type&
Albany::STKDiscretization::getFlowFactor() const
{
  return flowFactor;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type&
Albany::STKDiscretization::getSurfaceVelocity() const
{
  return surfaceVelocity;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type&
Albany::STKDiscretization::getVelocityRMS() const
{
  return velocityRMS;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type&
Albany::STKDiscretization::getSphereVolume() const
{
  return sphereVolume;
}

void
Albany::STKDiscretization::printCoords() const
{

std::cout << "Processor " << bulkData.parallel_rank() << " has " << coords.size() << " worksets." << std::endl;

       for (int ws=0; ws<coords.size(); ws++) {  //workset
         for (int e=0; e<coords[ws].size(); e++) { //cell
           for (int j=0; j<coords[ws][e].size(); j++) { //node
             for (int d=0; d<stkMeshStruct->numDim; d++){  //node
std::cout << "Coord for workset: " << ws << " element: " << e << " node: " << j << " DOF: " << d << " is: " <<
                coords[ws][e][j][d] << std::endl;
       } } } }

}


Teuchos::ArrayRCP<double>&
Albany::STKDiscretization::getCoordinates() const
{
  // Coordinates are computed here, and not precomputed,
  // since the mesh can move in shape opt problems

  AbstractSTKFieldContainer::VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();

  for (int i=0; i < numOverlapNodes; i++)  {
    GO node_gid = gid(overlapnodes[i]);
    int node_lid = overlap_node_mapT->getLocalElement(node_gid);

    double* x = stk_classic::mesh::field_data(*coordinates_field, *overlapnodes[i]);
    for (int dim=0; dim<stkMeshStruct->numDim; dim++)
      coordinates[3*node_lid + dim] = x[dim];

  }

  return coordinates;
}

//The function transformMesh() maps a unit cube domain by applying the transformation
//x = L*x
//y = L*y
//z = s(x,y)*z + b(x,y)*(1-z)
//where b(x,y) and s(x,y) are curves specifying the bedrock and top surface
//geometries respectively.
//Currently this function is only needed for some FELIX problems.


void
Albany::STKDiscretization::transformMesh()
{
  using std::cout; using std::endl;
  AbstractSTKFieldContainer::VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();
  std::string transformType = stkMeshStruct->transformType;

  if (transformType == "None") {}
#ifdef ALBANY_FELIX
  else if (transformType == "ISMIP-HOM Test A") {
#ifdef OUTPUT_TO_SCREEN
    *out << "Test A!" << endl;
#endif
    double L = stkMeshStruct->felixL;
    double alpha = stkMeshStruct->felixAlpha;
#ifdef OUTPUT_TO_SCREEN
    *out << "L: " << L << endl;
    *out << "alpha degrees: " << alpha << endl;
#endif
    alpha = alpha*pi/180; //convert alpha, read in from ParameterList, to radians
#ifdef OUTPUT_TO_SCREEN
    *out << "alpha radians: " << alpha << endl;
#endif
    stkMeshStruct->PBCStruct.scale[0]*=L;
    stkMeshStruct->PBCStruct.scale[1]*=L;
    AbstractSTKFieldContainer::ScalarFieldType* surfaceHeight_field = stkMeshStruct->getFieldContainer()->getSurfaceHeightField();
    for (int i=0; i < numOverlapNodes; i++)  {
      double* x = stk_classic::mesh::field_data(*coordinates_field, *overlapnodes[i]);
      x[0] = L*x[0];
      x[1] = L*x[1];
      double s = -x[0]*tan(alpha);
      double b = s - 1.0 + 0.5*sin(2*pi/L*x[0])*sin(2*pi/L*x[1]);
      x[2] = s*x[2] + b*(1-x[2]);
      *stk_classic::mesh::field_data(*surfaceHeight_field, *overlapnodes[i]) = s;
     }
   }
  else if (transformType == "ISMIP-HOM Test B") {
#ifdef OUTPUT_TO_SCREEN
    *out << "Test B!" << endl;
#endif
    double L = stkMeshStruct->felixL;
    double alpha = stkMeshStruct->felixAlpha;
#ifdef OUTPUT_TO_SCREEN
    *out << "L: " << L << endl;
    *out << "alpha degrees: " << alpha << endl;
#endif
    alpha = alpha*pi/180; //convert alpha, read in from ParameterList, to radians
#ifdef OUTPUT_TO_SCREEN
    *out << "alpha radians: " << alpha << endl;
#endif
    stkMeshStruct->PBCStruct.scale[0]*=L;
    stkMeshStruct->PBCStruct.scale[1]*=L;
    AbstractSTKFieldContainer::ScalarFieldType* surfaceHeight_field = stkMeshStruct->getFieldContainer()->getSurfaceHeightField();
    for (int i=0; i < numOverlapNodes; i++)  {
      double* x = stk_classic::mesh::field_data(*coordinates_field, *overlapnodes[i]);
      x[0] = L*x[0];
      x[1] = L*x[1];
      double s = -x[0]*tan(alpha);
      double b = s - 1.0 + 0.5*sin(2*pi/L*x[0]);
      x[2] = s*x[2] + b*(1-x[2]);
      *stk_classic::mesh::field_data(*surfaceHeight_field, *overlapnodes[i]) = s;
     }
   }
   else if ((transformType == "ISMIP-HOM Test C") || (transformType == "ISMIP-HOM Test D")) {
#ifdef OUTPUT_TO_SCREEN
    *out << "Test C and D!" << endl;
#endif
    double L = stkMeshStruct->felixL;
    double alpha = stkMeshStruct->felixAlpha;
#ifdef OUTPUT_TO_SCREEN
    *out << "L: " << L << endl;
    *out << "alpha degrees: " << alpha << endl;
#endif
    alpha = alpha*pi/180; //convert alpha, read in from ParameterList, to radians
#ifdef OUTPUT_TO_SCREEN
    *out << "alpha radians: " << alpha << endl;
#endif
    stkMeshStruct->PBCStruct.scale[0]*=L;
    stkMeshStruct->PBCStruct.scale[1]*=L;
    AbstractSTKFieldContainer::ScalarFieldType* surfaceHeight_field = stkMeshStruct->getFieldContainer()->getSurfaceHeightField();
    for (int i=0; i < numOverlapNodes; i++)  {
      double* x = stk_classic::mesh::field_data(*coordinates_field, *overlapnodes[i]);
      x[0] = L*x[0];
      x[1] = L*x[1];
      double s = -x[0]*tan(alpha);
      double b = s - 1.0;
      x[2] = s*x[2] + b*(1-x[2]);
      *stk_classic::mesh::field_data(*surfaceHeight_field, *overlapnodes[i]) = s;
     }
   }
   else if (transformType == "Dome") {
#ifdef OUTPUT_TO_SCREEN
    *out << "Dome transform!" << endl;
#endif
    double L = 0.7071*30;
    stkMeshStruct->PBCStruct.scale[0]*=L;
    stkMeshStruct->PBCStruct.scale[1]*=L;
    AbstractSTKFieldContainer::ScalarFieldType* surfaceHeight_field = stkMeshStruct->getFieldContainer()->getSurfaceHeightField();
    for (int i=0; i < numOverlapNodes; i++)  {
      double* x = stk_classic::mesh::field_data(*coordinates_field, *overlapnodes[i]);
      x[0] = L*x[0];
      x[1] = L*x[1];
      double s = 0.7071*sqrt(450.0 - x[0]*x[0] - x[1]*x[1])/sqrt(450.0);
      x[2] = s*x[2];
      *stk_classic::mesh::field_data(*surfaceHeight_field, *overlapnodes[i]) = s;
    }
  }
   else if (transformType == "Confined Shelf") {
#ifdef OUTPUT_TO_SCREEN
    *out << "Confined shelf transform!" << endl;
#endif
    double L = stkMeshStruct->felixL;
    cout << "L: " << L << endl;
    stkMeshStruct->PBCStruct.scale[0]*=L;
    stkMeshStruct->PBCStruct.scale[1]*=L;
    AbstractSTKFieldContainer::ScalarFieldType* surfaceHeight_field = stkMeshStruct->getFieldContainer()->getSurfaceHeightField();
    for (int i=0; i < numOverlapNodes; i++)  {
      double* x = stk_classic::mesh::field_data(*coordinates_field, *overlapnodes[i]);
      x[0] = L*x[0];
      x[1] = L*x[1];
      double s = 0.06; //top surface is at z=0.06km=60m
      double b = -0.440; //basal surface is at z=-0.440km=-440m
      x[2] = s*x[2] + b*(1.0-x[2]);
      *stk_classic::mesh::field_data(*surfaceHeight_field, *overlapnodes[i]) = s;
    }
  }
  else if (transformType == "Circular Shelf") {
#ifdef OUTPUT_TO_SCREEN
    *out << "Circular shelf transform!" << endl;
#endif
    double L = stkMeshStruct->felixL;
#ifdef OUTPUT_TO_SCREEN
    *out << "L: " << L << endl;
#endif
    double rhoIce = 910.0; //ice density, in kg/m^3
    double rhoOcean = 1028.0; //ocean density, in kg/m^3
    stkMeshStruct->PBCStruct.scale[0]*=L;
    stkMeshStruct->PBCStruct.scale[1]*=L;
    AbstractSTKFieldContainer::ScalarFieldType* surfaceHeight_field = stkMeshStruct->getFieldContainer()->getSurfaceHeightField();
    for (int i=0; i < numOverlapNodes; i++)  {
      double* x = stk_classic::mesh::field_data(*coordinates_field, *overlapnodes[i]);
      x[0] = L*x[0];
      x[1] = L*x[1];
      double s = 1.0-rhoIce/rhoOcean; //top surface is at z=(1-rhoIce/rhoOcean) km
      double b = s - 1.0; //basal surface is at z=s-1 km
      x[2] = s*x[2] + b*(1.0-x[2]);
      *stk_classic::mesh::field_data(*surfaceHeight_field, *overlapnodes[i]) = s;
    }
  }
#endif
#ifdef ALBANY_AERAS
  else if (transformType == "Aeras Schar Mountain") {
    *out << "Aeras Schar Mountain transformation!" << endl;
    double rhoOcean = 1028.0; //ocean density, in kg/m^3
    for (int i=0; i < numOverlapNodes; i++)  {
      double* x = stk_classic::mesh::field_data(*coordinates_field, *overlapnodes[i]);
      x[0] = x[0];
      double hstar = 0.0, h;
      if (std::abs(x[0]-150.0) <= 25.0) hstar = 3.0* std::pow(cos(M_PI*(x[0]-150.0) / 50.0),2);
      h = hstar * std::pow(cos(M_PI*(x[0]-150.0) / 8.0),2);
      x[1] = x[1] + h*(25.0 - x[1])/25.0;
    }
  }
#endif
  else {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
      "STKDiscretization::transformMesh() Unknown transform type :" << transformType << std::endl);
  }
}

void
Albany::STKDiscretization::setupMLCoords()
{

  // if ML is not used, return

  if(rigidBodyModes.is_null()) return;

  if(!rigidBodyModes->isMLUsed() && !rigidBodyModes->isMueLuUsed()) return;

  // Function to return x,y,z at owned nodes as double*, specifically for ML
  int numDim = stkMeshStruct->numDim;
  AbstractSTKFieldContainer::VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();

  rigidBodyModes->resize(numDim, numOwnedNodes);

  //If ML preconditioner is selected
  if (rigidBodyModes->isMLUsed()) {

    double *xx;
    double *yy;
    double *zz;

    rigidBodyModes->getCoordArrays(&xx, &yy, &zz);

    for (int i=0; i < numOwnedNodes; i++)  {
      GO node_gid = gid(ownednodes[i]);
      int node_lid = node_mapT->getLocalElement(node_gid);

      double* X = stk_classic::mesh::field_data(*coordinates_field, *ownednodes[i]);
      if (numDim > 0) xx[node_lid] = X[0];
      if (numDim > 1) yy[node_lid] = X[1];
      if (numDim > 2) zz[node_lid] = X[2];
    }


    //see if user wants to write the coordinates to matrix market file
    bool writeCoordsToMMFile = stkMeshStruct->writeCoordsToMMFile;
    //if user wants to write the coordinates to matrix market file, write them to matrix market file
    if (writeCoordsToMMFile == true) {
      //IK, 10/29/13: neet to convert to tpetra!
      if (node_mapT->getComm()->getRank()==0) {std::cout << "Writing mesh coordinates to Matrix Market file." << std::endl;}
      //Writing of coordinates to MatrixMarket file for Ray
      Teuchos::ArrayView<ST> xxAV = Teuchos::arrayView(xx, numOwnedNodes);
      Teuchos::RCP<Tpetra_Vector> xCoordsT = Teuchos::rcp(new Tpetra_Vector(node_mapT, xxAV));
      Tpetra_MatrixMarket_Writer::writeDenseFile("xCoords.mm", xCoordsT);
      if (yy != NULL) {
        Teuchos::ArrayView<ST> yyAV = Teuchos::arrayView(yy, numOwnedNodes);
        Teuchos::RCP<Tpetra_Vector> yCoordsT = Teuchos::rcp(new Tpetra_Vector(node_mapT, yyAV));
        Tpetra_MatrixMarket_Writer::writeDenseFile("yCoords.mm", yCoordsT);
      }
      if (zz != NULL){
        Teuchos::ArrayView<ST> zzAV = Teuchos::arrayView(zz, numOwnedNodes);
        Teuchos::RCP<Tpetra_Vector> zCoordsT = Teuchos::rcp(new Tpetra_Vector(node_mapT, zzAV));
        Tpetra_MatrixMarket_Writer::writeDenseFile("zCoords.mm", zCoordsT);
      }
    }
    rigidBodyModes->informML();
  }

  //If MueLu preconditioner is selected
  if (rigidBodyModes->isMueLuUsed()) {
    std::cout << "MueLu selected!" << std::endl;
    double *xxyyzz; //make this ST?
    rigidBodyModes->getCoordArraysMueLu(&xxyyzz);

    for (int i=0; i < numOwnedNodes; i++)  {
      GO node_gid = gid(ownednodes[i]);
      int node_lid = node_mapT->getLocalElement(node_gid);

      double* X = stk_classic::mesh::field_data(*coordinates_field, *ownednodes[i]);
      for (int i=0; i<numDim; i++)
        xxyyzz[i*numOwnedNodes + node_lid] = X[i];
    }
   Teuchos::ArrayView<ST> xyzAV = Teuchos::arrayView(xxyyzz, numOwnedNodes*(numDim+1));
   Teuchos::RCP<Tpetra_MultiVector> xyzMV = Teuchos::rcp(new Tpetra_MultiVector(node_mapT, xyzAV, numOwnedNodes, numDim+1));

   rigidBodyModes->informMueLu(xyzMV, mapT);
   }


}


const Albany::WorksetArray<std::string>::type&
Albany::STKDiscretization::getWsEBNames() const
{
  return wsEBNames;
}

const Albany::WorksetArray<int>::type&
Albany::STKDiscretization::getWsPhysIndex() const
{
  return wsPhysIndex;
}

#ifdef ALBANY_EPETRA
void Albany::STKDiscretization::writeSolution(const Epetra_Vector& soln, const double time, const bool overlapped){

  // Put solution as Epetra_Vector into STK Mesh
  if(!overlapped)

    setSolutionField(soln);

  // soln coming in is overlapped
  else

    setOvlpSolutionField(soln);


#ifdef ALBANY_SEACAS

  if (stkMeshStruct->exoOutput && stkMeshStruct->transferSolutionToCoords) {

   Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

   container->transferSolutionToCoords();

   if (mesh_data != NULL) {

     // Mesh coordinates have changed. Rewrite output file by deleting the mesh data object and recreate it
     delete mesh_data;
     setupExodusOutput();

   }
  }


   // Skip this write unless the proper interval has been reached
  if (stkMeshStruct->exoOutput && !(outputInterval % stkMeshStruct->exoOutputInterval)) {

     double time_label = monotonicTimeLabel(time);

     int out_step = stk_classic::io::process_output_request(*mesh_data, bulkData, time_label);

     if (mapT->getComm()->getRank()==0) {
       *out << "Albany::STKDiscretization::writeSolution: writing time " << time;
       if (time_label != time) *out << " with label " << time_label;
       *out << " to index " <<out_step<<" in file "<<stkMeshStruct->exoOutFile<< std::endl;
     }
  }
  outputInterval++;
#endif
}
#endif

//Tpetra version of writeSolution
void Albany::STKDiscretization::writeSolutionT(const Tpetra_Vector& solnT, const double time, const bool overlapped){

  // Put solution as Epetra_Vector into STK Mesh
  if(!overlapped)

    setSolutionFieldT(solnT);

  // soln coming in is overlapped
  else

    setOvlpSolutionFieldT(solnT);


#ifdef ALBANY_SEACAS

  if (stkMeshStruct->exoOutput && stkMeshStruct->transferSolutionToCoords) {

   Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

   container->transferSolutionToCoords();

   if (mesh_data != NULL) {

     // Mesh coordinates have changed. Rewrite output file by deleting the mesh data object and recreate it
     delete mesh_data;
     setupExodusOutput();

   }
  }


  if (stkMeshStruct->exoOutput) {

     // Skip this write unless the proper interval has been reached
     if(outputInterval++ % stkMeshStruct->exoOutputInterval)

       return;

     double time_label = monotonicTimeLabel(time);

     int out_step = stk_classic::io::process_output_request(*mesh_data, bulkData, time_label);

     if (mapT->getComm()->getRank()==0) {
       *out << "Albany::STKDiscretization::writeSolutionT: writing time " << time;
       if (time_label != time) *out << " with label " << time_label;
       *out << " to index " <<out_step<<" in file "<<stkMeshStruct->exoOutFile<< std::endl;
     }
  }
#endif

}

double
Albany::STKDiscretization::monotonicTimeLabel(const double time)
{
  // If increasing, then all is good
  if (time > previous_time_label) {
    previous_time_label = time;
    return time;
  }
// Try absolute value
  double time_label = fabs(time);
  if (time_label > previous_time_label) {
    previous_time_label = time_label;
    return time_label;
  }

  // Try adding 1.0 to time
  if (time_label+1.0 > previous_time_label) {
    previous_time_label = time_label+1.0;
    return time_label+1.0;
  }

  // Otherwise, just add 1.0 to previous
  previous_time_label += 1.0;
  return previous_time_label;
}

#ifdef ALBANY_EPETRA
void
Albany::STKDiscretization::setResidualField(const Epetra_Vector& residual)
{
#ifdef ALBANY_LCM
  Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  if(container->hasResidualField()){

    // Iterate over the on-processor nodes
    stk_classic::mesh::Selector locally_owned = metaData.locally_owned_part();

    Teuchos::RCP<Epetra_Map> node_map = Petra::TpetraMap_To_EpetraMap(node_mapT, comm);
    container->saveResVector(residual, locally_owned, node_map);

    // Write the overlapped data
//    stk_classic::mesh::Selector select_owned_or_shared = metaData.locally_owned_part() | metaData.globally_shared_part();

//    container->saveResVector(residual, select_owned_or_shared, overlap_node_map);
  }
#endif
}
#endif

void
Albany::STKDiscretization::setResidualFieldT(const Tpetra_Vector& residualT)
{
#ifdef ALBANY_LCM
  Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  if(container->hasResidualField()){

    // Iterate over the on-processor nodes
    stk_classic::mesh::Selector locally_owned = metaData.locally_owned_part();
    container->saveResVectorT(residualT, locally_owned, node_mapT);

    // Write the overlapped data
//    stk_classic::mesh::Selector select_owned_or_shared = metaData.locally_owned_part() | metaData.globally_shared_part();

//    container->saveResVector(residual, select_owned_or_shared, overlap_node_map);
  }
#endif
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<Epetra_Vector>
Albany::STKDiscretization::getSolutionField() const
{
  // Copy soln vector into solution field, one node at a time
  Teuchos::ArrayView<const GO> indicesAV = mapT->getNodeElementList();
  int numElements = mapT->getNodeNumElements();
#ifdef ALBANY_64BIT_INT
  Teuchos::Array<int> i_indices(numElements);
  for(std::size_t k = 0; k < numElements; k++)
	i_indices[k] = Teuchos::as<int>(indicesAV[k]);
  Teuchos::RCP<Epetra_Map> map = Teuchos::rcp(new Epetra_Map(-1, numElements, i_indices.getRawPtr(), 0, *comm));
#else
  Teuchos::RCP<Epetra_Map> map = Teuchos::rcp(new Epetra_Map(-1, numElements, indicesAV.getRawPtr(), 0, *comm));
#endif
  Teuchos::RCP<Epetra_Vector> soln = Teuchos::rcp(new Epetra_Vector(*map));
  this->getSolutionField(*soln);
  return soln;
}
#endif

Teuchos::RCP<Tpetra_Vector>
Albany::STKDiscretization::getSolutionFieldT() const
{
  // Copy soln vector into solution field, one node at a time
  Teuchos::RCP<Tpetra_Vector> solnT = Teuchos::rcp(new Tpetra_Vector(mapT));
  this->getSolutionFieldT(*solnT);
  return solnT;
}


int
Albany::STKDiscretization::getSolutionFieldHistoryDepth() const
{
  return stkMeshStruct->getSolutionFieldHistoryDepth();
}

#ifdef ALBANY_EPETRA
Teuchos::RCP<Epetra_MultiVector>
Albany::STKDiscretization::getSolutionFieldHistory() const
{
  const int stepCount = this->getSolutionFieldHistoryDepth();
  return this->getSolutionFieldHistoryImpl(stepCount);
}

Teuchos::RCP<Epetra_MultiVector>
Albany::STKDiscretization::getSolutionFieldHistory(int maxStepCount) const
{
  const int stepCount = std::min(this->getSolutionFieldHistoryDepth(), maxStepCount);
  return this->getSolutionFieldHistoryImpl(stepCount);
}

//IK, 10/28/13: this function should be converted to Tpetra...
void
Albany::STKDiscretization::getSolutionFieldHistory(Epetra_MultiVector &result) const
{
  Teuchos::RCP<Epetra_Map> map = Petra::TpetraMap_To_EpetraMap(mapT, comm);
  TEUCHOS_TEST_FOR_EXCEPT(!map->SameAs(result.Map()));
  const int stepCount = std::min(this->getSolutionFieldHistoryDepth(), result.NumVectors());
  Epetra_MultiVector head(View, result, 0, stepCount);
  this->getSolutionFieldHistoryImpl(head);
}

Teuchos::RCP<Epetra_MultiVector>
Albany::STKDiscretization::getSolutionFieldHistoryImpl(int stepCount) const
{
  const int vectorCount = stepCount > 0 ? stepCount : 1; // A valid MultiVector has at least one vector
  Teuchos::ArrayView<const GO> indicesAV = mapT->getNodeElementList();
  LO numElements = mapT->getNodeNumElements();
#ifdef ALBANY_64BIT_INT
  Teuchos::Array<int> i_indices(numElements);
  for(std::size_t k = 0; k < numElements; k++)
	i_indices[k] = Teuchos::as<int>(indicesAV[k]);
  Teuchos::RCP<Epetra_Map> map = Teuchos::rcp(new Epetra_Map(-1, numElements, i_indices.getRawPtr(), 0, *comm));
#else
  Teuchos::RCP<Epetra_Map> map = Teuchos::rcp(new Epetra_Map(-1, numElements, indicesAV.getRawPtr(), 0, *comm));
#endif
  const Teuchos::RCP<Epetra_MultiVector> result = Teuchos::rcp(new Epetra_MultiVector(*map, vectorCount));
  if (stepCount > 0) {
    this->getSolutionFieldHistoryImpl(*result);
  }
  return result;
}

void
Albany::STKDiscretization::getSolutionFieldHistoryImpl(Epetra_MultiVector &result) const
{
  const int stepCount = result.NumVectors();
  for (int i = 0; i < stepCount; ++i) {
    stkMeshStruct->loadSolutionFieldHistory(i);
    Epetra_Vector v(View, result, i);
    this->getSolutionField(v);
  }
}

void
Albany::STKDiscretization::getSolutionField(Epetra_Vector &result) const
{

  Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk_classic::mesh::Selector locally_owned = metaData.locally_owned_part();

  Teuchos::RCP<Epetra_Map> node_map = Petra::TpetraMap_To_EpetraMap(node_mapT, comm);
  container->fillSolnVector(result, locally_owned, node_map);

}
#endif

void
Albany::STKDiscretization::getSolutionFieldT(Tpetra_Vector &resultT) const
{
  Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk_classic::mesh::Selector locally_owned = metaData.locally_owned_part();

  container->fillSolnVectorT(resultT, locally_owned, node_mapT);

}



/*****************************************************************/
/*** Private functions follow. These are just used in above code */
/*****************************************************************/

#ifdef ALBANY_EPETRA
void
Albany::STKDiscretization::setSolutionField(const Epetra_Vector& soln)
{
  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local (non overlapped) soln

  Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes
  stk_classic::mesh::Selector locally_owned = metaData.locally_owned_part();

  Teuchos::RCP<Epetra_Map> node_map = Petra::TpetraMap_To_EpetraMap(node_mapT, comm);
  container->saveSolnVector(soln, locally_owned, node_map);

}
#endif

//Tpetra version of above
void
Albany::STKDiscretization::setSolutionFieldT(const Tpetra_Vector& solnT)
{

  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local (non overlapped) soln

  Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes
  stk_classic::mesh::Selector locally_owned = metaData.locally_owned_part();

  container->saveSolnVectorT(solnT, locally_owned, node_mapT);

}

#ifdef ALBANY_EPETRA
void
Albany::STKDiscretization::setOvlpSolutionField(const Epetra_Vector& soln)
{
  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local+ghost (overlapped) soln

  Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the processor-visible nodes
  stk_classic::mesh::Selector select_owned_or_shared = metaData.locally_owned_part() | metaData.globally_shared_part();

  Teuchos::RCP<Epetra_Map> overlap_node_map = Petra::TpetraMap_To_EpetraMap(overlap_node_mapT, comm);
  container->saveSolnVector(soln, select_owned_or_shared, overlap_node_map);

}
#endif

void
Albany::STKDiscretization::setOvlpSolutionFieldT(const Tpetra_Vector& solnT)
{
  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local+ghost (overlapped) soln

  Teuchos::RCP<AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the processor-visible nodes
  stk_classic::mesh::Selector select_owned_or_shared = metaData.locally_owned_part() | metaData.globally_shared_part();

  container->saveSolnVectorT(solnT, select_owned_or_shared, overlap_node_mapT);

}


inline GO Albany::STKDiscretization::gid(const stk_classic::mesh::Entity& node) const
{ return node.identifier()-1; }

inline GO Albany::STKDiscretization::gid(const stk_classic::mesh::Entity* node) const
{ return gid(*node); }

int Albany::STKDiscretization::getOwnedDOF(const int inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numOwnedNodes*eq;
}

int Albany::STKDiscretization::getOverlapDOF(const int inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numOverlapNodes*eq;
}

GO Albany::STKDiscretization::getGlobalDOF(const GO inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numGlobalNodes*eq;
}

int Albany::STKDiscretization::nonzeroesPerRow(const int neq) const
{
  int numDim = stkMeshStruct->numDim;
  int estNonzeroesPerRow;
  switch (numDim) {
  case 0: estNonzeroesPerRow=1*neq; break;
  case 1: estNonzeroesPerRow=3*neq; break;
  case 2: estNonzeroesPerRow=9*neq; break;
  case 3: estNonzeroesPerRow=27*neq; break;
  default: TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
			      "STKDiscretization:  Bad numDim"<< numDim);
  }
  return estNonzeroesPerRow;
}

void Albany::STKDiscretization::computeOwnedNodesAndUnknowns()
{
  // Loads member data:  ownednodes, numOwnedNodes, node_map, numGlobalNodes, map
  // maps for owned nodes and unknowns
  stk_classic::mesh::Selector select_owned_in_part =
    stk_classic::mesh::Selector( metaData.universal_part() ) &
    stk_classic::mesh::Selector( metaData.locally_owned_part() );

  stk_classic::mesh::get_selected_entities( select_owned_in_part ,
				    bulkData.buckets( metaData.node_rank() ) ,
				    ownednodes );

  numOwnedNodes = ownednodes.size();
  Teuchos::Array<GO> indicesT(numOwnedNodes);
  for (int i=0; i < numOwnedNodes; i++) indicesT[i] = gid(ownednodes[i]);

  node_mapT = Teuchos::null; // delete existing map happens here on remesh

  node_mapT = Tpetra::createNonContigMap<LO, GO> (indicesT(), commT);

  numGlobalNodes = node_mapT->getMaxAllGlobalIndex() + 1;

  if(Teuchos::nonnull(stkMeshStruct->nodal_data_base))
    stkMeshStruct->nodal_data_base->resizeLocalMap(indicesT, commT);

  indicesT.resize(numOwnedNodes * neq);

  for (int i=0; i < numOwnedNodes; i++)
    for (std::size_t j=0; j < neq; j++)
      indicesT[getOwnedDOF(i,j)] = getGlobalDOF(gid(ownednodes[i]),j);

  mapT = Teuchos::null; // delete existing map happens here on remesh

  mapT = Tpetra::createNonContigMap<LO, GO> (indicesT(), commT);
}

void Albany::STKDiscretization::computeOverlapNodesAndUnknowns()
{
  // Loads member data:  overlapodes, numOverlapodes, overlap_node_map, coordinates
  std::vector<int> indices;
  // maps for overlap unknowns
  stk_classic::mesh::Selector select_overlap_in_part =
    stk_classic::mesh::Selector( metaData.universal_part() ) &
    ( stk_classic::mesh::Selector( metaData.locally_owned_part() )
      | stk_classic::mesh::Selector( metaData.globally_shared_part() ) );

  //  overlapnodes used for overlap map -- stored for changing coords
  stk_classic::mesh::get_selected_entities( select_overlap_in_part ,
				    bulkData.buckets( metaData.node_rank() ) ,
				    overlapnodes );

  numOverlapNodes = overlapnodes.size();

  Teuchos::Array<GO> indicesT(numOverlapNodes * neq);
  for (int i=0; i < numOverlapNodes; i++)
    for (std::size_t j=0; j < neq; j++)
      indicesT[getOverlapDOF(i,j)] = getGlobalDOF(gid(overlapnodes[i]),j);

  overlap_mapT = Teuchos::null; // delete existing map happens here on remesh

  overlap_mapT = Tpetra::createNonContigMap<LO, GO> (indicesT(), commT);

  indicesT.resize(numOverlapNodes);
  for (int i=0; i < numOverlapNodes; i++)
    indicesT[i] = gid(overlapnodes[i]);

  overlap_node_mapT = Teuchos::null; // delete existing map happens here on remesh

  overlap_node_mapT = Tpetra::createNonContigMap<LO, GO> (indicesT(), commT);

  if(Teuchos::nonnull(stkMeshStruct->nodal_data_base))
    stkMeshStruct->nodal_data_base->resizeOverlapMap(indicesT, commT);

  coordinates.resize(3*numOverlapNodes);

}


void Albany::STKDiscretization::computeGraphs()
{

  std::map<int, stk_classic::mesh::Part*>::iterator pv = stkMeshStruct->partVec.begin();
  int nodes_per_element =  metaData.get_cell_topology(*(pv->second)).getNodeCount();
// int nodes_per_element_est =  metaData.get_cell_topology(*(stkMeshStruct->partVec[0])).getNodeCount();

  // Loads member data:  overlap_graph, numOverlapodes, overlap_node_map, coordinates, graphs

  overlap_graphT = Teuchos::null; // delete existing graph happens here on remesh

  overlap_graphT = Teuchos::rcp(new Tpetra_CrsGraph(overlap_mapT, neq*nodes_per_element));

  stk_classic::mesh::Selector select_owned_in_part =
    stk_classic::mesh::Selector( metaData.universal_part() ) &
    stk_classic::mesh::Selector( metaData.locally_owned_part() );

  stk_classic::mesh::get_selected_entities( select_owned_in_part ,
				    bulkData.buckets( metaData.element_rank() ) ,
				    cells );


  if (commT->getRank()==0)
    *out << "STKDisc: " << cells.size() << " elements on Proc 0 " << std::endl;

  GO row, col;
  Teuchos::ArrayView<GO> colAV;

  for (std::size_t i=0; i < cells.size(); i++) {
    stk_classic::mesh::Entity& e = *cells[i];
    stk_classic::mesh::PairIterRelation rel = e.relations(metaData.NODE_RANK);

    // loop over local nodes
    for (std::size_t j=0; j < rel.size(); j++) {
      stk_classic::mesh::Entity& rowNode = * rel[j].entity();

      // loop over eqs
      for (std::size_t k=0; k < neq; k++) {
        row = getGlobalDOF(gid(rowNode), k);
        for (std::size_t l=0; l < rel.size(); l++) {
          stk_classic::mesh::Entity& colNode = * rel[l].entity();
          for (std::size_t m=0; m < neq; m++) {
            col = getGlobalDOF(gid(colNode), m);
            colAV = Teuchos::arrayView(&col, 1);
            overlap_graphT->insertGlobalIndices(row, colAV);
          }
        }
      }
    }
  }
  overlap_graphT->fillComplete();

  // Create Owned graph by exporting overlap with known row map
  graphT = Teuchos::null; // delete existing graph happens here on remesh

  graphT = Teuchos::rcp(new Tpetra_CrsGraph(mapT, nonzeroesPerRow(neq)));

  // Create non-overlapped matrix using two maps and export object
  Teuchos::RCP<Tpetra_Export> exporterT = Teuchos::rcp(new Tpetra_Export(overlap_mapT, mapT));
  graphT->doExport(*overlap_graphT, *exporterT, Tpetra::INSERT);
  graphT->fillComplete();
}

void Albany::STKDiscretization::computeWorksetInfo()
{

  stk_classic::mesh::Selector select_owned_in_part =
    stk_classic::mesh::Selector( metaData.universal_part() ) &
    stk_classic::mesh::Selector( metaData.locally_owned_part() );

  std::vector< stk_classic::mesh::Bucket * > buckets ;
  stk_classic::mesh::get_buckets( select_owned_in_part ,
                          bulkData.buckets( metaData.element_rank() ) ,
                          buckets);

  int numBuckets =  buckets.size();

  AbstractSTKFieldContainer::VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();
  AbstractSTKFieldContainer::ScalarFieldType* surfaceHeight_field;
  AbstractSTKFieldContainer::ScalarFieldType* temperature_field;
  AbstractSTKFieldContainer::ScalarFieldType* basalFriction_field;
  AbstractSTKFieldContainer::ScalarFieldType* thickness_field;
  AbstractSTKFieldContainer::ScalarFieldType* flowFactor_field;
  AbstractSTKFieldContainer::VectorFieldType* surfaceVelocity_field;
  AbstractSTKFieldContainer::VectorFieldType* velocityRMS_field;
  AbstractSTKFieldContainer::ScalarFieldType* sphereVolume_field;

  if(stkMeshStruct->getFieldContainer()->hasSurfaceHeightField())
    surfaceHeight_field = stkMeshStruct->getFieldContainer()->getSurfaceHeightField();

  if(stkMeshStruct->getFieldContainer()->hasTemperatureField())
    temperature_field = stkMeshStruct->getFieldContainer()->getTemperatureField();

  if(stkMeshStruct->getFieldContainer()->hasBasalFrictionField())
	  basalFriction_field = stkMeshStruct->getFieldContainer()->getBasalFrictionField();

  if(stkMeshStruct->getFieldContainer()->hasThicknessField())
  	thickness_field = stkMeshStruct->getFieldContainer()->getThicknessField();

  if(stkMeshStruct->getFieldContainer()->hasFlowFactorField())
    flowFactor_field = stkMeshStruct->getFieldContainer()->getFlowFactorField();

  if(stkMeshStruct->getFieldContainer()->hasSurfaceVelocityField())
    surfaceVelocity_field = stkMeshStruct->getFieldContainer()->getSurfaceVelocityField();

  if(stkMeshStruct->getFieldContainer()->hasVelocityRMSField())
    velocityRMS_field = stkMeshStruct->getFieldContainer()->getVelocityRMSField();

  if(stkMeshStruct->getFieldContainer()->hasSphereVolumeField())
    sphereVolume_field = stkMeshStruct->getFieldContainer()->getSphereVolumeField();

  wsEBNames.resize(numBuckets);
  for (int i=0; i<numBuckets; i++) {
    std::vector< stk_classic::mesh::Part * >  bpv;
    buckets[i]->supersets(bpv);
    for (std::size_t j=0; j<bpv.size(); j++) {
      if (bpv[j]->primary_entity_rank() == metaData.element_rank()) {
        if (bpv[j]->name()[0] != '{') {
	  // *out << "Bucket " << i << " is in Element Block:  " << bpv[j]->name()
	  //      << "  and has " << buckets[i]->size() << " elements." << std::endl;
          wsEBNames[i]=bpv[j]->name();
        }
      }
    }
  }

  wsPhysIndex.resize(numBuckets);
  if (stkMeshStruct->allElementBlocksHaveSamePhysics)
    for (int i=0; i<numBuckets; i++) wsPhysIndex[i]=0;
  else
    for (int i=0; i<numBuckets; i++) wsPhysIndex[i]=stkMeshStruct->ebNameToIndex[wsEBNames[i]];

  // Fill  wsElNodeEqID(workset, el_LID, local node, Eq) => unk_LID

  wsElNodeEqID.resize(numBuckets);
  wsElNodeID.resize(numBuckets);
  coords.resize(numBuckets);
  sHeight.resize(numBuckets);
  sphereVolume.resize(numBuckets);
  temperature.resize(numBuckets);
  basalFriction.resize(numBuckets);
  thickness.resize(numBuckets);
  flowFactor.resize(numBuckets);
  surfaceVelocity.resize(numBuckets);
  velocityRMS.resize(numBuckets);

  // Clear map if remeshing
  if(!elemGIDws.empty()) elemGIDws.clear();

  for (int b=0; b < numBuckets; b++) {

    stk_classic::mesh::Bucket& buck = *buckets[b];
    wsElNodeEqID[b].resize(buck.size());
    wsElNodeID[b].resize(buck.size());
    coords[b].resize(buck.size());
#ifdef ALBANY_FELIX
    if(stkMeshStruct->getFieldContainer()->hasSurfaceHeightField())
      sHeight[b].resize(buck.size());
    if(stkMeshStruct->getFieldContainer()->hasTemperatureField())
      temperature[b].resize(buck.size());
    if(stkMeshStruct->getFieldContainer()->hasBasalFrictionField())
      basalFriction[b].resize(buck.size());
    if(stkMeshStruct->getFieldContainer()->hasThicknessField())
      thickness[b].resize(buck.size());
    if(stkMeshStruct->getFieldContainer()->hasFlowFactorField())
      flowFactor[b].resize(buck.size());
    if(stkMeshStruct->getFieldContainer()->hasSurfaceVelocityField())
      surfaceVelocity[b].resize(buck.size());
    if(stkMeshStruct->getFieldContainer()->hasVelocityRMSField())
      velocityRMS[b].resize(buck.size());
#endif

#ifdef ALBANY_LCM
    if(stkMeshStruct->getFieldContainer()->hasSphereVolumeField())
      sphereVolume[b].resize(buck.size());
#endif

    // i is the element index within bucket b

    for (std::size_t i=0; i < buck.size(); i++) {

      // Traverse all the elements in this bucket
      stk_classic::mesh::Entity& element = buck[i];

      // Now, save a map from element GID to workset on this PE
      elemGIDws[gid(element)].ws = b;

      // Now, save a map from element GID to local id on this workset on this PE
      elemGIDws[gid(element)].LID = i;

      stk_classic::mesh::PairIterRelation rel = element.relations(metaData.NODE_RANK);

      int nodes_per_element = rel.size();
      wsElNodeEqID[b][i].resize(nodes_per_element);
      wsElNodeID[b][i].resize(nodes_per_element);
      coords[b][i].resize(nodes_per_element);
#ifdef ALBANY_FELIX
      if(stkMeshStruct->getFieldContainer()->hasSurfaceHeightField())
        sHeight[b][i].resize(nodes_per_element);
      if(stkMeshStruct->getFieldContainer()->hasTemperatureField())
        temperature[b][i] = *stk_classic::mesh::field_data(*temperature_field, element);
      if(stkMeshStruct->getFieldContainer()->hasBasalFrictionField())
    	  basalFriction[b][i].resize(nodes_per_element);
      if(stkMeshStruct->getFieldContainer()->hasThicknessField())
    	  thickness[b][i].resize(nodes_per_element);
      if(stkMeshStruct->getFieldContainer()->hasFlowFactorField())
         flowFactor[b][i] = *stk_classic::mesh::field_data(*flowFactor_field, element);
      if(stkMeshStruct->getFieldContainer()->hasSurfaceVelocityField())
    	  surfaceVelocity[b][i].resize(nodes_per_element);
      if(stkMeshStruct->getFieldContainer()->hasVelocityRMSField())
        velocityRMS[b][i].resize(nodes_per_element);
#endif

#ifdef ALBANY_LCM
      if(stkMeshStruct->getFieldContainer()->hasSphereVolumeField() && nodes_per_element == 1)
	sphereVolume[b][i] = *stk_classic::mesh::field_data(*sphereVolume_field, element);
#endif

      // loop over local nodes
      for (int j=0; j < nodes_per_element; j++) {
        stk_classic::mesh::Entity& rowNode = * rel[j].entity();
        GO node_gid = gid(rowNode);
        int node_lid = overlap_node_mapT->getLocalElement(node_gid);

        TEUCHOS_TEST_FOR_EXCEPTION(node_lid<0, std::logic_error,
			   "STK1D_Disc: node_lid out of range " << node_lid << std::endl);
        coords[b][i][j] = stk_classic::mesh::field_data(*coordinates_field, rowNode);
#ifdef ALBANY_FELIX
        if(stkMeshStruct->getFieldContainer()->hasSurfaceHeightField())
          sHeight[b][i][j] = *stk_classic::mesh::field_data(*surfaceHeight_field, rowNode);
        if(stkMeshStruct->getFieldContainer()->hasBasalFrictionField())
          basalFriction[b][i][j] = *stk_classic::mesh::field_data(*basalFriction_field, rowNode);
        if(stkMeshStruct->getFieldContainer()->hasThicknessField())
          thickness[b][i][j] = *stk_classic::mesh::field_data(*thickness_field, rowNode);
        if(stkMeshStruct->getFieldContainer()->hasSurfaceVelocityField())
          surfaceVelocity[b][i][j] = stk_classic::mesh::field_data(*surfaceVelocity_field, rowNode);
        if(stkMeshStruct->getFieldContainer()->hasVelocityRMSField())
          velocityRMS[b][i][j] = stk_classic::mesh::field_data(*velocityRMS_field, rowNode);
#endif
        wsElNodeEqID[b][i][j].resize(neq);
        wsElNodeID[b][i][j] = node_gid;

        for (std::size_t eq=0; eq < neq; eq++)
          wsElNodeEqID[b][i][j][eq] = getOverlapDOF(node_lid,eq);
      }
    }
  }

  for (int d=0; d<stkMeshStruct->numDim; d++) {
  if (stkMeshStruct->PBCStruct.periodic[d]) {
    for (int b=0; b < numBuckets; b++) {
      for (std::size_t i=0; i < buckets[b]->size(); i++) {
        int nodes_per_element = (*buckets[b])[i].relations(metaData.NODE_RANK).size();
        bool anyXeqZero=false;
        for (int j=0; j < nodes_per_element; j++)  if (coords[b][i][j][d]==0.0) anyXeqZero=true;
        if (anyXeqZero)  {
          bool flipZeroToScale=false;
          for (int j=0; j < nodes_per_element; j++)
              if (coords[b][i][j][d] > stkMeshStruct->PBCStruct.scale[d]/1.9) flipZeroToScale=true;
          if (flipZeroToScale) {
            for (int j=0; j < nodes_per_element; j++)  {
              if (coords[b][i][j][d] == 0.0) {
                double* xleak = new double [stkMeshStruct->numDim];
                for (int k=0; k < stkMeshStruct->numDim; k++)
                  if (k==d) xleak[d]=stkMeshStruct->PBCStruct.scale[d];
                  else xleak[k] = coords[b][i][j][k];
                std::string transformType = stkMeshStruct->transformType;
                double alpha = stkMeshStruct->felixAlpha;
                alpha *= pi/180.; //convert alpha, read in from ParameterList, to radians
                if ((transformType=="ISMIP-HOM Test A" || transformType == "ISMIP-HOM Test B" ||
                     transformType=="ISMIP-HOM Test C" || transformType == "ISMIP-HOM Test D") && d==0) {
                    xleak[2] -= stkMeshStruct->PBCStruct.scale[d]*tan(alpha);
#ifdef ALBANY_FELIX
                    if(stkMeshStruct->getFieldContainer()->hasSurfaceHeightField())
                	    sHeight[b][i][j] -= stkMeshStruct->PBCStruct.scale[d]*tan(alpha);
#endif
                }
                coords[b][i][j] = xleak; // replace ptr to coords
                toDelete.push_back(xleak);
              }
            }
          }
        }
      }
    }
  }
  }

  typedef Albany::AbstractSTKFieldContainer::ScalarValueState ScalarValueState;
  typedef Albany::AbstractSTKFieldContainer::QPScalarState QPScalarState ;
  typedef Albany::AbstractSTKFieldContainer::QPVectorState QPVectorState;
  typedef Albany::AbstractSTKFieldContainer::QPTensorState QPTensorState;

  typedef Albany::AbstractSTKFieldContainer::ScalarState ScalarState ;
  typedef Albany::AbstractSTKFieldContainer::VectorState VectorState;
  typedef Albany::AbstractSTKFieldContainer::TensorState TensorState;

  // Pull out pointers to shards::Arrays for every bucket, for every state
  // Code is data-type dependent

  ScalarValueState scalarValue_states = stkMeshStruct->getFieldContainer()->getScalarValueStates();
  QPScalarState qpscalar_states = stkMeshStruct->getFieldContainer()->getQPScalarStates();
  QPVectorState qpvector_states = stkMeshStruct->getFieldContainer()->getQPVectorStates();
  QPTensorState qptensor_states = stkMeshStruct->getFieldContainer()->getQPTensorStates();
  std::map<std::string, double>& time = stkMeshStruct->getFieldContainer()->getTime();

  stateArrays.elemStateArrays.resize(numBuckets);
  for (std::size_t b=0; b < buckets.size(); b++) {
    stk_classic::mesh::Bucket& buck = *buckets[b];
    for (QPScalarState::iterator qpss = qpscalar_states.begin();
              qpss != qpscalar_states.end(); ++qpss){
      stk_classic::mesh::BucketArray<Albany::AbstractSTKFieldContainer::QPScalarFieldType> array(**qpss, buck);
//Debug
//std::cout << "Buck.size(): " << buck.size() << " QPSFT dim[1]: " << array.dimension(1) << std::endl;
      MDArray ar = array;
      stateArrays.elemStateArrays[b][(*qpss)->name()] = ar;
    }
    for (QPVectorState::iterator qpvs = qpvector_states.begin();
              qpvs != qpvector_states.end(); ++qpvs){
      stk_classic::mesh::BucketArray<Albany::AbstractSTKFieldContainer::QPVectorFieldType> array(**qpvs, buck);
//Debug
//std::cout << "Buck.size(): " << buck.size() << " QPVFT dim[2]: " << array.dimension(2) << std::endl;
      MDArray ar = array;
      stateArrays.elemStateArrays[b][(*qpvs)->name()] = ar;
    }
    for (QPTensorState::iterator qpts = qptensor_states.begin();
              qpts != qptensor_states.end(); ++qpts){
      stk_classic::mesh::BucketArray<Albany::AbstractSTKFieldContainer::QPTensorFieldType> array(**qpts, buck);
//Debug
//std::cout << "Buck.size(): " << buck.size() << " QPTFT dim[3]: " << array.dimension(3) << std::endl;
      MDArray ar = array;
      stateArrays.elemStateArrays[b][(*qpts)->name()] = ar;
    }
    for (ScalarValueState::iterator svs = scalarValue_states.begin();
              svs != scalarValue_states.end(); ++svs){
      const int size = 1;
      shards::Array<double, shards::NaturalOrder, Cell> array(&time[*svs], size);
      MDArray ar = array;
//Debug
//std::cout << "Buck.size(): " << buck.size() << " SVState dim[0]: " << array.dimension(0) << std::endl;
//std::cout << "SV Name: " << *svs << " address : " << &array << std::endl;
      stateArrays.elemStateArrays[b][*svs] = ar;
    }
  }

// Process node data sets if present

  if(Teuchos::nonnull(stkMeshStruct->nodal_data_base) &&
    stkMeshStruct->nodal_data_base->isNodeDataPresent()){

    Teuchos::RCP<Albany::NodeFieldContainer> node_states = stkMeshStruct->nodal_data_base->getNodeContainer();

    stk_classic::mesh::get_buckets( select_owned_in_part ,
                            bulkData.buckets( metaData.node_rank() ) ,
                            buckets);

    numBuckets =  buckets.size();

    stateArrays.nodeStateArrays.resize(numBuckets);
    for (std::size_t b=0; b < buckets.size(); b++) {
      stk_classic::mesh::Bucket& buck = *buckets[b];
      for (Albany::NodeFieldContainer::iterator nfs = node_states->begin();
                nfs != node_states->end(); ++nfs){
        stateArrays.nodeStateArrays[b][(*nfs).first] =
             Teuchos::rcp_dynamic_cast<Albany::AbstractSTKNodeFieldContainer>((*nfs).second)->getMDA(buck);
      }
    }
  }
}

void Albany::STKDiscretization::computeSideSets(){

  // Clean up existing sideset structure if remeshing

  for(int i = 0; i < sideSets.size(); i++)
    sideSets[i].clear(); // empty the ith map

  const stk_classic::mesh::EntityRank element_rank = metaData.element_rank();

  // iterator over all side_rank parts found in the mesh
  std::map<std::string, stk_classic::mesh::Part*>::iterator ss = stkMeshStruct->ssPartVec.begin();

  int numBuckets = wsEBNames.size();

  sideSets.resize(numBuckets); // Need a sideset list per workset

  while ( ss != stkMeshStruct->ssPartVec.end() ) {

    // Get all owned sides in this side set
    stk_classic::mesh::Selector select_owned_in_sspart =

      // get only entities in the ss part (ss->second is the current sideset part)
      stk_classic::mesh::Selector( *(ss->second) ) &
      // and only if the part is local
      stk_classic::mesh::Selector( metaData.locally_owned_part() );

    std::vector< stk_classic::mesh::Entity * > sides ;
    stk_classic::mesh::get_selected_entities( select_owned_in_sspart , // sides local to this processor
				      bulkData.buckets( metaData.side_rank() ) ,
				      sides ); // store the result in "sides"

    *out << "STKDisc: sideset "<< ss->first <<" has size " << sides.size() << "  on Proc 0." << std::endl;

    // loop over the sides to see what they are, then fill in the data holder
    // for side set options, look at $TRILINOS_DIR/packages/stk/stk_usecases/mesh/UseCase_13.cpp

    for (std::size_t localSideID=0; localSideID < sides.size(); localSideID++) {

      stk_classic::mesh::Entity &sidee = *sides[localSideID];

      const stk_classic::mesh::PairIterRelation side_elems = sidee.relations(element_rank); // get the elements
            // containing the side. Note that if the side is internal, it will show up twice in the
            // element list, once for each element that contains it.

      TEUCHOS_TEST_FOR_EXCEPTION(side_elems.size() != 1, std::logic_error,
			   "STKDisc: cannot figure out side set topology for side set " << ss->first << std::endl);

      const stk_classic::mesh::Entity & elem = *side_elems[0].entity();

      SideStruct sStruct;

      // Save elem id. This is the global element id
      sStruct.elem_GID = gid(elem);

      int workset = elemGIDws[sStruct.elem_GID].ws; // Get the ws that this element lives in

      // Save elem id. This is the local element id within the workset
      sStruct.elem_LID = elemGIDws[sStruct.elem_GID].LID;

      // Save the side identifier inside of the element. This starts at zero here.
      sStruct.side_local_id = determine_local_side_id(elem, sidee);

      // Save the index of the element block that this elem lives in
      sStruct.elem_ebIndex = stkMeshStruct->ebNameToIndex[wsEBNames[workset]];

      SideSetList& ssList = sideSets[workset];   // Get a ref to the side set map for this ws
      SideSetList::iterator it = ssList.find(ss->first); // Get an iterator to the correct sideset (if
                                                                // it exists)

      if(it != ssList.end()) // The sideset has already been created

        it->second.push_back(sStruct); // Save this side to the vector that belongs to the name ss->first

      else { // Add the key ss->first to the map, and the side vector to that map

        std::vector<SideStruct> tmpSSVec;
        tmpSSVec.push_back(sStruct);

        ssList.insert(SideSetList::value_type(ss->first, tmpSSVec));

      }

    }

    ss++;
  }
}

unsigned
Albany::STKDiscretization::determine_local_side_id( const stk_classic::mesh::Entity & elem , stk_classic::mesh::Entity & side ) {

  using namespace stk_classic;

  const CellTopologyData * const elem_top = mesh::fem::get_cell_topology( elem ).getCellTopologyData();

  const mesh::PairIterRelation elem_nodes = elem.relations( mesh::fem::FEMMetaData::NODE_RANK );
  const mesh::PairIterRelation side_nodes = side.relations( mesh::fem::FEMMetaData::NODE_RANK );

  int side_id = -1 ;

  if(elem_nodes.size() == 0 || side_nodes.size() == 0){ // Node relations are not present, look at elem->face

    int elem_rank = elem.entity_rank();
    const mesh::PairIterRelation elem_sides = elem.relations( elem_rank - 1);

    for ( unsigned i = 0 ; i < elem_sides.size() ; ++i ) {

      const stk_classic::mesh::Entity & elem_side = *elem_sides[i].entity();

      if(elem_side.identifier() == side.identifier()){ // Found the local side in the element

         side_id = static_cast<int>(i);

         return side_id;

      }

    }

    if ( side_id < 0 ) {
      std::ostringstream msg ;
      msg << "determine_local_side_id( " ;
      msg << elem_top->name ;
      msg << " , Element[ " ;
      msg << elem.identifier();
      msg << " ]{" ;
      for ( unsigned i = 0 ; i < elem_sides.size() ; ++i ) {
        msg << " " << elem_sides[i].entity()->identifier();
      }
      msg << " } , Side[ " ;
      msg << side.identifier();
      msg << " ] ) FAILED" ;
      throw std::runtime_error( msg.str() );
    }

  }
  else { // Conventional elem->node - side->node connectivity present

    for ( unsigned i = 0 ; side_id == -1 && i < elem_top->side_count ; ++i ) {
      const CellTopologyData & side_top = * elem_top->side[i].topology ;
      const unsigned     * side_map =   elem_top->side[i].node ;

      if ( side_nodes.size() == side_top.node_count ) {

        side_id = i ;

        for ( unsigned j = 0 ;
              side_id == static_cast<int>(i) && j < side_top.node_count ; ++j ) {

          mesh::Entity * const elem_node = elem_nodes[ side_map[j] ].entity();

          bool found = false ;

          for ( unsigned k = 0 ; ! found && k < side_top.node_count ; ++k ) {
            found = elem_node == side_nodes[k].entity();
          }

          if ( ! found ) { side_id = -1 ; }
        }
      }
    }

    if ( side_id < 0 ) {
      std::ostringstream msg ;
      msg << "determine_local_side_id( " ;
      msg << elem_top->name ;
      msg << " , Element[ " ;
      msg << elem.identifier();
      msg << " ]{" ;
      for ( unsigned i = 0 ; i < elem_nodes.size() ; ++i ) {
        msg << " " << elem_nodes[i].entity()->identifier();
      }
      msg << " } , Side[ " ;
      msg << side.identifier();
      msg << " ]{" ;
      for ( unsigned i = 0 ; i < side_nodes.size() ; ++i ) {
        msg << " " << side_nodes[i].entity()->identifier();
      }
      msg << " } ) FAILED" ;
      throw std::runtime_error( msg.str() );
    }
  }

  return static_cast<unsigned>(side_id) ;
}

void Albany::STKDiscretization::computeNodeSets()
{

  std::map<std::string, stk_classic::mesh::Part*>::iterator ns = stkMeshStruct->nsPartVec.begin();
  AbstractSTKFieldContainer::VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();

  while ( ns != stkMeshStruct->nsPartVec.end() ) { // Iterate over Node Sets
    // Get all owned nodes in this node set
    stk_classic::mesh::Selector select_owned_in_nspart =
      stk_classic::mesh::Selector( *(ns->second) ) &
      stk_classic::mesh::Selector( metaData.locally_owned_part() );

    std::vector< stk_classic::mesh::Entity * > nodes ;
    stk_classic::mesh::get_selected_entities( select_owned_in_nspart ,
				      bulkData.buckets( metaData.node_rank() ) ,
				      nodes );

    nodeSets[ns->first].resize(nodes.size());
    nodeSetCoords[ns->first].resize(nodes.size());
//    nodeSetIDs.push_back(ns->first); // Grab string ID
    *out << "STKDisc: nodeset "<< ns->first <<" has size " << nodes.size() << "  on Proc 0." << std::endl;
    for (std::size_t i=0; i < nodes.size(); i++) {
      GO node_gid = gid(nodes[i]);
      int node_lid = node_mapT->getLocalElement(node_gid);
      nodeSets[ns->first][i].resize(neq);
      for (std::size_t eq=0; eq < neq; eq++)  nodeSets[ns->first][i][eq] = getOwnedDOF(node_lid,eq);
      nodeSetCoords[ns->first][i] = stk_classic::mesh::field_data(*coordinates_field, *nodes[i]);
    }
    ns++;
  }
}

void Albany::STKDiscretization::setupExodusOutput()
{
#ifdef ALBANY_SEACAS
  if (stkMeshStruct->exoOutput) {

    outputInterval = 0;

    std::string str = stkMeshStruct->exoOutFile;

    Ioss::Init::Initializer io;
    mesh_data = new stk_classic::io::MeshData();
    stk_classic::io::create_output_mesh(str,
		  Albany::getMpiCommFromTeuchosComm(commT),
		  bulkData, *mesh_data);

    stk_classic::io::define_output_fields(*mesh_data, metaData);

  }
#else
  if (stkMeshStruct->exoOutput)
    *out << "\nWARNING: exodus output requested but SEACAS not compiled in:"
         << " disabling exodus output \n" << std::endl;

#endif
}

namespace {
  const std::vector<double> spherical_to_cart(const std::pair<double, double> & sphere){
    const double radius_of_earth = 1;
    std::vector<double> cart(3);

    cart[0] = radius_of_earth*std::cos(sphere.first)*std::cos(sphere.second);
    cart[1] = radius_of_earth*std::cos(sphere.first)*std::sin(sphere.second);
    cart[2] = radius_of_earth*std::sin(sphere.first);

    return cart;
  }
  double distance (const double* x, const double* y) {
    const double d = std::sqrt((x[0]-y[0])*(x[0]-y[0]) +
                               (x[1]-y[1])*(x[1]-y[1]) +
                               (x[2]-y[2])*(x[2]-y[2]));
    return d;
  }
  double distance (const std::vector<double> &x, const std::vector<double> &y) {
    const double d = std::sqrt((x[0]-y[0])*(x[0]-y[0]) +
                               (x[1]-y[1])*(x[1]-y[1]) +
                               (x[2]-y[2])*(x[2]-y[2]));
    return d;
  }

  bool point_inside(const Teuchos::ArrayRCP<double*> &coords,
                    const std::vector<double>        &sphere_xyz) {
    // first check if point is near the element:
    const double  tol_inside = 1e-12;
    const double elem_diam = std::max(::distance(coords[0],coords[2]), ::distance(coords[1],coords[3]));
    std::vector<double> center(3,0);
    for (unsigned i=0; i<4; ++i)
      for (unsigned j=0; j<3; ++j) center[j] += coords[i][j];
    for (unsigned j=0; j<3; ++j) center[j] /= 4;
    bool inside = true;

    if ( ::distance(&center[0],&sphere_xyz[0]) > 1.0*elem_diam ) inside = false;

    unsigned j=3;
    for (unsigned i=0; i<4 && inside; ++i) {
      std::vector<double> cross(3);
      // outward normal to plane containing j->i edge:  corner(i) x corner(j)
      // sphere dot (corner(i) x corner(j) ) = negative if inside
      cross[0]=  coords[i][1]*coords[j][2] - coords[i][2]*coords[j][1];
      cross[1]=-(coords[i][0]*coords[j][2] - coords[i][2]*coords[j][0]);
      cross[2]=  coords[i][0]*coords[j][1] - coords[i][1]*coords[j][0];
      j = i;
      const double dotprod = cross[0]*sphere_xyz[0] +
                             cross[1]*sphere_xyz[1] +
                             cross[2]*sphere_xyz[2];

      // dot product is proportional to elem_diam. positive means outside,
      // but allow machine precision tolorence:
      if (tol_inside*elem_diam < dotprod) inside = false;
    }
    return inside;
  }


  const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > >
  Basis(const int C) {
    TEUCHOS_TEST_FOR_EXCEPTION(C!=4 && C!=9, std::logic_error,
      " Albany_STKDiscretization Error Basis not linear or quad"<<std::endl);
    static const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > > HGRAD_Basis_4 =
      Teuchos::rcp( new Intrepid::Basis_HGRAD_QUAD_C1_FEM<double, Intrepid::FieldContainer<double> >() );
    static const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > > HGRAD_Basis_9 =
      Teuchos::rcp( new Intrepid::Basis_HGRAD_QUAD_C2_FEM<double, Intrepid::FieldContainer<double> >() );
    return C==4 ? HGRAD_Basis_4 : HGRAD_Basis_9;
  }

  double value(const std::vector<double> &soln,
               const std::pair<double, double> &ref) {

    const int C = soln.size();
    const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > > HGRAD_Basis = Basis(C);

    const int numPoints        = 1;
    Intrepid::FieldContainer<double> basisVals (C, numPoints);
    Intrepid::FieldContainer<double> tempPoints(numPoints, 2);
    tempPoints(0,0) = ref.first;
    tempPoints(0,1) = ref.second;

    HGRAD_Basis->getValues(basisVals, tempPoints, Intrepid::OPERATOR_VALUE);

    double x = 0;
    for (unsigned j=0; j<C; ++j) x += soln[j] * basisVals(j,0);
    return x;
  }

  void value(double x[3],
             const Teuchos::ArrayRCP<double*> &coords,
             const std::pair<double, double> &ref){

    const int C = coords.size();
    const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > > HGRAD_Basis = Basis(C);

    const int numPoints        = 1;
    Intrepid::FieldContainer<double> basisVals (C, numPoints);
    Intrepid::FieldContainer<double> tempPoints(numPoints, 2);
    tempPoints(0,0) = ref.first;
    tempPoints(0,1) = ref.second;

    HGRAD_Basis->getValues(basisVals, tempPoints, Intrepid::OPERATOR_VALUE);

    for (unsigned i=0; i<3; ++i) x[i] = 0;
    for (unsigned i=0; i<3; ++i)
      for (unsigned j=0; j<C; ++j)
        x[i] += coords[j][i] * basisVals(j,0);
  }

  void grad(double x[3][2],
             const Teuchos::ArrayRCP<double*> &coords,
             const std::pair<double, double> &ref){

    const int C = coords.size();
    const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > > HGRAD_Basis = Basis(C);

    const int numPoints        = 1;
    Intrepid::FieldContainer<double> basisGrad (C, numPoints, 2);
    Intrepid::FieldContainer<double> tempPoints(numPoints, 2);
    tempPoints(0,0) = ref.first;
    tempPoints(0,1) = ref.second;

    HGRAD_Basis->getValues(basisGrad, tempPoints, Intrepid::OPERATOR_GRAD);

    for (unsigned i=0; i<3; ++i) x[i][0] = x[i][1] = 0;
    for (unsigned i=0; i<3; ++i)
      for (unsigned j=0; j<C; ++j) {
        x[i][0] += coords[j][i] * basisGrad(j,0,0);
        x[i][1] += coords[j][i] * basisGrad(j,0,1);
      }
  }

  std::pair<double, double>  ref2sphere(const Teuchos::ArrayRCP<double*> &coords,
                                        const std::pair<double, double> &ref) {

    static const double DIST_THRESHOLD= 1.0e-9;

    double x[3];
    value(x,coords,ref);

    const double r = std::sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);

    for (unsigned i=0; i<3; ++i) x[i] /= r;

    std::pair<double, double> sphere(std::asin(x[2]), std::atan2(x[1],x[0]));

    // ==========================================================
    // enforce three facts:
    //
    // 1) lon at poles is defined to be zero
    //
    // 2) Grid points must be separated by about .01 Meter (on earth)
    //   from pole to be considered "not the pole".
    //
    // 3) range of lon is { 0<= lon < 2*PI }
    //
    // ==========================================================

    if (std::abs(std::abs(sphere.first)-pi/2) < DIST_THRESHOLD) sphere.second = 0;
    else if (sphere.second < 0) sphere.second += 2*pi;

    return sphere;
  }

  void Dmap(const Teuchos::ArrayRCP<double*> &coords,
            const std::pair<double, double>  &sphere,
            const std::pair<double, double>  &ref,
            double D[][2]) {

    const double th     = sphere.first;
    const double lam    = sphere.second;
    const double sinlam = std::sin(lam);
    const double sinth  = std::sin(th);
    const double coslam = std::cos(lam);
    const double costh  = std::cos(th);

    const double D1[2][3] = {{-sinlam, coslam, 0},
                             {      0,      0, 1}};

    const double D2[3][3] = {{ sinlam*sinlam*costh*costh+sinth*sinth, -sinlam*coslam*costh*costh,             -coslam*sinth*costh},
                             {-sinlam*coslam*costh*costh,              coslam*coslam*costh*costh+sinth*sinth, -sinlam*sinth*costh},
                             {-coslam*sinth,                          -sinlam*sinth,                                        costh}};

    double D3[3][2] = {0};
    grad(D3,coords,ref);

    double D4[3][2] = {0};
    for (unsigned i=0; i<3; ++i)
      for (unsigned j=0; j<2; ++j)
        for (unsigned k=0; k<3; ++k)
           D4[i][j] += D2[i][k] * D3[k][j];

    for (unsigned i=0; i<2; ++i)
      for (unsigned j=0; j<2; ++j) D[i][j] = 0;

    for (unsigned i=0; i<2; ++i)
      for (unsigned j=0; j<2; ++j)
        for (unsigned k=0; k<3; ++k)
          D[i][j] += D1[i][k] * D4[k][j];

  }

  std::pair<double, double> parametric_coordinates(const Teuchos::ArrayRCP<double*> &coords,
                                                   const std::pair<double, double>  &sphere) {

    static const double tol_sq = 1e-26;
    static const unsigned MAX_NR_ITER = 10;
    double costh = std::cos(sphere.first);
    double D[2][2], Dinv[2][2];
    double resa = 1;
    double resb = 1;
    std::pair<double, double> ref(0,0); // initial guess is center of element.

    for (unsigned i=0; i<MAX_NR_ITER && tol_sq < (costh*resb*resb + resa*resa) ; ++i) {
      const std::pair<double, double> sph = ref2sphere(coords,ref);
      resa = sph.first  - sphere.first;
      resb = sph.second - sphere.second;

      if (resb >  pi) resb -= 2*pi;
      if (resb < -pi) resb += 2*pi;

      Dmap(coords, sph, ref, D);
      const double detD = D[0][0]*D[1][1] - D[0][1]*D[1][0];
      Dinv[0][0] =  D[1][1]/detD;
      Dinv[0][1] = -D[0][1]/detD;
      Dinv[1][0] = -D[1][0]/detD;
      Dinv[1][1] =  D[0][0]/detD;

      const std::pair<double, double> del( Dinv[0][0]*costh*resb + Dinv[0][1]*resa,
                                           Dinv[1][0]*costh*resb + Dinv[1][1]*resa);
      ref.first  -= del.first;
      ref.second -= del.second;
    }
    return ref;
  }

  const std::pair<bool,std::pair<unsigned, unsigned> >point_in_element(const std::pair<double, double> &sphere,
      const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& coords,
      std::pair<double, double> &parametric) {
    const std::vector<double> sphere_xyz = spherical_to_cart(sphere);
    std::pair<bool,std::pair<unsigned, unsigned> > element(false,std::pair<unsigned, unsigned>(0,0));
    for (unsigned i=0; i<coords.size() && !element.first; ++i) {
      for (unsigned j=0; j<coords[i].size() && !element.first; ++j) {
        const bool found =  point_inside(coords[i][j], sphere_xyz);
        if (found) {
          parametric = parametric_coordinates(coords[i][j], sphere);
          if (parametric.first  < -1) parametric.first  = -1;
          if (parametric.second < -1) parametric.second = -1;
          if (1 < parametric.first  ) parametric.first  =  1;
          if (1 < parametric.second ) parametric.second =  1;
          element.first         = true;
          element.second.first  = i;
          element.second.second = j;
        }
      }
    }
    return element;
  }

  void setup_latlon_interp(
    const unsigned nlat, const double nlon,
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& coords,
    Albany::WorksetArray<Teuchos::ArrayRCP<std::vector<Albany::STKDiscretization::interp> > >::type& interpdata,
    const Teuchos::RCP<const Teuchos_Comm> commT) {

    double err=0;
    const long long unsigned rank = commT->getRank();
    std::vector<double> lat(nlat);
    std::vector<double> lon(nlon);

    unsigned count=0;
    for (unsigned i=0; i<nlat; ++i) lat[i] = -pi/2 + i*pi/(nlat-1);
    for (unsigned j=0; j<nlon; ++j) lon[j] =       2*j*pi/nlon;
    for (unsigned i=0; i<nlat; ++i) {
      for (unsigned j=0; j<nlon; ++j) {
        const std::pair<double, double> sphere(lat[i],lon[j]);
        std::pair<double, double> paramtric;
        const std::pair<bool,std::pair<unsigned, unsigned> >element = point_in_element(sphere, coords, paramtric);
        if (element.first) {
          // compute error: map 'cart' back to sphere and compare with original
          // interpolation point:
          const unsigned b = element.second.first ;
          const unsigned e = element.second.second;
          const std::vector<double> sphere2_xyz = spherical_to_cart(ref2sphere(coords[b][e], paramtric));
          const std::vector<double> sphere_xyz  = spherical_to_cart(sphere);
          err = std::max(err, ::distance(&sphere2_xyz[0],&sphere_xyz[0]));
          Albany::STKDiscretization::interp interp;
          interp.parametric_coords = paramtric;
          interp.latitude_longitude = std::pair<unsigned,unsigned>(i,j);
          interpdata[b][e].push_back(interp);
          ++count;
        }
      }
      if (!rank && (!(i%64) || i==nlat-1)) std::cout<< "Finished Latitude "<<i<<" of "<<nlat<<std::endl;
    }
    if (!rank) std::cout<<"Max interpolation point search error: "<<err<<std::endl;
  }
}

#ifdef ALBANY_EPETRA
int Albany::STKDiscretization::processNetCDFOutputRequest(const Epetra_Vector& solution_field) {
#ifdef ALBANY_SEACAS
  const long long unsigned rank = commT->getRank();
  const unsigned nlat = stkMeshStruct->nLat;
  const unsigned nlon = stkMeshStruct->nLon;

  std::vector<double> local(nlat*nlon*neq, -std::numeric_limits<double>::max());


  for (unsigned n=0; n<neq; ++n) {
    for (unsigned b=0; b<interpolateData.size(); ++b) {
      Teuchos::ArrayRCP<std::vector<interp> >        Interpb = interpolateData[b];
      Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > Coordsb = coords[b];
      Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > > ElNodeEqID = wsElNodeEqID[b];

      for (unsigned e=0; e<Interpb.size(); ++e) {
        const std::vector<interp>                    &interp = Interpb[e];
        Teuchos::ArrayRCP<double*>                    coordp = Coordsb[e];
        Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >    elnode = ElNodeEqID[e];

        const int C = coordp.size();
        std::vector<double> soln(C);
        for (unsigned i=0; i<C; ++i) {
          const int overlap_dof = elnode[i][n];
          soln[i] = solution_field[overlap_dof];
        }
        for (unsigned p=0; p<interp.size(); ++p) {
          Albany::STKDiscretization::interp par    = interp[p];
          const double y = value(soln, par.parametric_coords);
          std::pair<unsigned,unsigned> latlon =   par.latitude_longitude;
          local[nlon*latlon.first + latlon.second + n*nlat*nlon] = y;
        }
      }
    }
  }

  std::vector<double> global(neq*nlat*nlon);
  comm->MaxAll(&local[0], &global[0], neq*nlat*nlon);

#ifdef ALBANY_PAR_NETCDF
  const long long unsigned np   = commT->getSize();
  const size_t start            = static_cast<size_t>((rank*nlat)/np);
  const size_t end              = static_cast<size_t>(((rank+1)*nlat)/np);
  const size_t len              = end-start;

  for (unsigned n=0; n<neq; ++n) {
    const size_t  startp[] = {netCDFOutputRequest,    0, start, 0};
    const size_t  countp[] = {1, 1, len, nlon};
    if (const int ierr = nc_put_vara_double (netCDFp, varSolns[n], startp, countp, &global[n*nlat*nlon]))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_vara_double returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
  }
#else
  if (netCDFp) {
    for (unsigned n=0; n<neq; ++n)  {
      const size_t  startp[] = {netCDFOutputRequest,    0, 0, 0};
      const size_t  countp[] = {1, 1, nlat, nlon};
      if (const int ierr = nc_put_vara_double (netCDFp, varSolns[n], startp, countp, &global[n*nlat*nlon]))
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
          "nc_put_vara returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
    }
  }
#endif
#endif
  return netCDFOutputRequest++;
}
#endif

void Albany::STKDiscretization::setupNetCDFOutput()
{
  const long long unsigned rank = commT->getRank();
#ifdef ALBANY_SEACAS
  if (stkMeshStruct->cdfOutput) {
    outputInterval = 0;
    const unsigned nlat = stkMeshStruct->nLat;
    const unsigned nlon = stkMeshStruct->nLon;


    std::string str = stkMeshStruct->cdfOutFile;

    interpolateData.resize(coords.size());
    for (int b=0; b < coords.size(); b++) interpolateData[b].resize(coords[b].size());

    setup_latlon_interp(nlat, nlon, coords, interpolateData, commT);

    const std::string name = stkMeshStruct->cdfOutFile;
    netCDFp=0;
    netCDFOutputRequest=0;


#ifdef ALBANY_PAR_NETCDF
    MPI_Comm theMPIComm = Albany::getMpiCommFromTeuchosComm(commT);
    MPI_Info info;
    MPI_Info_create(&info);
    if (const int ierr = nc_create_par (name.c_str(), NC_NETCDF4 | NC_MPIIO | NC_CLOBBER | NC_64BIT_OFFSET, theMPIComm, info, &netCDFp))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_create_par returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
    MPI_Info_free(&info);
#else
    if (!rank)
    if (const int ierr = nc_create (name.c_str(), NC_CLOBBER | NC_SHARE | NC_64BIT_OFFSET | NC_CLASSIC_MODEL, &netCDFp))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_create returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
#endif

    const size_t nlev = 1;
    const char *dimnames[] = {"time","lev","lat","lon"};
    const size_t  dimlen[] = {NC_UNLIMITED, nlev, nlat, nlon};
    int dimID[4]={0,0,0,0};

    for (unsigned i=0; i<4; ++i) {
      if (netCDFp)
      if (const int ierr = nc_def_dim (netCDFp,  dimnames[i], dimlen[i], &dimID[i]))
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
          "nc_def_dim returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
    }
    varSolns.resize(neq,0);

    for (unsigned n=0; n<neq; ++n) {
      std::ostringstream var;
      var <<"variable_"<<n;
      const char *field_name = var.str().c_str();
      if (netCDFp)
      if (const int ierr = nc_def_var (netCDFp,  field_name, NC_DOUBLE, 4, dimID, &varSolns[n]))
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
          "nc_def_var "<<field_name<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

      const double fillVal = -9999.0;
      if (netCDFp)
      if (const int ierr = nc_put_att (netCDFp,  varSolns[n], "FillValue", NC_DOUBLE, 1, &fillVal))
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
          "nc_put_att FillValue returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
    }

    const char lat_name[] = "latitude";
    const char lat_unit[] = "degrees_north";
    const char lon_name[] = "longitude";
    const char lon_unit[] = "degrees_east";
    int latVarID=0;
      if (netCDFp)
    if (const int ierr = nc_def_var (netCDFp,  "lat", NC_DOUBLE, 1, &dimID[2], &latVarID))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_def_var lat returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  latVarID, "long_name", sizeof(lat_name), lat_name))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<lat_name<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  latVarID, "units", sizeof(lat_unit), lat_unit))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<lat_unit<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

    int lonVarID=0;
      if (netCDFp)
    if (const int ierr = nc_def_var (netCDFp,  "lon", NC_DOUBLE, 1, &dimID[3], &lonVarID))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_def_var lon returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  lonVarID, "long_name", sizeof(lon_name), lon_name))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<lon_name<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  lonVarID, "units", sizeof(lon_unit), lon_unit))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<lon_unit<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

    const char history[]="Created by Albany";
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  NC_GLOBAL, "history", sizeof(history), history))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<history<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

      if (netCDFp)
    if (const int ierr = nc_enddef (netCDFp))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_enddef returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

    std::vector<double> deglon(nlon);
    std::vector<double> deglat(nlat);
    for (unsigned i=0; i<nlon; ++i) deglon[i] =((      2*i*pi/nlon) *   (180/pi)) - 180;
    for (unsigned i=0; i<nlat; ++i) deglat[i] = (-pi/2 + i*pi/(nlat-1))*(180/pi);


      if (netCDFp)
    if (const int ierr = nc_put_var (netCDFp, lonVarID, &deglon[0]))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_var lon returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_var (netCDFp, latVarID, &deglat[0]))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_var lat returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

  }
#else
  if (stkMeshStruct->cdfOutput)
    *out << "\nWARNING: NetCDF output requested but SEACAS not compiled in:"
         << " disabling NetCDF output \n" << std::endl;
  stkMeshStruct->cdfOutput = false;

#endif
}

void Albany::STKDiscretization::reNameExodusOutput(std::string& filename)
{
#ifdef ALBANY_SEACAS
  if (stkMeshStruct->exoOutput && mesh_data != NULL) {

   // Delete the mesh data object and recreate it
   delete mesh_data;

   stkMeshStruct->exoOutFile = filename;

   // reset reference value for monotonic time function call as we are writing to a new file
   previous_time_label = -1.0e32;

  }
#else
  if (stkMeshStruct->exoOutput)
    *out << "\nWARNING: exodus output requested but SEACAS not compiled in:"
         << " disabling exodus output \n" << std::endl;

#endif
}

/*
  Convert the stk mesh on this processor to a nodal graph.
*/
void Albany::STKDiscretization::meshToGraph () {
  if(Teuchos::is_null(stkMeshStruct->nodal_data_base)) return;
  if(!stkMeshStruct->nodal_data_base->isNodeDataPresent()) return;

  // Set up the CRS graph used for solution transfer and projection mass
  // matrices. Assume the Crs row size is 27, which is the maximum number
  // required for first-order hexahedral elements.
  nodalGraph = Teuchos::rcp(new Tpetra_CrsGraph(overlap_node_mapT, 27));

  // Elements that surround a given node, in the form of Entity *'s
  std::vector<std::vector<stk_classic::mesh::Entity *> > sur_elem;
  // numOverlapNodes are the total # of nodes seen by this pe
  // numOwnedNodes are the total # of nodes owned by this pe
  sur_elem.resize(numOverlapNodes);

  // Get the elements owned by the current processor
  stk_classic::mesh::Selector select_owned_in_part =
    stk_classic::mesh::Selector( metaData.universal_part() ) &
    stk_classic::mesh::Selector( metaData.locally_owned_part() );

  std::vector< stk_classic::mesh::Bucket * > buckets;
  stk_classic::mesh::get_buckets(select_owned_in_part,
                                 bulkData.buckets( metaData.element_rank() ),
                                 buckets);

  for (int b = 0; b < buckets.size(); ++b) {
    stk_classic::mesh::Bucket& cells = *buckets[b];
    // TODO handle higher order elements

    /* Find the surrounding elements for each node owned by this processor */
    for (std::size_t ecnt = 0; ecnt < cells.size(); ecnt++) {
      stk_classic::mesh::Entity& e = cells[ecnt];
      stk_classic::mesh::PairIterRelation rel = e.relations(metaData.NODE_RANK);

      // loop over nodes within the element
      for (std::size_t ncnt = 0; ncnt < rel.size(); ncnt++) {
        stk_classic::mesh::Entity& rowNode = * rel[ncnt].entity();
        GO nodeGID = gid(rowNode);
        int nodeLID = overlap_node_mapT->getLocalElement(nodeGID);
        /* in the case of degenerate elements, where a node can be
         * entered into the connect table twice, need to check to
         * make sure that this element is not already listed as
         * surrounding this node */
        if (sur_elem[nodeLID].empty() || entity_in_list(&e, sur_elem[nodeLID]) < 0)
          sur_elem[nodeLID].push_back(&e); /* Add the element to the list */
      }
    } /* End "for(ecnt=0; ecnt < mesh->num_elems; ecnt++)" */
  } // End of loop over buckets

  std::size_t max_nsur = 0;
  for (std::size_t ncnt = 0; ncnt < numOverlapNodes; ncnt++) {
    if (sur_elem[ncnt].empty()) {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                                 "Node = " << ncnt+1 << " has no elements" << std::endl);
    } else {
      std::size_t nsur = sur_elem[ncnt].size();
      if (nsur > max_nsur) max_nsur = nsur;
    }
  }

  // end find_surrnd_elems

  // find_adjacency

  // Note that the center node of a subgraph must be owned by this pe, but we
  // want all nodes in the overlap graph to be covered in the nodal graph

  // loop over all the nodes owned by this PE
  for(std::size_t ncnt = 0; ncnt < numOverlapNodes; ncnt++) {
    Teuchos::Array<GO> adjacency;
    GO globalrow = overlap_node_mapT->getGlobalElement(ncnt);
    // loop over the elements surrounding node ncnt
    for(std::size_t ecnt = 0; ecnt < sur_elem[ncnt].size(); ecnt++) {
      stk_classic::mesh::Entity* elem  = sur_elem[ncnt][ecnt];
      stk_classic::mesh::PairIterRelation rel = elem->relations(metaData.NODE_RANK);
      std::size_t ws = elemGIDws[gid(elem)].ws;
      // loop over the nodes in the surrounding element elem
      for (std::size_t lnode = 0; lnode < rel.size(); lnode++) {
        stk_classic::mesh::Entity& node_a = * rel[lnode].entity();
        // entry is the GID of each node
        GO entry = gid(node_a);
        // Every node in an element adjacent to node 'globalrow' is in this
        // graph.
        if (in_list(entry, adjacency) < 0) adjacency.push_back(entry);
      }
    } /* End "for(ecnt=0; ecnt < graph->nsur_elem[ncnt]; ecnt++)" */
    nodalGraph->insertGlobalIndices(globalrow, adjacency());
  } /* End "for(ncnt=0; ncnt < mesh->num_nodes; ncnt++)" */

  // end find_adjacency

  nodalGraph->fillComplete();
  // Pass the graph RCP to the nodal data block
  stkMeshStruct->nodal_data_base->updateNodalGraph(nodalGraph);
}

void
Albany::STKDiscretization::printVertexConnectivity(){

  if(Teuchos::is_null(nodalGraph)) return;

  for(std::size_t i = 0; i < numOverlapNodes; i++){

    GO globalvert = overlap_node_mapT->getGlobalElement(i);

    std::cout << "Center vert is : " << globalvert + 1 << std::endl;

    Teuchos::ArrayView<const GO> adj;

    nodalGraph->getGlobalRowView(globalvert, adj);

    for(std::size_t j = 0; j < adj.size(); j++)

      std::cout << "                  " << adj[j] + 1 << std::endl;

   }
}

void
Albany::STKDiscretization::updateMesh()
{

  computeOwnedNodesAndUnknowns();

  setupMLCoords();

  computeOverlapNodesAndUnknowns();

  transformMesh();

  computeGraphs();

  computeWorksetInfo();

  computeNodeSets();

  computeSideSets();

  setupExodusOutput();

  // Build the node graph needed for the mass matrix for solution transfer and projection operations
  // FIXME this only needs to be called if we are using the L2 Projection response
  meshToGraph();
//  printVertexConnectivity();
  setupNetCDFOutput();
//meshToGraph();
//printVertexConnectivity();

}
