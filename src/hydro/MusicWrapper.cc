/*******************************************************************************
 * Copyright (c) The JETSCAPE Collaboration, 2018
 *
 * Modular, task-based framework for simulating all aspects of heavy-ion collisions
 * 
 * For the list of contributors see AUTHORS.
 *
 * Report issues at https://github.com/JETSCAPE/JETSCAPE/issues
 *
 * or via email to bugs.jetscape@gmail.com
 *
 * Distributed under the GNU General Public License 3.0 (GPLv3 or later).
 * See COPYING for details.
 ******************************************************************************/

#include <stdio.h>
#include <sys/stat.h>
#include <MakeUniqueHelper.h>

#include <string>
#include <vector>

#include "JetScapeLogger.h"
#include "MusicWrapper.h"

using namespace Jetscape;

// Register the module with the base class
RegisterJetScapeModule<MusicWrapper> MusicWrapper::reg("MUSIC");

MpiMusic::MpiMusic() {
    hydro_status = NOT_START;
    doCooperFrye = 0;
    flag_output_evo_to_file = 0;
    SetId("MUSIC");
}


MpiMusic::~MpiMusic() {
    if (hydro_status != NOT_START) {
        delete music_hydro_ptr;
    }
}


void MpiMusic::InitializeHydro(Parameter parameter_list) {
    JSINFO << "Initialize MUSIC ...";
    VERBOSE(8);

    string input_file = GetXMLElementText({"Hydro", "MUSIC", "MUSIC_input_file"});
    doCooperFrye = GetXMLElementInt({"Hydro", "MUSIC", "Perform_CooperFrye_Feezeout"});
  
    music_hydro_ptr = new MUSIC(input_file);

    // overwrite input options
    int flag_output_evo_to_file = GetXMLElementInt({"Hydro", "MUSIC", "output_evolution_to_file"});
    music_hydro_ptr->set_parameter("output_movie_flag",
                                static_cast<double>(flag_output_evo_to_file));
    double eta_over_s = GetXMLElementDouble({"Hydro", "MUSIC", "shear_viscosity_eta_over_s"});

    if (eta_over_s > 1e-6) {
        music_hydro_ptr->set_parameter("Viscosity_Flag_Yes_1_No_0", 1);
        music_hydro_ptr->set_parameter("Include_Shear_Visc_Yes_1_No_0", 1);
        music_hydro_ptr->set_parameter("Shear_to_S_ratio", eta_over_s);
    } else if (eta_over_s >= 0.) {
        music_hydro_ptr->set_parameter("Viscosity_Flag_Yes_1_No_0", 0);
        music_hydro_ptr->set_parameter("Include_Shear_Visc_Yes_1_No_0", 0);
    } else {
        JSWARN << "The input shear viscosity is negative! eta/s = "
               << eta_over_s;
        exit(1);
    }
}


void MpiMusic::EvolveHydro() {
    VERBOSE(8);
    JSINFO << "Initialize density profiles in MUSIC ...";
    std::vector<double> entropy_density = ini->GetEntropyDensityDistribution();
    double dx = ini->GetXStep();
    double dz = ini->GetZStep();
    double z_max  = ini->GetZMax();
    int nz = ini->GetZSize();
    if (pre_eq_ptr == nullptr) {
        JSWARN << "Missing the pre-equilibrium module ...";
    } else {
        music_hydro_ptr->initialize_hydro_from_jetscape_preequilibrium_vectors(
                dx, dz, z_max, nz,
                pre_eq_ptr->e_,
                pre_eq_ptr->utau_, pre_eq_ptr->ux_,
                pre_eq_ptr->uy_,   pre_eq_ptr->ueta_,
                pre_eq_ptr->pi00_, pre_eq_ptr->pi01_, pre_eq_ptr->pi02_,
                pre_eq_ptr->pi03_, pre_eq_ptr->pi11_, pre_eq_ptr->pi12_,
                pre_eq_ptr->pi13_, pre_eq_ptr->pi22_, pre_eq_ptr->pi23_,
                pre_eq_ptr->pi33_, pre_eq_ptr->bulk_Pi_);
    }
    
    JSINFO << "initial density profile dx = " << dx << " fm";
    hydro_status = INITIALIZED;
    if (hydro_status == INITIALIZED) {
        JSINFO << "running MUSIC ...";
        music_hydro_ptr->run_hydro();
        hydro_status = FINISHED;
    }

    if (flag_output_evo_to_file == 1) {
        PassHydroEvolutionHistoryToFramework();
        JSINFO << "number of fluid cells received by the JETSCAPE: "
               << bulk_info.data.size();
        music_hydro_ptr->clear_hydro_info_from_memory();

        FindAConstantTemperatureSurface(0.16);
    }
    
    collect_freeze_out_surface();
    
    if (hydro_status == FINISHED && doCooperFrye == 1) {
        music_hydro_ptr->run_Cooper_Frye();
    }
}


void MpiMusic::collect_freeze_out_surface() {
    system("cat surface_eps* >> surface.dat");
    system("rm surface_eps* 2> /dev/null");
}


void MpiMusic::SetHydroGridInfo() {
    bulk_info.neta    = music_hydro_ptr->get_neta();
    bulk_info.ntau    = music_hydro_ptr->get_ntau();
    bulk_info.nx      = music_hydro_ptr->get_nx();
    bulk_info.ny      = music_hydro_ptr->get_nx();
    bulk_info.tau_min = music_hydro_ptr->get_hydro_tau0();
    bulk_info.dtau    = music_hydro_ptr->get_hydro_dtau();
    bulk_info.x_min   = -music_hydro_ptr->get_hydro_x_max();
    bulk_info.dx      = music_hydro_ptr->get_hydro_dx();
    bulk_info.y_min   = -music_hydro_ptr->get_hydro_x_max();
    bulk_info.dy      = music_hydro_ptr->get_hydro_dx();
    bulk_info.eta_min = -music_hydro_ptr->get_hydro_eta_max();
    bulk_info.deta    = music_hydro_ptr->get_hydro_deta();
    
    bulk_info.boost_invariant = music_hydro_ptr->is_boost_invariant();
}


void MpiMusic::PassHydroEvolutionHistoryToFramework() {
    JSINFO << "Passing hydro evolution information to JETSCAPE ... ";
    auto number_of_cells = music_hydro_ptr->get_number_of_fluid_cells();
    JSINFO << "total number of fluid cells: " << number_of_cells;
    
    SetHydroGridInfo();

    fluidCell *fluidCell_ptr = new fluidCell;
    for (int i = 0; i < number_of_cells; i++) {
        std::unique_ptr<FluidCellInfo> fluid_cell_info_ptr(new FluidCellInfo);
        music_hydro_ptr->get_fluid_cell_with_index(i, fluidCell_ptr);

        fluid_cell_info_ptr->energy_density = fluidCell_ptr->ed;
        fluid_cell_info_ptr->entropy_density = fluidCell_ptr->sd;
        fluid_cell_info_ptr->temperature = fluidCell_ptr->temperature;
        fluid_cell_info_ptr->pressure = fluidCell_ptr->pressure;
        fluid_cell_info_ptr->vx = fluidCell_ptr->vx;
        fluid_cell_info_ptr->vy = fluidCell_ptr->vy;
        fluid_cell_info_ptr->vz = fluidCell_ptr->vz;
        fluid_cell_info_ptr->mu_B = 0.0;
        fluid_cell_info_ptr->mu_C = 0.0;
        fluid_cell_info_ptr->mu_S = 0.0;
        fluid_cell_info_ptr->qgp_fraction = 0.0;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                fluid_cell_info_ptr->pi[i][j] = fluidCell_ptr->pi[i][j];
            }
        }
        fluid_cell_info_ptr->bulk_Pi = fluidCell_ptr->bulkPi;
        StoreHydroEvolutionHistory(fluid_cell_info_ptr);
    }
    delete fluidCell_ptr;
}


void MpiMusic::GetHydroInfo(
        Jetscape::real t, Jetscape::real x, Jetscape::real y, Jetscape::real z,
        std::unique_ptr<FluidCellInfo>& fluid_cell_info_ptr) {
    GetHydroInfo_JETSCAPE(t, x, y, z, fluid_cell_info_ptr);
    //GetHydroInfo_MUSIC(t, x, y, z, fluid_cell_info_ptr);
}


void MpiMusic::GetHydroInfo_JETSCAPE(
        Jetscape::real t, Jetscape::real x, Jetscape::real y, Jetscape::real z,
        std::unique_ptr<FluidCellInfo>& fluid_cell_info_ptr) {
    auto temp = bulk_info.get_tz(t, x, y, z);
    fluid_cell_info_ptr = std::unique_ptr<FluidCellInfo>(
                                                    new FluidCellInfo(temp));
}

void MpiMusic::GetHydroInfo_MUSIC(
        Jetscape::real t, Jetscape::real x, Jetscape::real y, Jetscape::real z,
        std::unique_ptr<FluidCellInfo>& fluid_cell_info_ptr) {
    fluid_cell_info_ptr = Jetscape::make_unique<FluidCellInfo>();
    fluidCell *fluidCell_ptr = new fluidCell;
    music_hydro_ptr->get_hydro_info(x, y, z, t, fluidCell_ptr);
    fluid_cell_info_ptr->energy_density = fluidCell_ptr->ed;
    fluid_cell_info_ptr->entropy_density = fluidCell_ptr->sd;
    fluid_cell_info_ptr->temperature = fluidCell_ptr->temperature;
    fluid_cell_info_ptr->pressure = fluidCell_ptr->pressure;
    fluid_cell_info_ptr->vx = fluidCell_ptr->vx;
    fluid_cell_info_ptr->vy = fluidCell_ptr->vy;
    fluid_cell_info_ptr->vz = fluidCell_ptr->vz;
    fluid_cell_info_ptr->mu_B = 0.0;
    fluid_cell_info_ptr->mu_C = 0.0;
    fluid_cell_info_ptr->mu_S = 0.0;
    fluid_cell_info_ptr->qgp_fraction = 0.0;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            fluid_cell_info_ptr->pi[i][j] = fluidCell_ptr->pi[i][j];
        }
    }
    fluid_cell_info_ptr->bulk_Pi = fluidCell_ptr->bulkPi;
    delete fluidCell_ptr;
}

