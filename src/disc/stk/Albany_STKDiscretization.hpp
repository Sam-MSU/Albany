//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#ifndef ALBANY_STKDISCRETIZATION_HPP
#define ALBANY_STKDISCRETIZATION_HPP

#include <vector>
#include <utility>

#include "Teuchos_ParameterList.hpp"
#include "Teuchos_VerboseObject.hpp"


#include "Albany_AbstractDiscretization.hpp"
#include "Albany_AbstractSTKMeshStruct.hpp"
#include "Albany_DataTypes.hpp"

#ifdef ALBANY_EPETRA
#include "Epetra_Comm.h"
#include "Epetra_CrsMatrix.h"
#include "Epetra_Vector.h"
#endif

#include "Piro_NullSpaceUtils.hpp" // has defn of struct that holds null space info for ML

// Start of STK stuff
#include <stk_util/parallel/Parallel.hpp>
#include <stk_mesh/base/Types.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/base/FieldTraits.hpp>
#ifdef ALBANY_SEACAS
  #include <stk_io/MeshReadWriteUtils.hpp>
#endif


namespace Albany {

/*
=======
  struct MeshGraph {

       std::vector<std::size_t> start;
       std::vector<std::size_t> adj;

  };
*/

  class STKDiscretization : public Albany::AbstractDiscretization {
  public:

    //! Constructor
    STKDiscretization(
       Teuchos::RCP<Albany::AbstractSTKMeshStruct> stkMeshStruct,
       const Teuchos::RCP<const Teuchos_Comm>& commT,
       const Teuchos::RCP<Piro::MLRigidBodyModes>& rigidBodyModes = Teuchos::null);


    //! Destructor
    ~STKDiscretization();

#ifdef ALBANY_EPETRA
    //! Get Epetra DOF map
    Teuchos::RCP<const Epetra_Map> getMap() const;
#endif
    //! Get Tpetra DOF map
    Teuchos::RCP<const Tpetra_Map> getMapT() const;

#ifdef ALBANY_EPETRA
    //! Get Epetra overlapped DOF map
    Teuchos::RCP<const Epetra_Map> getOverlapMap() const;
#endif
    //! Get Tpetra overlapped DOF map
    Teuchos::RCP<const Tpetra_Map> getOverlapMapT() const;

#ifdef ALBANY_EPETRA
    //! Get Epetra Jacobian graph
    Teuchos::RCP<const Epetra_CrsGraph> getJacobianGraph() const;
#endif
    //! Get Tpetra Jacobian graph
    Teuchos::RCP<const Tpetra_CrsGraph> getJacobianGraphT() const;

#ifdef ALBANY_EPETRA
    //! Get Epetra overlap Jacobian graph
    Teuchos::RCP<const Epetra_CrsGraph> getOverlapJacobianGraph() const;
#endif
    //! Get Tpetra overlap Jacobian graph
    Teuchos::RCP<const Tpetra_CrsGraph> getOverlapJacobianGraphT() const;

#ifdef ALBANY_EPETRA
    //! Get Epetra Node map
    Teuchos::RCP<const Epetra_Map> getNodeMap() const; 
#endif
    //! Get Tpetra Node map
    Teuchos::RCP<const Tpetra_Map> getNodeMapT() const; 

    //! Get Node set lists (typedef in Albany_AbstractDiscretization.hpp)
    const NodeSetList& getNodeSets() const { return nodeSets; };
    const NodeSetCoordList& getNodeSetCoords() const { return nodeSetCoords; };

    //! Get Side set lists (typedef in Albany_AbstractDiscretization.hpp)
    const SideSetList& getSideSets(const int workset) const { return sideSets[workset]; };

    //! Get connectivity map from elementGID to workset
    WsLIDList& getElemGIDws() { return elemGIDws; };

    //! Get map from (Ws, El, Local Node) -> NodeLID
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<LO> > > >::type& getWsElNodeEqID() const;

    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> > >::type& getWsElNodeID() const;

    //! Retrieve coodinate vector (num_used_nodes * 3)
    Teuchos::ArrayRCP<double>& getCoordinates() const;

    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& getCoords() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type& getSurfaceHeight() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type& getTemperature() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type& getBasalFriction() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type& getThickness() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type& getFlowFactor() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& getSurfaceVelocity() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& getVelocityRMS() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type& getSphereVolume() const;

    //! Print the coordinates for debugging

    void printCoords() const;

    Albany::StateArrays& getStateArrays() {return stateArrays;}

    //! Retrieve Vector (length num worksets) of element block names
    const Albany::WorksetArray<std::string>::type&  getWsEBNames() const;
    //! Retrieve Vector (length num worksets) of physics set index
    const Albany::WorksetArray<int>::type&  getWsPhysIndex() const;

#ifdef ALBANY_EPETRA
    void writeSolution(const Epetra_Vector& soln, const double time, const bool overlapped = false);
#endif
   
   //Tpetra version of writeSolution  
   void writeSolutionT(const Tpetra_Vector& solnT, const double time, const bool overlapped = false);

#ifdef ALBANY_EPETRA 
    Teuchos::RCP<Epetra_Vector> getSolutionField() const;
#endif
    //Tpetra analog
    Teuchos::RCP<Tpetra_Vector> getSolutionFieldT() const;

    int getSolutionFieldHistoryDepth() const;
#ifdef ALBANY_EPETRA
    Teuchos::RCP<Epetra_MultiVector> getSolutionFieldHistory() const;
    Teuchos::RCP<Epetra_MultiVector> getSolutionFieldHistory(int maxStepCount) const;
    void getSolutionFieldHistory(Epetra_MultiVector &result) const;

    void setResidualField(const Epetra_Vector& residual);
#endif
    //Tpetra analog
    void setResidualFieldT(const Tpetra_Vector& residualT);

    // Retrieve mesh struct
    Teuchos::RCP<Albany::AbstractSTKMeshStruct> getSTKMeshStruct() {return stkMeshStruct;}
    Teuchos::RCP<Albany::AbstractMeshStruct> getMeshStruct() const {return stkMeshStruct;}

    //! Flag if solution has a restart values -- used in Init Cond
    bool hasRestartSolution() const {return stkMeshStruct->hasRestartSolution();}

    //! STK supports MOR
    virtual bool supportsMOR() const { return true; }

    //! If restarting, convenience function to return restart data time
    double restartDataTime() const {return stkMeshStruct->restartDataTime();}

    //! After mesh modification, need to update the element connectivity and nodal coordinates
    void updateMesh();

    //! Function that transforms an STK mesh of a unit cube (for FELIX problems)
    void transformMesh();

    //! Close current exodus file in stk_io and create a new one for an adapted mesh and new results
    void reNameExodusOutput(std::string& filename);

   //! Get number of spatial dimensions
    int getNumDim() const { return stkMeshStruct->numDim; }

    //! Get number of total DOFs per node
    int getNumEq() const { return neq; }

    //! Locate nodal dofs in non-overlapping vectors using local indexing
    int getOwnedDOF(const int inode, const int eq) const;

    //! Locate nodal dofs in overlapping vectors using local indexing
    int getOverlapDOF(const int inode, const int eq) const;

    //! Locate nodal dofs using global indexing
    GO getGlobalDOF(const GO inode, const int eq) const;


    //! used when NetCDF output on a latitude-longitude grid is requested.
    // Each struct contains a latitude/longitude index and it's parametric
    // coordinates in an element.
    struct interp {
      std::pair<double, double> parametric_coords;
      std::pair<unsigned, unsigned> latitude_longitude;
    };

    const stk_classic::mesh::fem::FEMMetaData& getSTKMetaData(){ return metaData; }

    const stk_classic::mesh::BulkData& getSTKBulkData(){ return bulkData; }

  private:

    //! Private to prohibit copying
    STKDiscretization(const STKDiscretization&);

    //! Private to prohibit copying
    STKDiscretization& operator=(const STKDiscretization&);

    inline GO gid(const stk_classic::mesh::Entity& node) const;
    inline GO gid(const stk_classic::mesh::Entity* node) const;

#ifdef ALBANY_EPETRA
    // Copy values from STK Mesh field to given Epetra_Vector
    void getSolutionField(Epetra_Vector &result) const;
#endif
    // Copy values from STK Mesh field to given Tpetra_Vector
    void getSolutionFieldT(Tpetra_Vector &resultT) const;

#ifdef ALBANY_EPETRA
    Teuchos::RCP<Epetra_MultiVector> getSolutionFieldHistoryImpl(int stepCount) const;
    void getSolutionFieldHistoryImpl(Epetra_MultiVector &result) const;

    // Copy solution vector from Epetra_Vector into STK Mesh
    // Here soln is the local (non overlapped) solution
    void setSolutionField(const Epetra_Vector& soln);
#endif
    //Tpetra version of agove
    void setSolutionFieldT(const Tpetra_Vector& solnT);

    // Copy solution vector from Epetra_Vector into STK Mesh
    // Here soln is the local + neighbor (overlapped) solution
#ifdef ALBANY_EPETRA
    void setOvlpSolutionField(const Epetra_Vector& soln);
#endif
    //Tpetra version of above
    void setOvlpSolutionFieldT(const Tpetra_Vector& solnT);

    int nonzeroesPerRow(const int neq) const;
    double monotonicTimeLabel(const double time);

    //! Process STK mesh for Owned nodal quantitites
    void computeOwnedNodesAndUnknowns();
    //! Process coords for ML
    void setupMLCoords();
    //! Process STK mesh for Overlap nodal quantitites
    void computeOverlapNodesAndUnknowns();
    //! Process STK mesh for CRS Graphs
    void computeGraphs();
    //! Process STK mesh for Workset/Bucket Info
    void computeWorksetInfo();
    //! Process STK mesh for NodeSets
    void computeNodeSets();
    //! Process STK mesh for SideSets
    void computeSideSets();
    //! Call stk_io for creating exodus output file
    void setupExodusOutput();
    //! Call stk_io for creating NetCDF output file
    void setupNetCDFOutput();
#ifdef ALBANY_EPETRA
    int processNetCDFOutputRequest(const Epetra_Vector&);
#endif
    //! Find the local side id number within parent element
    unsigned determine_local_side_id( const stk_classic::mesh::Entity & elem , stk_classic::mesh::Entity & side );
    //! Call stk_io for creating exodus output file
    Teuchos::RCP<Teuchos::FancyOStream> out;

    //! Convert the stk mesh on this processor to a nodal graph using SEACAS
    void meshToGraph();

    double previous_time_label;

  protected:


    //! Stk Mesh Objects
    stk_classic::mesh::fem::FEMMetaData& metaData;
    stk_classic::mesh::BulkData& bulkData;

#ifdef ALBANY_EPETRA
    //! Epetra communicator
    Teuchos::RCP<const Epetra_Comm> comm;
#endif

   //! Tpetra communicator and Kokkos node
    Teuchos::RCP<const Teuchos::Comm<int> > commT;

    //! Node map
    Teuchos::RCP<const Tpetra_Map> node_mapT; 

    //! Unknown Map
    Teuchos::RCP<const Tpetra_Map> mapT; 

    //! Overlapped unknown map, and node map
    Teuchos::RCP<const Tpetra_Map> overlap_mapT; 
    Teuchos::RCP<const Tpetra_Map> overlap_node_mapT; 

    //! Jacobian matrix graph
    Teuchos::RCP<Tpetra_CrsGraph> graphT; 

    //! Overlapped Jacobian matrix graph
    Teuchos::RCP<Tpetra_CrsGraph> overlap_graphT; 

    //! Processor ID
    unsigned int myPID;

    //! Number of equations (and unknowns) per node
    const unsigned int neq;

    //! Number of elements on this processor
    unsigned int numMyElements;

    //! node sets stored as std::map(string ID, int vector of GIDs)
    Albany::NodeSetList nodeSets;
    Albany::NodeSetCoordList nodeSetCoords;

    //! side sets stored as std::map(string ID, SideArray classes) per workset (std::vector across worksets)
    std::vector<Albany::SideSetList> sideSets;

    //! Connectivity array [workset, element, local-node, Eq] => LID
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<LO> > > >::type wsElNodeEqID;

    //! Connectivity array [workset, element, local-node] => GID
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> > >::type wsElNodeID;

    mutable Teuchos::ArrayRCP<double> coordinates;
    Albany::WorksetArray<std::string>::type wsEBNames;
    Albany::WorksetArray<int>::type wsPhysIndex;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type coords;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type sHeight;
    Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type temperature;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type basalFriction;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type thickness;
    Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type flowFactor;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type surfaceVelocity;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type velocityRMS;
    Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type sphereVolume;

    //! Connectivity map from elementGID to workset and LID in workset
    WsLIDList  elemGIDws;

    // States: vector of length worksets of a map from field name to shards array
    Albany::StateArrays stateArrays;

    //! list of all owned nodes, saved for setting solution
    std::vector< stk_classic::mesh::Entity * > ownednodes ;
    std::vector< stk_classic::mesh::Entity * > cells ;

    //! list of all overlap nodes, saved for getting coordinates for mesh motion
    std::vector< stk_classic::mesh::Entity * > overlapnodes ;

    //! Number of elements on this processor
    int numOwnedNodes;
    int numOverlapNodes;
    GO numGlobalNodes;

    // Needed to pass coordinates to ML.
    Teuchos::RCP<Piro::MLRigidBodyModes> rigidBodyModes;

    int netCDFp;
    size_t netCDFOutputRequest;
    std::vector<int> varSolns;
    Albany::WorksetArray<Teuchos::ArrayRCP<std::vector<interp> > >::type interpolateData;

    // Storage used in periodic BCs to un-roll coordinates. Pointers saved for destructor.
    std::vector<double*>  toDelete;

    Teuchos::RCP<Albany::AbstractSTKMeshStruct> stkMeshStruct;

    // Used in Exodus writing capability
#ifdef ALBANY_SEACAS
    stk_classic::io::MeshData* mesh_data;

    int outputInterval;
#endif
    bool interleavedOrdering;

  private:

//    MeshGraph nodalGraph;
    Teuchos::RCP<Tpetra_CrsGraph> nodalGraph;


    // find the location of "value" within the first "count" locations of "vector"
    ssize_t in_list(const std::size_t value, std::size_t count, std::size_t *vector) {

      for(std::size_t i=0; i < count; i++) {
        if(vector[i] == value)
          return i;
      }
       return -1;
    }

    ssize_t in_list(const std::size_t value, const Teuchos::Array<GO> &vector) {

      for(std::size_t i=0; i < vector.size(); i++) {
        if(vector[i] == value)
          return i;
      }
       return -1;
    }

    ssize_t in_list(const std::size_t value, std::vector<std::size_t> vector) {

      std::size_t count = vector.size();
      for(std::size_t i=0; i < count; i++) {
        if(vector[i] == value)
          return i;
      }
      return -1;
    }

    ssize_t entity_in_list(const stk_classic::mesh::Entity *value, std::vector<stk_classic::mesh::Entity *> vector) {

      std::size_t count = vector.size();
      for(std::size_t i=0; i < count; i++) {
        if(vector[i]->identifier() == value->identifier())
          return i;
      }
      return -1;
    }

    void printVertexConnectivity();

  };

}

#endif // ALBANY_STKDISCRETIZATION_HPP
