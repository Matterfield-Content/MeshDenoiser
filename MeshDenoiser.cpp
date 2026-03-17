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
}

int main(int argc, char **argv)
{
	if(argc < 4)
	{
		std::cout << "Usage:	MeshDenoiser  OPTION_FILE  INPUT_MESH  OUTPUT_MESH "
			<< "[--obj-export-precision N] [--metrics-json PATH] [--metrics-csv PATH] [--deterministic]" << std::endl;
		return 1;
	}

	int obj_export_precision = 16;
	std::string metrics_json_path;
	std::string metrics_csv_path;
	bool deterministic_cli = false;

	for(int i = 4; i < argc; ++i)
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
    if(!SDFilter::read_mesh(mesh, argv[2]))
    {
    	std::cerr << "Error: unable to read input mesh from the file " << argv[2] << std::endl;
    	return 1;
    }
	auto import_end = Clock::now();

	#ifdef USE_OPENMP
    Eigen::initParallel();
	#endif

    // Load option file
    SDFilter::MeshDenoisingParameters param;
    if(!param.load(argv[1])){
    	std::cerr << "Error: unable to load option file " << argv[1] << std::endl;
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
	if(!SDFilter::write_mesh(output_mesh, argv[3], obj_export_precision)){
		std::cerr << "Error: unable to save the result mesh to file " << argv[3] << std::endl;
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
