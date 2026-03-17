#include "MeshIO.h"

#include <rapidobj/rapidobj.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

namespace SDFilter
{
namespace
{

std::string lowercase(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
	return s;
}

bool is_obj_path(const std::string &filename)
{
	std::filesystem::path p(filename);
	return lowercase(p.extension().string()) == ".obj";
}

bool read_obj_rapidobj(TriMesh &mesh, const std::string &filename)
{
	std::filesystem::path path(filename);
	rapidobj::Result result = rapidobj::ParseFile(path, rapidobj::MaterialLibrary::Ignore());
	if(result.error)
	{
		std::ifstream in(filename.c_str(), std::ios::binary);
		if(!in.is_open())
		{
			std::cerr << "rapidOBJ parse error: " << result.error.code.message() << std::endl;
			return false;
		}

		std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		bool had_null = false;
		for(char &c : data)
		{
			if(c == '\0')
			{
				c = ' ';
				had_null = true;
			}
		}

		if(!had_null)
		{
			std::cerr << "rapidOBJ parse error: " << result.error.code.message() << std::endl;
			return false;
		}

		std::istringstream obj_stream(data);
		result = rapidobj::ParseStream(obj_stream, rapidobj::MaterialLibrary::Ignore());
		if(result.error)
		{
			std::cerr << "rapidOBJ parse error after sanitizing NUL bytes: " << result.error.code.message() << std::endl;
			return false;
		}
	}

	std::size_t non_tri_input_faces = 0;
	for(const auto &shape : result.shapes)
	{
		for(const auto n_vertices : shape.mesh.num_face_vertices){
			if(n_vertices != 3){
				++non_tri_input_faces;
			}
		}
	}

	if(!rapidobj::Triangulate(result))
	{
		std::cerr << "rapidOBJ triangulation failed for " << filename << std::endl;
		return false;
	}

	const auto &positions = result.attributes.positions;
	if(positions.size() % 3 != 0)
	{
		std::cerr << "rapidOBJ invalid position data in " << filename << std::endl;
		return false;
	}

	mesh.clear();

	const std::size_t vertex_count = positions.size() / 3;
	if(vertex_count == 0)
	{
		std::cerr << "rapidOBJ loaded zero vertices from " << filename << std::endl;
		return false;
	}
	if(vertex_count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
	{
		std::cerr << "rapidOBJ vertex count exceeds supported range for OpenMesh int handles: "
		          << vertex_count << std::endl;
		return false;
	}
	for(std::size_t i = 0; i < vertex_count; ++i)
	{
		const double x = static_cast<double>(positions[3 * i]);
		const double y = static_cast<double>(positions[3 * i + 1]);
		const double z = static_cast<double>(positions[3 * i + 2]);
		if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
		{
			std::cerr << "rapidOBJ invalid non-finite vertex at index " << i << " in " << filename << std::endl;
			return false;
		}
		mesh.add_vertex(TriMesh::Point(
			x, y, z));
	}

	std::size_t failed_faces = 0;
	std::size_t malformed_face_streams = 0;
	for(const auto &shape : result.shapes)
	{
		const auto &shape_mesh = shape.mesh;
		std::size_t index_offset = 0;

		for(const auto n_vertices : shape_mesh.num_face_vertices)
		{
			if(index_offset + static_cast<std::size_t>(n_vertices) > shape_mesh.indices.size())
			{
				++malformed_face_streams;
				break;
			}

			if(n_vertices != 3)
			{
				index_offset += n_vertices;
				++failed_faces;
				continue;
			}

			std::vector<TriMesh::VertexHandle> face(3);
			bool valid_face = true;
			for(std::size_t k = 0; k < 3; ++k)
			{
				const rapidobj::Index idx = shape_mesh.indices[index_offset + k];
				if(idx.position_index < 0)
				{
					valid_face = false;
					break;
				}
				const std::size_t pos_idx = static_cast<std::size_t>(idx.position_index);
				if(pos_idx >= vertex_count)
				{
					valid_face = false;
					break;
				}
				face[k] = TriMesh::VertexHandle(static_cast<int>(pos_idx));
			}
			index_offset += 3;

			if(!valid_face)
			{
				++failed_faces;
				continue;
			}

			if(!mesh.add_face(face).is_valid())
			{
				++failed_faces;
			}
		}
	}

	if(mesh.n_faces() == 0)
	{
		std::cerr << "rapidOBJ loaded zero valid faces from " << filename << std::endl;
		return false;
	}
	if(malformed_face_streams > 0)
	{
		std::cerr << "rapidOBJ warning: detected " << malformed_face_streams
		          << " malformed face-index streams in " << filename << std::endl;
	}
	if(non_tri_input_faces > 0)
	{
		std::cerr << "rapidOBJ info: triangulated " << non_tri_input_faces
		          << " non-triangle input faces in " << filename << std::endl;
	}

	if(failed_faces > 0)
	{
		std::cerr << "rapidOBJ warning: skipped " << failed_faces << " invalid faces while loading " << filename << std::endl;
	}

	return true;
}

bool write_obj_ascii(const TriMesh &mesh, const std::string &filename, int precision)
{
	std::ofstream out(filename.c_str());
	if(!out.is_open())
	{
		std::cerr << "Unable to open output OBJ file " << filename << std::endl;
		return false;
	}

	out.setf(std::ios::fixed, std::ios::floatfield);
	out.precision(std::max(1, precision));

	for(TriMesh::ConstVertexIter cv_it = mesh.vertices_begin(); cv_it != mesh.vertices_end(); ++cv_it)
	{
		const TriMesh::Point p = mesh.point(*cv_it);
		if(!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]))
		{
			std::cerr << "OBJ export failed: encountered non-finite vertex at index " << cv_it->idx() << std::endl;
			return false;
		}
		out << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
	}

	for(TriMesh::ConstFaceIter cf_it = mesh.faces_begin(); cf_it != mesh.faces_end(); ++cf_it)
	{
		out << "f";
		for(TriMesh::ConstFaceVertexIter cfv_it = mesh.cfv_iter(*cf_it); cfv_it.is_valid(); ++cfv_it)
		{
			out << " " << (cfv_it->idx() + 1);
		}
		out << "\n";
	}

	if(!out.good())
	{
		std::cerr << "Failed while writing OBJ file " << filename << std::endl;
		return false;
	}

	return true;
}

}

bool read_mesh(TriMesh &mesh, const std::string &filename)
{
	if(!is_obj_path(filename))
	{
		std::cerr << "Unsupported input format for rapidOBJ: " << filename << std::endl;
		return false;
	}

	return read_obj_rapidobj(mesh, filename);
}

bool write_mesh(const TriMesh &mesh, const std::string &filename, int obj_precision)
{
	if(!is_obj_path(filename))
	{
		std::cerr << "Unsupported output format for rapidOBJ: " << filename << std::endl;
		return false;
	}

	return write_obj_ascii(mesh, filename, obj_precision);
}

}
