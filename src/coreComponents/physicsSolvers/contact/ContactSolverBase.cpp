/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 TotalEnergies
 * Copyright (c) 2019-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/*
 * ContactSolverBase.cpp
 */

#include "ContactSolverBase.hpp"

#include "common/TimingMacros.hpp"
#include "constitutive/ConstitutiveManager.hpp"
#include "constitutive/contact/ContactSelector.hpp"
#include "constitutive/solid/ElasticIsotropic.hpp"
#include "finiteElement/elementFormulations/FiniteElementBase.hpp"
#include "linearAlgebra/utilities/LAIHelperFunctions.hpp"
#include "mesh/DomainPartition.hpp"
#include "fieldSpecification/FieldSpecificationManager.hpp"
#include "mesh/NodeManager.hpp"
#include "mesh/SurfaceElementRegion.hpp"
#include "physicsSolvers/solidMechanics/SolidMechanicsLagrangianFEM.hpp"
#include "common/GEOS_RAJA_Interface.hpp"

namespace geos
{

using namespace dataRepository;
using namespace constitutive;
using namespace fields::contact;

ContactSolverBase::ContactSolverBase( const string & name,
                                      Group * const parent ):
  SolverBase( name, parent ),
  m_solidSolver( nullptr ),
  m_setupSolidSolverDofs( true )
{
  registerWrapper( viewKeyStruct::solidSolverNameString(), &m_solidSolverName ).
    setRTTypeName( rtTypes::CustomTypes::groupNameRef ).
    setInputFlag( InputFlags::REQUIRED ).
    setDescription( "Name of the solid mechanics solver in the rock matrix" );

  registerWrapper( viewKeyStruct::contactRelationNameString(), &m_contactRelationName ).
    setRTTypeName( rtTypes::CustomTypes::groupNameRef ).
    setInputFlag( InputFlags::REQUIRED ).
    setDescription( "Name of contact relation to enforce constraints on fracture boundary." );

  registerWrapper( viewKeyStruct::fractureRegionNameString(), &m_fractureRegionName ).
    setRTTypeName( rtTypes::CustomTypes::groupNameRef ).
    setInputFlag( InputFlags::REQUIRED ).
    setDescription( "Name of the fracture region." );

}

void ContactSolverBase::postProcessInput()
{
  m_solidSolver = &this->getParent().getGroup< SolidMechanicsLagrangianFEM >( m_solidSolverName );
  SolverBase::postProcessInput();
}

void ContactSolverBase::registerDataOnMesh( dataRepository::Group & meshBodies )
{
  using namespace fields::contact;

  forDiscretizationOnMeshTargets( meshBodies,
                                  [&]( string const,
                                       MeshLevel & meshLevel,
                                       arrayView1d< string const > const regionNames )
  {
    ElementRegionManager & elemManager = meshLevel.getElemManager();
    elemManager.forElementRegions< SurfaceElementRegion >( regionNames,
                                                           [&] ( localIndex const,
                                                                 SurfaceElementRegion & region )
    {
      string const labels[3] = { "normal", "tangent1", "tangent2" };

      region.forElementSubRegions< SurfaceElementSubRegion >( [&]( SurfaceElementSubRegion & subRegion )
      {
        subRegion.registerField< dispJump >( getName() ).
          setDimLabels( 1, labels ).
          reference().resizeDimension< 1 >( 3 );

        subRegion.registerField< deltaDispJump >( getName() ).
          reference().resizeDimension< 1 >( 3 );

        subRegion.registerField< oldDispJump >( getName() ).
          reference().resizeDimension< 1 >( 3 );

        subRegion.registerField< traction >( getName() ).
          setDimLabels( 1, labels ).
          reference().resizeDimension< 1 >( 3 );

        subRegion.registerField< fractureState >( getName() );

        subRegion.registerField< oldFractureState >( getName() );
      } );
    } );
  } );
}

void ContactSolverBase::computeFractureStateStatistics( MeshLevel const & mesh,
                                                        globalIndex & numStick,
                                                        globalIndex & numSlip,
                                                        globalIndex & numOpen ) const
{
  ElementRegionManager const & elemManager = mesh.getElemManager();

  array1d< globalIndex > localCounter( 3 );

  elemManager.forElementSubRegions< SurfaceElementSubRegion >( [&]( SurfaceElementSubRegion const & subRegion )
  {
    arrayView1d< integer const > const & ghostRank = subRegion.ghostRank();
    arrayView1d< integer const > const & fractureState = subRegion.getField< fields::contact::fractureState >();

    RAJA::ReduceSum< parallelHostReduce, localIndex > stickCount( 0 ), slipCount( 0 ), openCount( 0 );
    forAll< parallelHostPolicy >( subRegion.size(), [=] ( localIndex const kfe )
    {
      if( ghostRank[kfe] < 0 )
      {
        switch( fractureState[kfe] )
        {
          case FractureState::Stick:
            {
              stickCount += 1;
              break;
            }
          case FractureState::NewSlip:
          case FractureState::Slip:
            {
              slipCount += 1;
              break;
            }
          case FractureState::Open:
            {
              openCount += 1;
              break;
            }
        }
      }
    } );

    localCounter[0] += stickCount.get();
    localCounter[1] += slipCount.get();
    localCounter[2] += openCount.get();
  } );

  array1d< globalIndex > totalCounter( 3 );

  MpiWrapper::allReduce( localCounter.data(),
                         totalCounter.data(),
                         3,
                         MPI_SUM,
                         MPI_COMM_GEOSX );

  numStick = totalCounter[0];
  numSlip  = totalCounter[1];
  numOpen  = totalCounter[2];
}

void ContactSolverBase::outputConfigurationStatistics( DomainPartition const & domain ) const
{
  if( getLogLevel() >=1 )
  {
    globalIndex numStick = 0;
    globalIndex numSlip  = 0;
    globalIndex numOpen  = 0;

    forDiscretizationOnMeshTargets( domain.getMeshBodies(), [&]( string const &,
                                                                 MeshLevel const & mesh,
                                                                 arrayView1d< string const > const & )
    {
      computeFractureStateStatistics( mesh, numStick, numSlip, numOpen );

      GEOS_LOG_RANK_0( GEOS_FMT( "  Number of element for each fracture state:"
                                 " stick: {:12} | slip:  {:12} | open:  {:12}",
                                 numStick, numSlip, numOpen ) );
    } );
  }
}

void ContactSolverBase::applyBoundaryConditions( real64 const time,
                                                 real64 const dt,
                                                 DomainPartition & domain,
                                                 DofManager const & dofManager,
                                                 CRSMatrixView< real64, globalIndex const > const & localMatrix,
                                                 arrayView1d< real64 > const & localRhs )
{
  GEOS_MARK_FUNCTION;

  if( m_setupSolidSolverDofs )
  {
    m_solidSolver->applyBoundaryConditions( time,
                                            dt,
                                            domain,
                                            dofManager,
                                            localMatrix,
                                            localRhs );
  }
}

real64 ContactSolverBase::explicitStep( real64 const & GEOS_UNUSED_PARAM( time_n ),
                                        real64 const & dt,
                                        const int GEOS_UNUSED_PARAM( cycleNumber ),
                                        DomainPartition & GEOS_UNUSED_PARAM( domain ) )
{
  GEOS_MARK_FUNCTION;
  GEOS_ERROR( getDataContext() << ": ExplicitStep non available for contact solvers." );
  return dt;
}

void ContactSolverBase::synchronizeFractureState( DomainPartition & domain ) const
{
  forDiscretizationOnMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                                MeshLevel & mesh,
                                                                arrayView1d< string const > const & )
  {
    FieldIdentifiers fieldsToBeSync;

    fieldsToBeSync.addElementFields( { fields::contact::fractureState::key() }, { getFractureRegionName() } );

    CommunicationTools::getInstance().synchronizeFields( fieldsToBeSync,
                                                         mesh,
                                                         domain.getNeighbors(),
                                                         true );
  } );
}

} /* namespace geos */
