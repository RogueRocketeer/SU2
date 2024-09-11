/*!
 * \file CFluidCantera.cpp
 * \brief Defines the multicomponent incompressible Ideal Gas model for reacting flows.
 * \author T. Economon, Cristopher Morales Ubal
 * \version 8.0.1 "Harrier"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2024, SU2 Contributors (cf. AUTHORS.md)
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

#include "../../include/fluid/CFluidCantera.hpp"

#include <cmath>

#include <numeric>

#include "../../include/fluid/CConstantConductivity.hpp"
#include "../../include/fluid/CConstantConductivityRANS.hpp"
#include "../../include/fluid/CConstantPrandtl.hpp"
#include "../../include/fluid/CConstantPrandtlRANS.hpp"
#include "../../include/fluid/CConstantViscosity.hpp"
#include "../../include/fluid/CPolynomialConductivity.hpp"
#include "../../include/fluid/CPolynomialConductivityRANS.hpp"
#include "../../include/fluid/CPolynomialViscosity.hpp"
#include "../../include/fluid/CSutherland.hpp"
#include "../../Common/include/basic_types/ad_structure.hpp"

// #include <cantera/core.h>
#include "/home/cristopher/codes/cantera/include/cantera/core.h"
#include <fstream>
#include <iostream>

using namespace Cantera;
using namespace SU2_TYPE;

CFluidCantera::CFluidCantera(su2double val_Cp, su2double val_gas_constant, su2double value_pressure_operating,
                           const CConfig* config)
    : CFluidModel(),
      n_species_mixture(config->GetnSpecies() + 1),
      Gas_Constant(val_gas_constant),
      Gamma(config->GetGamma()),
      Pressure_Thermodynamic(value_pressure_operating),
      GasConstant_Ref(config->GetGas_Constant_Ref()),
      Prandtl_Number(config->GetPrandtl_Turb()),
      wilke(config->GetKind_MixingViscosityModel() == MIXINGVISCOSITYMODEL::WILKE),
      davidson(config->GetKind_MixingViscosityModel() == MIXINGVISCOSITYMODEL::DAVIDSON) {
  if (n_species_mixture > ARRAYSIZE) {
    SU2_MPI::Error("Too many species, increase ARRAYSIZE", CURRENT_FUNCTION);
  }

  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    molarMasses[iVar] = config->GetMolecular_Weight(iVar);
    specificHeat[iVar] = config->GetSpecific_Heat_CpND(iVar);
  }

  SetLaminarViscosityModel(config);
  SetThermalConductivityModel(config);
  SetMassDiffusivityModel(config);
}

void CFluidCantera::SetLaminarViscosityModel(const CConfig* config) {
  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    LaminarViscosityPointers[iVar] = MakeLaminarViscosityModel(config, iVar);
  }
}

void CFluidCantera::SetThermalConductivityModel(const CConfig* config) {
  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    ThermalConductivityPointers[iVar] = MakeThermalConductivityModel(config, iVar);
  }
}

void CFluidCantera::SetMassDiffusivityModel(const CConfig* config) {
  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    MassDiffusivityPointers[iVar] = MakeMassDiffusivityModel(config, iVar);
  }
}

void CFluidCantera::ComputeMassDiffusivity() {
  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    MassDiffusivityPointers[iVar]->SetDiffusivity(Density, Mu, Cp, Kt);
    massDiffusivity[iVar] = MassDiffusivityPointers[iVar]->GetDiffusivity();
  }
}

void CFluidCantera::MassToMoleFractions(const su2double* val_scalars) {

  su2double val_scalars_sum{0.0};
  for (int i_scalar = 0; i_scalar < n_species_mixture - 1; i_scalar++) {
    massFractions[i_scalar] = val_scalars[i_scalar];
    val_scalars_sum += val_scalars[i_scalar];
  }
  massFractions[n_species_mixture - 1] = 1 - val_scalars_sum;

  su2double mixtureMolarMass{0.0};
  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    mixtureMolarMass += massFractions[iVar] / molarMasses[iVar];
  }

  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    moleFractions[iVar] = (massFractions[iVar] / molarMasses[iVar]) / mixtureMolarMass;
  }
}

su2double CFluidCantera::WilkeViscosity(const su2double* val_scalars) {

  /* Fill laminarViscosity with n_species_mixture viscosity values. */
  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    LaminarViscosityPointers[iVar]->SetViscosity(Temperature, Density);
    laminarViscosity[iVar] = LaminarViscosityPointers[iVar]->GetViscosity();
  }

  su2double viscosityMixture = 0.0;

  for (int i = 0; i < n_species_mixture; i++) {
    su2double wilkeDenumerator = 0.0;
    for (int j = 0; j < n_species_mixture; j++) {
      if (j != i) {
        const su2double phi =
            pow(1 + sqrt(laminarViscosity[i] / laminarViscosity[j]) * pow(molarMasses[j] / molarMasses[i], 0.25), 2) /
            sqrt(8 * (1 + molarMasses[i] / molarMasses[j]));
        wilkeDenumerator += moleFractions[j] * phi;
      } else {
        wilkeDenumerator += moleFractions[j];
      }
    }
    const su2double wilkeNumerator = moleFractions[i] * laminarViscosity[i];
    viscosityMixture += wilkeNumerator / wilkeDenumerator;
  }
  return viscosityMixture;
}

su2double CFluidCantera::DavidsonViscosity(const su2double* val_scalars) {
  const su2double A = 0.375;

  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    LaminarViscosityPointers[iVar]->SetViscosity(Temperature, Density);
    laminarViscosity[iVar] = LaminarViscosityPointers[iVar]->GetViscosity();
  }

  su2double mixtureFractionDenumerator = 0.0;
  for (int i = 0; i < n_species_mixture; i++) {
    mixtureFractionDenumerator += moleFractions[i] * sqrt(molarMasses[i]);
  }

  std::array<su2double, ARRAYSIZE> mixtureFractions;
  for (int j = 0; j < n_species_mixture; j++) {
    mixtureFractions[j] = (moleFractions[j] * sqrt(molarMasses[j])) / mixtureFractionDenumerator;
  }

  su2double fluidity = 0.0;
  for (int i = 0; i < n_species_mixture; i++) {
    for (int j = 0; j < n_species_mixture; j++) {
      const su2double E = (2 * sqrt(molarMasses[i]) * sqrt(molarMasses[j])) / (molarMasses[i] + molarMasses[j]);
      fluidity +=
          ((mixtureFractions[i] * mixtureFractions[j]) / (sqrt(laminarViscosity[i]) * sqrt(laminarViscosity[j]))) *
          pow(E, A);
    }
  }
  return 1.0 / fluidity;
}

su2double CFluidCantera::WilkeConductivity(const su2double* val_scalars) {

  for (int iVar = 0; iVar < n_species_mixture; iVar++) {
    ThermalConductivityPointers[iVar]->SetConductivity(Temperature, Density, Mu, 0.0, 0.0, 0.0, 0.0);
    laminarThermalConductivity[iVar] = ThermalConductivityPointers[iVar]->GetConductivity();
  }

  su2double conductivityMixture = 0.0;

  for (int i = 0; i < n_species_mixture; i++) {
    su2double wilkeDenumerator = 0.0;
    for (int j = 0; j < n_species_mixture; j++) {
      if (j != i) {
        const su2double phi =
            pow(1 + sqrt(laminarViscosity[i] / laminarViscosity[j]) * pow(molarMasses[j] / molarMasses[i], 0.25), 2) /
            sqrt(8 * (1 + molarMasses[i] / molarMasses[j]));
        wilkeDenumerator += moleFractions[j] * phi;
      } else {
        wilkeDenumerator += moleFractions[j];
      }
    }
    const su2double wilkeNumerator = moleFractions[i] * laminarThermalConductivity[i];
    conductivityMixture += wilkeNumerator / wilkeDenumerator;
  }
  return conductivityMixture;
}

su2double CFluidCantera::ComputeGasConstant() {
  su2double MeanMolecularWeight = 0.0;

  for (int i = 0; i < n_species_mixture; i++) {
    MeanMolecularWeight += moleFractions[i] * molarMasses[i] / 1000;
  }
  Gas_Constant = UNIVERSAL_GAS_CONSTANT / (GasConstant_Ref * MeanMolecularWeight);

  return Gas_Constant;
}

su2double CFluidCantera::ComputeMeanSpecificHeatCp(const su2double* val_scalars) {
  su2double mean_cp = 0.0;

  for (int i_scalar = 0; i_scalar < n_species_mixture; i_scalar++) {
    mean_cp += specificHeat[i_scalar] * massFractions[i_scalar];
  }
  return mean_cp;
}

void CFluidCantera::SetTDState_T(const su2double val_temperature, const su2double* val_scalars) {
  auto sol = newSolution("h2o2.yaml");
  auto gas = sol->thermo();

  // Set the thermodynamic state by specifying T (500 K) P (2 atm) and the mole
  // fractions. Note that the mole fractions do not need to sum to 1.0 - they will
  // be normalized internally. Also, the values for any unspecified species will be
  // set to zero.
  gas->setState_TPX(GetValue(Temperature), GetValue(Pressure_Thermodynamic), "H2O:1.0, H2:8.0, AR:1.0");

  // Print a summary report of the state of the gas.
  MassToMoleFractions(val_scalars);
  ComputeGasConstant();
  Temperature = val_temperature;
  Density = Pressure_Thermodynamic / (Temperature * Gas_Constant);
  Cp = ComputeMeanSpecificHeatCp(val_scalars);
  Cv = Cp - Gas_Constant;

  if (wilke) {
    Mu = WilkeViscosity(val_scalars);
  } else if (davidson) {
    Mu = DavidsonViscosity(val_scalars);
  }

  Kt = WilkeConductivity(val_scalars);
  ComputeMassDiffusivity();
}