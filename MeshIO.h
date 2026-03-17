#ifndef MESHIO_H
#define MESHIO_H

#include "MeshTypes.h"

#include <string>

namespace SDFilter
{

// Read mesh from ASCII OBJ via rapidOBJ.
bool read_mesh(TriMesh &mesh, const std::string &filename);

// Write mesh as ASCII OBJ.
bool write_mesh(const TriMesh &mesh, const std::string &filename, int obj_precision = 16);

}

#endif // MESHIO_H
