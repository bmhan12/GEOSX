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

/**
 * @file RelativePermeabilityExtrinsicData.hpp
 */

#ifndef GEOSX_CONSTITUTIVE_RELATIVEPERMEABILITY_RELATIVEPERMEABILITYEXTRINSICDATA_HPP_
#define GEOSX_CONSTITUTIVE_RELATIVEPERMEABILITY_RELATIVEPERMEABILITYEXTRINSICDATA_HPP_

#include "common/DataLayouts.hpp"
#include "constitutive/relativePermeability/layouts.hpp"
#include "mesh/ExtrinsicMeshData.hpp"

namespace geosx
{

namespace extrinsicMeshData
{

namespace relperm
{

using array3dLayoutRelPerm = array3d< real64, constitutive::relperm::LAYOUT_RELPERM >;
using array4dLayoutRelPerm_dS = array4d< real64, constitutive::relperm::LAYOUT_RELPERM_DS >;

EXTRINSIC_MESH_DATA_TRAIT( phaseRelPerm,
                           "phaseRelPerm",
                           array3dLayoutRelPerm,
                           0,
                           LEVEL_0,
                           WRITE_AND_READ,
                           "Phase relative permeability" );

EXTRINSIC_MESH_DATA_TRAIT( dPhaseRelPerm_dPhaseVolFraction,
                           "dPhaseRelPerm_dPhaseVolFraction",
                           array4dLayoutRelPerm_dS,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "Derivative of phase relative permeability with respect to phase volume fraction" );

}

}

}

#endif // GEOSX_CONSTITUTIVE_RELATIVEPERMEABILITY_RELATIVEPERMEABILITYEXTRINSICDATA_HPP_