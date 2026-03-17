// BSD 3-Clause License
//
// Copyright (c) 2017, Bailin Deng
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "MeshTypes.h"
#include "MeshNormalDenoising.h"
#include "MeshIO.h"
#include "AppMetrics.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <filesystem>
#include <string>
#include <Eigen/Core>
#ifdef USE_OPENMP
#include <omp.h>
#endif

namespace
{
void configure_deterministic_runtime(bool deterministic)
{
	if(!deterministic){
		return;
	}
#ifdef USE_OPENMP
	omp_set_dynamic(0);
	omp_set_num_threads(1);
#endif
	Eigen::setNbThreads(1);
}

const char* default_options_text()
{
	return
R"(#### Option file for SD filter based mesh denoising
#### Lines starting with '#' are comments

## Regularization weight, must be positive.
Lambda  0.25

## Gaussian standard deviation for spatial weight, scaled by the average distance between adjacent face cetroids. Must be positive.
Eta 2.8

## Gaussian standard deviation for guidance weight, must be positive.
Mu  0.25

## Gaussian standard deviation for signal weight, must be positive
Nu  0.3

## Closeness weight for mesh update, must be positive
MeshUpdateClosenessWeight  0.001

## Iterations for mesh update, must be positive
MeshUpdateIterations  5

## Early-stop threshold for per-iteration mesh update RMS displacement (<=0 disables)
MeshUpdateDisplacementEps 1e-1

## Outer iteration for denoising, must be positive integers
OuterIterations   2

## Force deterministic mode (single-threaded runtime) for reproducible outputs (0/1)
DeterministicMode 0

## Linear solver backend: 0=CG, 1=Eigen LDLT, 2=CHOLMOD (falls back to 1 if unavailable)
LinearSolverType 2
)";
}

bool write_default_options_file(const std::string &path)
{
	std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
	if(!out.is_open()){
		return false;
	}

	out << default_options_text();
	return out.good();
}

void apply_default_options(SDFilter::MeshDenoisingParameters &param)
{
	param.lambda = 0.25;
	param.eta = 2.8;
	param.mu = 0.25;
	param.nu = 0.3;
	param.mesh_update_closeness_weight = 0.001;
	param.mesh_update_iter = 5;
	param.mesh_update_disp_eps = 1e-1;
	param.outer_iterations = 2;
	param.deterministic_mode = false;
	param.linear_solver_type = static_cast<SDFilter::Parameters::LinearSolverType>(SDFilter::DEFAULT_LINEAR_SOLVER_TYPE);
}

void print_help(const char *exe_name)
{
	std::cout << "MeshDenoiser.exe" << std::endl;
	std::cout << std::endl;
	std::cout << "Usage:" << std::endl;
	std::cout << "  " << exe_name << " INPUT_MESH OUTPUT_MESH [optional flags]" << std::endl;
	std::cout << "  " << exe_name << " OPTION_FILE INPUT_MESH OUTPUT_MESH [optional flags]" << std::endl;
	std::cout << "  " << exe_name << " --write-default-options PATH" << std::endl;
	std::cout << "  " << exe_name << " --help" << std::endl;
	std::cout << std::endl;
	std::cout << "Positional arguments:" << std::endl;
	std::cout << "  OPTION_FILE               Optional. Path to the denoising options text file." << std::endl;
	std::cout << "                            If omitted, built-in defaults are used." << std::endl;
	std::cout << "  INPUT_MESH                Required. Input ASCII OBJ mesh to denoise." << std::endl;
	std::cout << "  OUTPUT_MESH               Required. Output ASCII OBJ mesh path." << std::endl;
	std::cout << std::endl;
	std::cout << "Optional flags:" << std::endl;
	std::cout << "  --obj-export-precision N  Optional. OBJ vertex float precision. Default: 16." << std::endl;
	std::cout << "  --metrics-json PATH       Optional. Write JSON timing and solver metrics." << std::endl;
	std::cout << "  --metrics-csv PATH        Optional. Append CSV timing and solver metrics." << std::endl;
	std::cout << "  --deterministic           Optional. Force deterministic single-threaded runtime." << std::endl;
	std::cout << "  --write-default-options PATH" << std::endl;
	std::cout << "                            Optional standalone command. Writes the default" << std::endl;
	std::cout << "                            options template to PATH and exits." << std::endl;
	std::cout << "  --help                    Optional. Show this help text and exit." << std::endl;
}
}

int main(int argc, char **argv)
{
	if(argc == 1)
	{
		print_help(argv[0]);
		return 0;
	}

	if(argc == 2)
	{
		const std::string arg = argv[1];
		if(arg == "--help" || arg == "-h"){
			print_help(argv[0]);
			return 0;
		}
	}

	if(argc == 3)
	{
		const std::string arg = argv[1];
		if(arg == "--write-default-options")
		{
			if(!write_default_options_file(argv[2])){
				std::cerr << "Error: unable to write default options file " << argv[2] << std::endl;
				return 1;
			}

			std::cout << "Wrote default options to " << argv[2] << std::endl;
			return 0;
		}
	}

	if(argc < 4)
	{
		print_help(argv[0]);
		return 1;
	}

	int positional_argc = 0;
	for(int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if(arg.rfind("--", 0) == 0){
			break;
		}
		++positional_argc;
	}

	if(positional_argc != 2 && positional_argc != 3)
	{
		print_help(argv[0]);
		return 1;
	}

	std::string options_path;
	std::string input_mesh_path;
	std::string output_mesh_path;
	int flag_index = 1;
	bool using_builtin_defaults = false;

	if(positional_argc == 2)
	{
		using_builtin_defaults = true;
		input_mesh_path = argv[1];
		output_mesh_path = argv[2];
		flag_index = 3;
	}
	else
	{
		options_path = argv[1];
		input_mesh_path = argv[2];
		output_mesh_path = argv[3];
		flag_index = 4;
	}

	int obj_export_precision = 16;
	std::string metrics_json_path;
	std::string metrics_csv_path;
	bool deterministic_cli = false;

	for(int i = flag_index; i < argc; ++i)
	{
		std::string arg = argv[i];
		if(arg == "--obj-export-precision" && i + 1 < argc){
			obj_export_precision = std::stoi(argv[++i]);
		}
		else if(arg == "--metrics-json" && i + 1 < argc){
			metrics_json_path = argv[++i];
		}
		else if(arg == "--metrics-csv" && i + 1 < argc){
			metrics_csv_path = argv[++i];
		}
		else if(arg == "--deterministic"){
			deterministic_cli = true;
		}
		else{
			std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
			return 1;
		}
	}

	using Clock = std::chrono::steady_clock;
	auto app_begin = Clock::now();

	TriMesh mesh;
	auto import_begin = Clock::now();
    if(!SDFilter::read_mesh(mesh, input_mesh_path.c_str()))
    {
    	std::cerr << "Error: unable to read input mesh from the file " << input_mesh_path << std::endl;
    	return 1;
    }
	auto import_end = Clock::now();

	#ifdef USE_OPENMP
    Eigen::initParallel();
	#endif

    // Load option file
    SDFilter::MeshDenoisingParameters param;
	if(using_builtin_defaults)
	{
		apply_default_options(param);
		std::cout << "No option file specified; using built-in default denoising options." << std::endl;
	}
	else if(!param.load(options_path.c_str())){
		std::cerr << "Error: unable to load option file " << options_path << std::endl;
		return 1;
	}
    if(!param.valid_parameters()){
    	std::cerr << "Invalid filter options. Aborting..." << std::endl;
    	return 1;
    }
	if(deterministic_cli){
		param.deterministic_mode = true;
	}
	configure_deterministic_runtime(param.deterministic_mode);
    param.output();


    // Normalize the input mesh
    Eigen::Vector3d original_center;
    double original_scale;
    auto normalize_begin = Clock::now();
    SDFilter::normalize_mesh(mesh, original_center, original_scale);
    auto normalize_end = Clock::now();

    // Filter the normals and construct the output mesh
    SDFilter::MeshNormalDenoising denoiser(mesh);
    TriMesh output_mesh;
    auto denoise_begin = Clock::now();
    if(!denoiser.denoise(param, output_mesh)){
    	std::cerr << "Error: denoising failed." << std::endl;
    	return 1;
    }
    auto denoise_end = Clock::now();

    auto restore_begin = Clock::now();
    SDFilter::restore_mesh(output_mesh, original_center, original_scale);
    auto restore_end = Clock::now();

    // Save output mesh
	auto export_begin = Clock::now();
	if(!SDFilter::write_mesh(output_mesh, output_mesh_path.c_str(), obj_export_precision)){
		std::cerr << "Error: unable to save the result mesh to file " << output_mesh_path << std::endl;
		return 1;
	}
	auto export_end = Clock::now();
	auto app_end = Clock::now();

	SDFilter::PipelineMetrics metrics;
	metrics.mode = "denoise";
	metrics.input_mesh = argv[2];
	metrics.output_mesh = argv[3];
	metrics.import_secs = std::chrono::duration<double>(import_end - import_begin).count();
	metrics.normalize_secs = std::chrono::duration<double>(normalize_end - normalize_begin).count();
	metrics.algorithm_secs = std::chrono::duration<double>(denoise_end - denoise_begin).count();
	metrics.restore_secs = std::chrono::duration<double>(restore_end - restore_begin).count();
	metrics.export_secs = std::chrono::duration<double>(export_end - export_begin).count();
	metrics.total_secs = std::chrono::duration<double>(app_end - app_begin).count();
	metrics.obj_export_precision = obj_export_precision;

	const SDFilter::RunStatistics &stats = denoiser.run_stats();
	if(!metrics_json_path.empty()){
		if(!SDFilter::write_metrics_json(metrics_json_path, metrics, stats)){
			std::cerr << "Warning: unable to write metrics JSON file " << metrics_json_path << std::endl;
		}
	}

	if(!metrics_csv_path.empty()){
		bool write_header = !std::filesystem::exists(metrics_csv_path);
		if(!SDFilter::write_metrics_csv(metrics_csv_path, metrics, stats, write_header)){
			std::cerr << "Warning: unable to write metrics CSV file " << metrics_csv_path << std::endl;
		}
	}

	return 0;
}
