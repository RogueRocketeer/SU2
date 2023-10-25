/*!
 * \file python_wrapper_structure.cpp
 * \brief Driver subroutines that are used by the Python wrapper. Those routines are usually called from an external Python environment.
 * \author D. Thomas
 * \version 8.0.0 "Harrier"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2023, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../Common/include/toolboxes/geometry_toolbox.hpp"
#include "../include/drivers/CDriver.hpp"
#include "../include/drivers/CSinglezoneDriver.hpp"

void CDriver::PreprocessPythonInterface(CConfig** config, CGeometry**** geometry, CSolver***** solver) {
  int rank = MASTER_NODE;
  SU2_MPI::Comm_rank(SU2_MPI::GetComm(), &rank);

  /*--- Initialize boundary conditions customization, this is achieved through the Python wrapper. --- */
  for (iZone = 0; iZone < nZone; iZone++) {
    if (config[iZone]->GetnMarker_PyCustom() > 0) {
      if (rank == MASTER_NODE)
        cout << "----------------- Python Interface Preprocessing ( Zone " << iZone << " ) -----------------" << endl;

      if (rank == MASTER_NODE) cout << "Setting customized boundary conditions for zone " << iZone << endl;
      for (iMesh = 0; iMesh <= config[iZone]->GetnMGLevels(); iMesh++) {
        geometry[iZone][INST_0][iMesh]->SetCustomBoundary(config[iZone]);
      }
      geometry[iZone][INST_0][MESH_0]->UpdateCustomBoundaryConditions(geometry[iZone][INST_0], config[iZone]);

      if ((config[iZone]->GetKind_Solver() == MAIN_SOLVER::EULER) ||
          (config[iZone]->GetKind_Solver() == MAIN_SOLVER::NAVIER_STOKES) ||
          (config[iZone]->GetKind_Solver() == MAIN_SOLVER::RANS) ||
          (config[iZone]->GetKind_Solver() == MAIN_SOLVER::INC_EULER) ||
          (config[iZone]->GetKind_Solver() == MAIN_SOLVER::INC_NAVIER_STOKES) ||
          (config[iZone]->GetKind_Solver() == MAIN_SOLVER::INC_RANS) ||
          (config[iZone]->GetKind_Solver() == MAIN_SOLVER::NEMO_EULER) ||
          (config[iZone]->GetKind_Solver() == MAIN_SOLVER::NEMO_NAVIER_STOKES)) {
        solver[iZone][INST_0][MESH_0][FLOW_SOL]->UpdateCustomBoundaryConditions(geometry[iZone][INST_0], config[iZone]);
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
/* Functions related to the far-field flow variables.                      */
/////////////////////////////////////////////////////////////////////////////

passivedouble CDriver::GetAngleOfAttack() const { return SU2_TYPE::GetValue(main_config->GetAoA()); }

void CDriver::SetAngleOfAttack(const passivedouble AoA) {
  config_container[selected_zone]->SetAoA(AoA);
  solver_container[selected_zone][INST_0][MESH_0][FLOW_SOL]->UpdateFarfieldVelocity(config_container[selected_zone]);
}

passivedouble CDriver::GetAngleOfSideslip() const { return SU2_TYPE::GetValue(main_config->GetAoS()); }

void CDriver::SetAngleOfSideslip(const passivedouble AoS) {
  config_container[selected_zone]->SetAoS(AoS);
  solver_container[selected_zone][INST_0][MESH_0][FLOW_SOL]->UpdateFarfieldVelocity(config_container[selected_zone]);
}

passivedouble CDriver::GetMachNumber() const { return SU2_TYPE::GetValue(main_config->GetMach()); }

void CDriver::SetMachNumber(passivedouble value) {
  main_config->SetMach(value);
  UpdateFarfield();
}

passivedouble CDriver::GetReynoldsNumber() const { return SU2_TYPE::GetValue(main_config->GetReynolds()); }

void CDriver::SetReynoldsNumber(passivedouble value) {
  main_config->SetReynolds(value);
  UpdateFarfield();
}

/////////////////////////////////////////////////////////////////////////////
/* Functions related to the flow solver solution and variables.            */
/////////////////////////////////////////////////////////////////////////////

unsigned long CDriver::GetNumberStateVariables() const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }

  return solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetnVar();
}

unsigned long CDriver::GetNumberPrimitiveVariables() const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }

  return solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetnPrimVar();
}

vector<passivedouble> CDriver::GetSpeedOfSound() const {
  const auto nPoint = GetNumberVertices();

  vector<passivedouble> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetSpeedOfSound(iPoint));
  }

  return values;
}

passivedouble CDriver::GetSpeedOfSound(unsigned long iPoint) const {
  if (main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }

  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  return SU2_TYPE::GetValue(solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetSoundSpeed(iPoint));
}

vector<passivedouble> CDriver::GetMarkerSpeedOfSound(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<passivedouble> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetSpeedOfSound(iVertex));
  }

  return values;
}

passivedouble CDriver::GetMarkerSpeedOfSound(unsigned short iMarker, unsigned long iVertex) const {
  if (main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }

  const auto iPoint = GetMarkerVertexIndices(iMarker, iVertex);

  return SU2_TYPE::GetValue(solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetSoundSpeed(iPoint));
}

/////////////////////////////////////////////////////////////////////////////
/* Functions related to the adjoint flow solver solution.                  */
/////////////////////////////////////////////////////////////////////////////

vector<vector<passivedouble>> CDriver::GetMarkerAdjointForces(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<vector<passivedouble>> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerAdjointForces(iMarker, iVertex));
  }

  return values;
}

vector<passivedouble> CDriver::GetMarkerAdjointForces(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (iVertex >= GetNumberMarkerVertices(iMarker)) {
    SU2_MPI::Error("Vertex index exceeds marker size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetAdjointVertexTractions(iMarker, iVertex, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

void CDriver::SetMarkerAdjointForces(unsigned short iMarker, vector<vector<passivedouble>> values) {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  if (values.size() != nVertex) {
    SU2_MPI::Error("Invalid number of marker vertices!", CURRENT_FUNCTION);
  }

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    SetMarkerAdjointForces(iMarker, iVertex, values[iVertex]);
  }
}

void CDriver::SetMarkerAdjointForces(unsigned short iMarker, unsigned long iVertex, vector<passivedouble> values) {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (iVertex >= GetNumberMarkerVertices(iMarker)) {
    SU2_MPI::Error("Vertex index exceeds marker size.", CURRENT_FUNCTION);
  }
  if (values.size() != nDim) {
    SU2_MPI::Error("Invalid number of dimensions!", CURRENT_FUNCTION);
  }

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->StoreVertexTractionsAdjoint(iMarker, iVertex, iDim, values[iVertex * nDim + iDim]);
  }
}

vector<vector<passivedouble>> CDriver::GetCoordinatesCoordinatesSensitivities() const {
  const auto nPoint = GetNumberVertices();

  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetCoordinatesCoordinatesSensitivities(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetCoordinatesCoordinatesSensitivities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dCoordinates_dCoordinates(iPoint, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetMarkerCoordinatesDisplacementsSensitivities(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<vector<passivedouble>> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerCoordinatesDisplacementsSensitivities(iMarker, iVertex));
  }

  return values;
}

vector<passivedouble> CDriver::GetMarkerCoordinatesDisplacementsSensitivities(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iVertex >= GetNumberMarkerVertices(iMarker)) {
    SU2_MPI::Error("Vertex index exceeds marker size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dCoordinates_dDisplacements(iMarker, iVertex, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<passivedouble> CDriver::GetObjectiveFarfieldVariablesSensitivities() const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }

  const int nTrim = 2;
  vector<passivedouble> values(nTrim, 0.0);

  su2double mach = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetSens_dObjective_dVariables(0);
  values[0] = SU2_TYPE::GetValue(mach);

  su2double alpha = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetSens_dObjective_dVariables(1);
  values[1] = SU2_TYPE::GetValue(alpha);

  return values;
}

vector<passivedouble> CDriver::GetResidualsFarfieldVariablesSensitivities() const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }

  const int nTrim = 2;
  vector<passivedouble> values(nTrim, 0.0);

  su2double mach = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dResiduals_dVariables(0);
  values[0] = SU2_TYPE::GetValue(mach);

  su2double alpha = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dResiduals_dVariables(1);
  values[1] = SU2_TYPE::GetValue(alpha);

  return values;
}

vector<vector<passivedouble>> CDriver::GetObjectiveStatesSensitivities() const {
  const auto nPoint = GetNumberVertices();

  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetObjectiveStatesSensitivities(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetObjectiveStatesSensitivities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  const auto nVar = GetNumberStateVariables();
  vector<passivedouble> values(nVar, 0.0);

  for (auto iVar = 0u; iVar < nVar; iVar++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetSens_dObjective_dStates(iPoint, iVar);

    values[iVar] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetResidualsStatesSensitivities() const {
  const auto nPoint = GetNumberVertices();

  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetResidualsStatesSensitivities(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetResidualsStatesSensitivities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  const auto nVar = GetNumberStateVariables();
  vector<passivedouble> values(nVar, 0.0);

  for (auto iVar = 0u; iVar < nVar; iVar++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dResiduals_dStates(iPoint, iVar);

    values[iVar] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetForcesStatesSensitivities() const {
  const auto nPoint = GetNumberVertices();

  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetForcesStatesSensitivities(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetForcesStatesSensitivities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  const auto nVar = GetNumberStateVariables();
  vector<passivedouble> values(nVar, 0.0);

  for (auto iVar = 0u; iVar < nVar; iVar++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dTractions_dStates(iPoint, iVar);

    values[iVar] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetObjectiveCoordinatesSensitivities() const {
  const auto nPoint = GetNumberVertices();

  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetObjectiveCoordinatesSensitivities(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetObjectiveCoordinatesSensitivities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetSens_dObjective_dCoordinates(iPoint, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetResidualsCoordinatesSensitivities() const {
  const auto nPoint = GetNumberVertices();

  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetResidualsCoordinatesSensitivities(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetResidualsCoordinatesSensitivities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dResiduals_dCoordinates(iPoint, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetForcesCoordinatesSensitivities() const {
  const auto nPoint = GetNumberVertices();

  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetForcesCoordinatesSensitivities(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetForcesCoordinatesSensitivities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dTractions_dCoordinates(iPoint, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetMarkerObjectiveDisplacementsSensitivities(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<vector<passivedouble>> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerObjectiveDisplacementsSensitivities(iMarker, iVertex));
  }

  return values;
}

vector<passivedouble> CDriver::GetMarkerObjectiveDisplacementsSensitivities(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iVertex >= GetNumberMarkerVertices(iMarker)) {
    SU2_MPI::Error("Vertex index exceeds marker size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetSens_dObjective_dDisplacements(iMarker, iVertex, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetMarkerResidualsDisplacementsSensitivities(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<vector<passivedouble>> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerResidualsDisplacementsSensitivities(iMarker, iVertex));
  }

  return values;
}

vector<passivedouble> CDriver::GetMarkerResidualsDisplacementsSensitivities(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iVertex >= GetNumberMarkerVertices(iMarker)) {
    SU2_MPI::Error("Vertex index exceeds marker size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dResiduals_dDisplacements(iMarker, iVertex, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetMarkerForcesDisplacementsSensitivities(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<vector<passivedouble>> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerForcesDisplacementsSensitivities(iMarker, iVertex));
  }

  return values;
}

vector<passivedouble> CDriver::GetMarkerForcesDisplacementsSensitivities(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }
  if (iVertex >= GetNumberMarkerVertices(iMarker)) {
    SU2_MPI::Error("Vertex index exceeds marker size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    const su2double value = solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->GetProd_dTractions_dDisplacements(iMarker, iVertex, iDim);

    values[iDim] = SU2_TYPE::GetValue(value);
  }

  return values;
}

void CDriver::SetAdjointSourceTerm(vector<passivedouble> values) {
  if (!main_config->GetFluidProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != RESIDUALS) {
    SU2_MPI::Error("Discrete adjoint flow solver does not use residual-based formulation!", CURRENT_FUNCTION);
  }

  const auto nPoint = GetNumberVertices();
  const auto nVar = GetNumberStateVariables();

  if (values.size() != nPoint * nVar) {
    SU2_MPI::Error("Size does not match nPoint * nVar!", CURRENT_FUNCTION);
  }

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    for (auto iVar = 0u; iVar < nVar; iVar++) {
      solver_container[ZONE_0][INST_0][MESH_0][ADJFLOW_SOL]->SetAdjoint_SourceTerm(iPoint, iVar, values[iPoint * nVar + iVar]);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////
/* Functions to obtain global parameters from SU2 (time steps, delta t, etc.)   */
//////////////////////////////////////////////////////////////////////////////////

unsigned long CDriver::GetNumberTimeIterations() const { return config_container[selected_zone]->GetnTime_Iter(); }

unsigned long CDriver::GetTimeIteration() const { return TimeIter; }

passivedouble CDriver::GetUnsteadyTimeStep() const {
  return SU2_TYPE::GetValue(config_container[selected_zone]->GetTime_Step());
}

string CDriver::GetSurfaceFileName() const { return config_container[selected_zone]->GetSurfCoeff_FileName(); }

///////////////////////////////////////////////////////////////////////////////
/* Functions related to conjugate heat transfer solver.                      */
///////////////////////////////////////////////////////////////////////////////

vector<vector<passivedouble>> CDriver::GetHeatFluxes() const {
  const auto nPoint = GetNumberVertices();

  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetHeatFluxes(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetHeatFluxes(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  vector<passivedouble> values(nDim, 0.0);

  if (main_config->GetKind_Regime() != ENUM_REGIME::COMPRESSIBLE) {
    return values;
  }

  const auto Prandtl_Lam = main_config->GetPrandtl_Lam();
  const auto Gas_Constant = main_config->GetGas_ConstantND();
  const auto Gamma = main_config->GetGamma();
  const auto Cp = (Gamma / (Gamma - 1.0)) * Gas_Constant;
  const auto laminar_viscosity = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetLaminarViscosity(iPoint);
  const auto thermal_conductivity = Cp * (laminar_viscosity / Prandtl_Lam);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    auto GradT = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetGradient_Primitive(iPoint, 0, iDim);

    values[iDim] = SU2_TYPE::GetValue(-thermal_conductivity * GradT);
  }

  return values;
}

vector<vector<passivedouble>> CDriver::GetMarkerHeatFluxes(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<vector<passivedouble>> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerHeatFluxes(iMarker, iVertex));
  }

  return values;
}

vector<passivedouble> CDriver::GetMarkerHeatFluxes(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }

  const auto iPoint = GetMarkerVertexIndices(iMarker, iVertex);

  vector<passivedouble> values(nDim, 0.0);

  if (main_config->GetKind_Regime() != ENUM_REGIME::COMPRESSIBLE) {
    return values;
  }

  const auto Prandtl_Lam = main_config->GetPrandtl_Lam();
  const auto Gas_Constant = main_config->GetGas_ConstantND();
  const auto Gamma = main_config->GetGamma();
  const auto Cp = (Gamma / (Gamma - 1.0)) * Gas_Constant;
  const auto laminar_viscosity = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetLaminarViscosity(iPoint);
  const auto thermal_conductivity = Cp * (laminar_viscosity / Prandtl_Lam);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    auto GradT = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetGradient_Primitive(iPoint, 0, iDim);

    values[iDim] = SU2_TYPE::GetValue(-thermal_conductivity * GradT);
  }

  return values;
}

vector<passivedouble> CDriver::GetMarkerNormalHeatFluxes(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<passivedouble> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerNormalHeatFluxes(iMarker, iVertex));
  }

  return values;
}

passivedouble CDriver::GetMarkerNormalHeatFluxes(unsigned short iMarker, unsigned long iVertex) const {
  vector<passivedouble> values = GetMarkerHeatFluxes(iMarker, iVertex);
  passivedouble projected = 0.0;

  const auto Normal = main_geometry->vertex[iMarker][iVertex]->GetNormal();
  const auto Area = GeometryToolbox::Norm(nDim, Normal);

  for (auto iDim = 0u; iDim < nDim; iDim++) {
    projected += values[iDim] * SU2_TYPE::GetValue(Normal[iDim] / Area);
  }

  return projected;
}

void CDriver::SetMarkerNormalHeatFluxes(unsigned short iMarker, vector<passivedouble> values) {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  if (values.size() != nVertex) {
    SU2_MPI::Error("Invalid number of marker vertices!", CURRENT_FUNCTION);
  }

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    SetMarkerNormalHeatFluxes(iMarker, iVertex, values[iVertex]);
  }
}

void CDriver::SetMarkerNormalHeatFluxes(unsigned short iMarker, unsigned long iVertex, passivedouble value) {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (iVertex >= GetNumberMarkerVertices(iMarker)) {
    SU2_MPI::Error("Vertex index exceeds marker size.", CURRENT_FUNCTION);
  }

  main_geometry->SetCustomBoundaryHeatFlux(iMarker, iVertex, value);
}

vector<passivedouble> CDriver::GetThermalConductivities() const {
  const auto nPoint = GetNumberVertices();

  vector<passivedouble> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetThermalConductivities(iPoint));
  }

  return values;
}

passivedouble CDriver::GetThermalConductivities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  const auto Prandtl_Lam = main_config->GetPrandtl_Lam();
  const auto Gas_Constant = main_config->GetGas_ConstantND();
  const auto Gamma = main_config->GetGamma();
  const auto Cp = (Gamma / (Gamma - 1.0)) * Gas_Constant;

  const auto laminar_viscosity = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetLaminarViscosity(iPoint);

  return SU2_TYPE::GetValue(Cp * (laminar_viscosity / Prandtl_Lam));
}

vector<passivedouble> CDriver::GetMarkerThermalConductivities(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<passivedouble> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerThermalConductivities(iMarker, iVertex));
  }

  return values;
}

passivedouble CDriver::GetMarkerThermalConductivities(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }

  const auto iPoint = GetMarkerVertexIndices(iMarker, iVertex);

  const auto Prandtl_Lam = main_config->GetPrandtl_Lam();
  const auto Gas_Constant = main_config->GetGas_ConstantND();
  const auto Gamma = main_config->GetGamma();
  const auto Cp = (Gamma / (Gamma - 1.0)) * Gas_Constant;

  const auto laminar_viscosity = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetLaminarViscosity(iPoint);

  return SU2_TYPE::GetValue(Cp * (laminar_viscosity / Prandtl_Lam));
}

vector<passivedouble> CDriver::GetLaminarViscosities() const {
  const auto nPoint = GetNumberVertices();

  vector<passivedouble> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetLaminarViscosities(iPoint));
  }

  return values;
}

passivedouble CDriver::GetLaminarViscosities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  return SU2_TYPE::GetValue(solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetLaminarViscosity(iPoint));
}

vector<passivedouble> CDriver::GetMarkerLaminarViscosities(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<passivedouble> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerLaminarViscosities(iMarker, iVertex));
  }

  return values;
}

passivedouble CDriver::GetMarkerLaminarViscosities(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }

  const auto iPoint = GetMarkerVertexIndices(iMarker, iVertex);

  return SU2_TYPE::GetValue(solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetLaminarViscosity(iPoint));
}

vector<passivedouble> CDriver::GetEddyViscosities() const {
  const auto nPoint = GetNumberVertices();

  vector<passivedouble> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetEddyViscosities(iPoint));
  }

  return values;
}

passivedouble CDriver::GetEddyViscosities(unsigned long iPoint) const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds mesh size.", CURRENT_FUNCTION);
  }

  return SU2_TYPE::GetValue(solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetEddyViscosity(iPoint));
}

vector<passivedouble> CDriver::GetMarkerEddyViscosities(unsigned short iMarker) const {
  const auto nVertex = GetNumberMarkerVertices(iMarker);

  vector<passivedouble> values;

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    values.push_back(GetMarkerLaminarViscosities(iMarker, iVertex));
  }

  return values;
}

passivedouble CDriver::GetMarkerEddyViscosities(unsigned short iMarker, unsigned long iVertex) const {
  if (!main_config->GetFluidProblem()) {
    SU2_MPI::Error("Flow solver is not defined!", CURRENT_FUNCTION);
  }

  const auto iPoint = GetMarkerVertexIndices(iMarker, iVertex);

  return SU2_TYPE::GetValue(solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetEddyViscosity(iPoint));
}

////////////////////////////////////////////////////////////////////////////////
/* Functions related to nonequilibrium flow solver.                           */
////////////////////////////////////////////////////////////////////////////////

unsigned long CDriver::GetNumberNonequilibriumSpecies() const { return main_config->GetnSpecies(); }

unsigned long CDriver::GetNumberNonequilibriumStateVariables() const {
  return GetNumberNonequilibriumSpecies() + nDim + 2;
}

unsigned short CDriver::GetNumberNonequilibriumPrimitiveVariables() const {
  if (main_config->GetKind_Solver() == MAIN_SOLVER::NAVIER_STOKES) {
    return GetNumberNonequilibriumSpecies() + nDim + 10;
  } else {
    return GetNumberNonequilibriumSpecies() + nDim + 8;
  }
}

vector<vector<passivedouble>> CDriver::GetNonequilibriumMassFractions() const {
  if (!main_config->GetNEMOProblem()) {
    SU2_MPI::Error("Nonequilibrium flow solver is not defined!", CURRENT_FUNCTION);
  }

  const auto nPoint = GetNumberVertices();
  vector<vector<passivedouble>> values;

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values.push_back(GetNonequilibriumMassFractions(iPoint));
  }

  return values;
}

vector<passivedouble> CDriver::GetNonequilibriumMassFractions(unsigned long iPoint) const {
  if (!main_config->GetNEMOProblem()) {
    SU2_MPI::Error("Nonequilibrium flow solver is not defined!", CURRENT_FUNCTION);
  }
  if (iPoint >= GetNumberVertices()) {
    SU2_MPI::Error("Vertex index exceeds size.", CURRENT_FUNCTION);
  }

  const auto nSpecies = GetNumberNonequilibriumSpecies();
  vector<passivedouble> values;

  for (auto iSpecies = 0u; iSpecies < nSpecies; iSpecies++) {
    auto rho_s = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetSolution(iPoint, iSpecies);
    auto rho_t = solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetDensity(iPoint);

    values.push_back(SU2_TYPE::GetValue(rho_s / rho_t));
  }

  return values;
}

vector<passivedouble> CDriver::GetVibrationalTemperatures() const {
  if (!main_config->GetNEMOProblem()) {
    SU2_MPI::Error("Nonequilibrium flow solver is not defined!", CURRENT_FUNCTION);
  }

  const auto nPoint = GetNumberVertices();
  vector<passivedouble> values(nPoint, 0.0);

  for (auto iPoint = 0ul; iPoint < nPoint; iPoint++) {
    values[iPoint] = SU2_TYPE::GetValue(solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->GetNodes()->GetTemperature_ve(iPoint));
  }

  return values;
}

////////////////////////////////////////////////////////////////////////////////
/* Functions related to the management of markers.                            */
////////////////////////////////////////////////////////////////////////////////

vector<string> CDriver::GetFluidLoadMarkerTags() const {
  const auto nMarker = main_config->GetnMarker_Fluid_Load();
  vector<string> tags;

  tags.resize(nMarker);

  for (auto iMarker = 0u; iMarker < nMarker; iMarker++) {
    tags[iMarker] = main_config->GetMarker_Fluid_Load_TagBound(iMarker);
  }

  return tags;
}

void CDriver::SetHeatSourcePosition(passivedouble alpha, passivedouble pos_x, passivedouble pos_y,
                                     passivedouble pos_z) {
  CSolver* solver = solver_container[selected_zone][INST_0][MESH_0][RAD_SOL];

  config_container[selected_zone]->SetHeatSource_Rot_Z(alpha);
  config_container[selected_zone]->SetHeatSource_Center(pos_x, pos_y, pos_z);

  solver->SetVolumetricHeatSource(geometry_container[selected_zone][INST_0][MESH_0], config_container[selected_zone]);
}

void CDriver::SetInletAngle(unsigned short iMarker, passivedouble alpha) {
  su2double alpha_rad = alpha * PI_NUMBER / 180.0;

  for (auto iVertex = 0ul; iVertex < geometry_container[ZONE_0][INST_0][MESH_0]->nVertex[iMarker]; iVertex++) {
    solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->SetInlet_FlowDir(iMarker, iVertex, 0, cos(alpha_rad));
    solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->SetInlet_FlowDir(iMarker, iVertex, 1, sin(alpha_rad));
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Functions related to simulation control, high level functions (reset convergence, set initial mesh, etc.)   */
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CSinglezoneDriver::SetInitialMesh() {
  DynamicMeshUpdate(0);

  SU2_OMP_PARALLEL {
    for (iMesh = 0u; iMesh <= main_config->GetnMGLevels(); iMesh++) {
      SU2_OMP_FOR_STAT(roundUpDiv(geometry_container[selected_zone][INST_0][iMesh]->GetnPoint(), omp_get_max_threads()))
      for (auto iPoint = 0ul; iPoint < geometry_container[selected_zone][INST_0][iMesh]->GetnPoint(); iPoint++) {
        /*--- Overwrite fictitious velocities. ---*/
        su2double Grid_Vel[3] = {0.0, 0.0, 0.0};

        /*--- Set the grid velocity for this coarse node. ---*/
        geometry_container[selected_zone][INST_0][iMesh]->nodes->SetGridVel(iPoint, Grid_Vel);
      }
      END_SU2_OMP_FOR
      /*--- Push back the volume. ---*/
      geometry_container[selected_zone][INST_0][iMesh]->nodes->SetVolume_n();
      geometry_container[selected_zone][INST_0][iMesh]->nodes->SetVolume_nM1();
    }
    /*--- Push back the solution so that there is no fictitious velocity at the next step. ---*/
    solver_container[selected_zone][INST_0][MESH_0][MESH_SOL]->GetNodes()->Set_Solution_time_n();
    solver_container[selected_zone][INST_0][MESH_0][MESH_SOL]->GetNodes()->Set_Solution_time_n1();
  }
  END_SU2_OMP_PARALLEL
}

void CDriver::UpdateBoundaryConditions() {
  int rank = MASTER_NODE;

  SU2_MPI::Comm_rank(SU2_MPI::GetComm(), &rank);

  if (rank == MASTER_NODE) cout << "Updating boundary conditions." << endl;
  for (auto iZone = 0u; iZone < nZone; iZone++) {
    geometry_container[iZone][INST_0][MESH_0]->UpdateCustomBoundaryConditions(geometry_container[iZone][INST_0], config_container[iZone]);
  }
}

void CDriver::UpdateGeometry() {
  geometry_container[ZONE_0][INST_0][MESH_0]->InitiateComms(main_geometry, main_config, COORDINATES);
  geometry_container[ZONE_0][INST_0][MESH_0]->CompleteComms(main_geometry, main_config, COORDINATES);

  geometry_container[ZONE_0][INST_0][MESH_0]->SetControlVolume(main_config, UPDATE);
  geometry_container[ZONE_0][INST_0][MESH_0]->SetBoundControlVolume(main_config, UPDATE);
  geometry_container[ZONE_0][INST_0][MESH_0]->SetMaxLength(main_config);
}

void CDriver::UpdateFarfield() {
  su2double Velocity_Ref = main_config->GetVelocity_Ref();
  su2double Alpha = main_config->GetAoA() * PI_NUMBER / 180.0;
  su2double Beta = main_config->GetAoS() * PI_NUMBER / 180.0;
  su2double Mach = main_config->GetMach();
  su2double Temperature = main_config->GetTemperature_FreeStream();
  su2double Gas_Constant = main_config->GetGas_Constant();
  su2double Gamma = main_config->GetGamma();
  su2double SoundSpeed = sqrt(Gamma * Gas_Constant * Temperature);

  if (nDim == 2) {
    main_config->GetVelocity_FreeStreamND()[0] = cos(Alpha) * Mach * SoundSpeed / Velocity_Ref;
    main_config->GetVelocity_FreeStreamND()[1] = sin(Alpha) * Mach * SoundSpeed / Velocity_Ref;
  }
  if (nDim == 3) {
    main_config->GetVelocity_FreeStreamND()[0] = cos(Alpha) * cos(Beta) * Mach * SoundSpeed / Velocity_Ref;
    main_config->GetVelocity_FreeStreamND()[1] = sin(Beta) * Mach * SoundSpeed / Velocity_Ref;
    main_config->GetVelocity_FreeStreamND()[2] = sin(Alpha) * Mach * SoundSpeed / Velocity_Ref;
  }
}

////////////////////////////////////////////////////////////////////////////////
/* Functions related to adjoint finite element simulations.                    */
////////////////////////////////////////////////////////////////////////////////

vector<passivedouble> CDriver::GetMarkerForceSensitivities(unsigned short iMarker) const {
  if (!main_config->GetStructuralProblem() || !main_config->GetDiscrete_Adjoint()) {
    SU2_MPI::Error("Discrete adjoint structural solver is not defined!", CURRENT_FUNCTION);
  }
  if (main_config->GetKind_DiscreteAdjoint() != FIXED_POINT) {
    SU2_MPI::Error("Discrete adjoint structural solver does not use fixed-point formulation!", CURRENT_FUNCTION);
  }

  const auto nVertex = GetNumberMarkerVertices(iMarker);
  vector<passivedouble> values(nVertex * nDim, 0.0);

  for (auto iVertex = 0ul; iVertex < nVertex; iVertex++) {
    auto iPoint = main_geometry->vertex[iMarker][iVertex]->GetNode();

    for (auto iDim = 0u; iDim < nDim; iDim++) {
      values[iPoint * nDim + iDim] = SU2_TYPE::GetValue(
          solver_container[ZONE_0][INST_0][MESH_0][ADJFEA_SOL]->GetNodes()->GetFlowTractionSensitivity(iPoint, iDim));
    }
  }

  return values;
}

////////////////////////////////////////////////////////////////////////////////
/* Functions related to dynamic mesh                                          */
////////////////////////////////////////////////////////////////////////////////

void CDriver::SetTranslationRate(passivedouble xDot, passivedouble yDot, passivedouble zDot) {
  main_config->SetTranslation_Rate(0, xDot);
  main_config->SetTranslation_Rate(1, yDot);
  main_config->SetTranslation_Rate(2, zDot);
}

void CDriver::SetRotationRate(passivedouble rot_x, passivedouble rot_y, passivedouble rot_z) {
  main_config->SetRotation_Rate(0, rot_x);
  main_config->SetRotation_Rate(1, rot_y);
  main_config->SetRotation_Rate(2, rot_z);
}
