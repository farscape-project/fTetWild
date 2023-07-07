/* This file is part of PyMesh. Copyright (c) 2015 by Qingnan Zhou */
#include "MshSaver.h"

#include <cassert>
#include <iostream>
#include <sstream>

#include "Exception.h"

namespace floatTetWild {
using namespace PyMesh;

MshSaver::MshSaver(const std::string& filename, bool binary) :
        m_binary(binary), m_num_nodes(0), m_num_elements(0), m_dim(0) {
    if (!m_binary) {
        fout.open(filename.c_str(), std::fstream::out);
    } else {
        fout.open(filename.c_str(), std::fstream::binary);
    }
    if (!fout) {
        std::stringstream err_msg;
        err_msg << "Error opening " << filename << " to write msh file." << std::endl;
        throw ::PyMesh::IOError(err_msg.str());
    }
}

MshSaver::~MshSaver() {
    fout.close();
}

void MshSaver::save_mesh(size_t dim, const VectorF& nodes,
                         const VectorI& elements,    const VectorI& components,    ElementType type,
                         const VectorI& sf_elements, const VectorI& sf_components, ElementType sf_type) {
    if (dim != 2 && dim != 3) {
        std::stringstream err_msg;
        err_msg << dim << "D mesh is not supported!" << std::endl;
        throw ::PyMesh::NotImplementedError(err_msg.str());
    }
    m_dim = dim;

    save_header();
    save_nodes(nodes);
    save_elements(elements,    components,    type,
                  sf_elements, sf_components, sf_type);
}

void MshSaver::save_header() {
    if (!m_binary) {
        fout << "$MeshFormat" << std::endl;
        fout << "2.2 0 " << sizeof(Float) << std::endl;
        fout << "$EndMeshFormat" << std::endl;
    } else {
        fout << "$MeshFormat" << std::endl;
        fout << "2.2 1 " << sizeof(Float) << std::endl;
        int one = 1;
        fout.write((char*)&one, sizeof(int));
        fout << "$EndMeshFormat" << std::endl;
    }
    fout.flush();
}

void MshSaver::save_nodes(const VectorF& nodes) {
    // Save nodes.
    m_num_nodes = nodes.size() / m_dim;
    fout << "$Nodes" << std::endl;
    fout << m_num_nodes << std::endl;
    if (!m_binary) {
        for (size_t i=0; i<nodes.size(); i+=m_dim) {
            const VectorF& v = nodes.segment(i,m_dim);
            int node_idx = i/m_dim+1;
            fout << node_idx << " " << v[0] << " " << v[1] << " ";
            if (m_dim == 2) {
                fout << 0.0 << std::endl;
            } else {
                fout << v[2] << std::endl;
            }
        }
    } else {
        for (size_t i=0; i<nodes.size(); i+=m_dim) {
            const VectorF& v = nodes.segment(i,m_dim);
            int node_idx = i/m_dim+1;
            fout.write((char*)&node_idx, sizeof(int));
            fout.write((char*)v.data(), sizeof(Float)*m_dim);

            // for 2D shapes, z coordinate is always 0.
            if (m_dim == 2) {
                const Float zero = 0.0;
                fout.write((char*)&zero, sizeof(Float));
            }
        }
    }
    fout << "$EndNodes" << std::endl;
    fout.flush();
}

void MshSaver::save_elements(
        const VectorI& elements,    const VectorI& components,    ElementType type,
        const VectorI& sf_elements, const VectorI& sf_components, ElementType sf_type) {

    auto no_nodes_in_element = [](ElementType type) {
        switch (type) {
            case TRI:
                return 3;
            case QUAD:
                return 4;
            case TET:
                return 4;
            case HEX:
                return 8;
            default:
            {
                std::stringstream err_msg;
                err_msg << "Unsupported element type " << type;
                throw ::PyMesh::NotImplementedError(err_msg.str());
            }
        }
    };

    size_t nodes_per_element = no_nodes_in_element(type);
    size_t nodes_per_sf_element = no_nodes_in_element(sf_type);

    size_t num_elements = elements.size() / nodes_per_element;
    size_t num_sf_elements = sf_elements.size() / nodes_per_sf_element;
    m_num_elements = num_elements + num_sf_elements;

    // Save elements.
    fout << "$Elements" << std::endl;
    fout << m_num_elements << std::endl;

    save_elements(elements,    components,    type,    nodes_per_element,    num_elements,    0);
    save_elements(sf_elements, sf_components, sf_type, nodes_per_sf_element, num_sf_elements, num_elements);

    fout << "$EndElements" << std::endl;
    fout.flush();
}

void MshSaver::save_elements(
        const VectorI& elements, const VectorI& components, ElementType type,
        size_t nodes_per_element, size_t num_elements, size_t offset) {
    int elem_type = type;
    int num_elems = num_elements;
    int tags = components.size() > 0 ? 2 : 0;
    if (!m_binary) {
        for (size_t i=0; i<elements.size(); i+=nodes_per_element) {
            int elem_num = i/nodes_per_element + 1;
            VectorI elem = elements.segment(i, nodes_per_element) +
                            VectorI::Ones(nodes_per_element);
            int offset_elem_num = offset + elem_num;
            fout << offset_elem_num << " " << elem_type << " " << tags << " ";
            if(components.size() > 0)
                fout << components[elem_num-1] << " " << components[elem_num-1] << " ";

            for (size_t j=0; j<nodes_per_element; j++) {
                fout << elem[j] << " ";
            }
            fout << std::endl;
        }
    } else {
        fout.write((char*)&elem_type, sizeof(int));
        fout.write((char*)&num_elems, sizeof(int));
        fout.write((char*)&tags, sizeof(int));
        for (size_t i=0; i<elements.size(); i+=nodes_per_element) {
            int elem_num = i/nodes_per_element + 1;
            VectorI elem = elements.segment(i, nodes_per_element) +
                            VectorI::Ones(nodes_per_element);
            int offset_elem_num = offset + elem_num;
            fout.write((char*)&offset_elem_num, sizeof(int));
            if(components.size() > 0){
                std::array<int, 2> comps = {{components[elem_num-1], components[elem_num-1]}};
                fout.write((char*)comps.data(), sizeof(int) * 2);
            }
            fout.write((char*)elem.data(), sizeof(int)*nodes_per_element);
        }
    }
}

void MshSaver::save_scalar_field(const std::string& fieldname, const VectorF& field) {
    assert(field.size() == m_num_nodes);
    fout << "$NodeData" << std::endl;
    fout << "1" << std::endl; // num string tags.
    fout << "\"" << fieldname << "\"" << std::endl;
    fout << "1" << std::endl; // num real tags.
    fout << "0.0" << std::endl; // time value.
    fout << "3" << std::endl; // num int tags.
    fout << "0" << std::endl; // the time step
    fout << "1" << std::endl; // 1-component scalar field.
    fout << m_num_nodes << std::endl; // number of nodes

    if (m_binary) {
        for (size_t i=0; i<m_num_nodes; i++) {
            int node_idx = i+1;
            fout.write((char*)&node_idx, sizeof(int));
            fout.write((char*)&field[i], sizeof(Float));
        }
    } else {
        for (size_t i=0; i<m_num_nodes; i++) {
            int node_idx = i+1;
            fout << node_idx << " " << field[i] << std::endl;
        }
    }
    fout << "$EndNodeData" << std::endl;
    fout.flush();
}

void MshSaver::save_vector_field(const std::string& fieldname, const VectorF& field) {
    assert(field.size() == m_dim * m_num_nodes);
    fout << "$NodeData" << std::endl;
    fout << "1" << std::endl; // num string tags.
    fout << "\"" << fieldname << "\"" << std::endl;
    fout << "1" << std::endl; // num real tags.
    fout << "0.0" << std::endl; // time value.
    fout << "3" << std::endl; // num int tags.
    fout << "0" << std::endl; // the time step
    fout << "3" << std::endl; // 3-component vector field.
    fout << m_num_nodes << std::endl; // number of nodes

    const Float zero = 0.0;
    if (m_binary) {
        for (size_t i=0; i<m_num_nodes; i++) {
            int node_idx = i+1;
            fout.write((char*)&node_idx, sizeof(int));
            if (m_dim == 3)
                fout.write((char*)&field[i*3], sizeof(Float)*3);
            else if (m_dim == 2) {
                fout.write((char*)&field[i*2], sizeof(Float)*2);
                fout.write((char*)&zero, sizeof(Float)); // Set z=0
            }
        }
    } else {
        for (size_t i=0; i<m_num_nodes; i++) {
            int node_idx = i+1;
            if (m_dim == 3) {
                fout << node_idx
                     << " " << field[i*3]
                     << " " << field[i*3+1]
                     << " " << field[i*3+2]
                     << std::endl;
            } else if (m_dim == 2) {
                fout << node_idx
                     << " " << field[i*2]
                     << " " << field[i*2+1]
                     << " " << zero
                     << std::endl;
            }
        }
    }
    fout << "$EndNodeData" << std::endl;
    fout.flush();
}

void MshSaver::save_elem_scalar_field(const std::string& fieldname, const VectorF& field) {
    assert(field.size() == m_num_elements);
    fout << "$ElementData" << std::endl;
    fout << 1 << std::endl; // num string tags.
    fout << "\"" << fieldname << "\"" << std::endl;
    fout << "1" << std::endl; // num real tags.
    fout << "0.0" << std::endl; // time value.
    fout << "3" << std::endl; // num int tags.
    fout << "0" << std::endl; // the time step
    fout << "1" << std::endl; // 1-component scalar field.
    fout << m_num_elements << std::endl; // number of elements

    if (m_binary) {
        for (size_t i=0; i<m_num_elements; i++) {
            int elem_idx = i+1;
            fout.write((char*)&elem_idx, sizeof(int));
            fout.write((char*)&field[i], sizeof(Float));
        }
    } else {
        for (size_t i=0; i<m_num_elements; i++) {
            int elem_idx = i+1;
            fout << elem_idx << " " << field[i] << std::endl;
        }
    }

    fout << "$EndElementData" << std::endl;
    fout.flush();
}

void MshSaver::save_elem_vector_field(const std::string& fieldname, const VectorF& field) {
    assert(field.size() == m_num_elements * m_dim);
    fout << "$ElementData" << std::endl;
    fout << 1 << std::endl; // num string tags.
    fout << "\"" << fieldname << "\"" << std::endl;
    fout << "1" << std::endl; // num real tags.
    fout << "0.0" << std::endl; // time value.
    fout << "3" << std::endl; // num int tags.
    fout << "0" << std::endl; // the time step
    fout << "3" << std::endl; // 3-component vector field.
    fout << m_num_elements << std::endl; // number of elements

    const Float zero = 0.0;
    if (m_binary) {
        for (size_t i=0; i<m_num_elements; i++) {
            int elem_idx = i+1;
            fout.write((char*)&elem_idx, sizeof(int));
            if (m_dim == 3)
                fout.write((char*)&field[i*3], sizeof(Float) * 3);
            else if (m_dim == 2) {
                fout.write((char*)&field[i*2], sizeof(Float) * 2);
                fout.write((char*)&zero, sizeof(Float)); // set z=0
            }
        }
    } else {
        for (size_t i=0; i<m_num_elements; i++) {
            int elem_idx = i+1;
            if (m_dim == 3) {
                fout << elem_idx
                     << " " << field[i*3]
                     << " " << field[i*3+1]
                     << " " << field[i*3+2]
                     << std::endl;
            } else if (m_dim == 2) {
                fout << elem_idx
                     << " " << field[i*2]
                     << " " << field[i*2+1]
                     << " " << zero
                     << std::endl;
            }
        }
    }

    fout << "$EndElementData" << std::endl;
    fout.flush();
}

void MshSaver::save_elem_tensor_field(const std::string& fieldname, const VectorF& field) {
    assert(field.size() == m_num_elements * m_dim * (m_dim + 1) / 2);
    fout << "$ElementData" << std::endl;
    fout << 1 << std::endl; // num string tags.
    fout << "\"" << fieldname << "\"" << std::endl;
    fout << "1" << std::endl; // num real tags.
    fout << "0.0" << std::endl; // time value.
    fout << "3" << std::endl; // num int tags.
    fout << "0" << std::endl; // the time step
    fout << "9" << std::endl; // 9-component tensor field.
    fout << m_num_elements << std::endl; // number of elements

    const Float zero = 0.0;
    if (m_binary) {
        for (size_t i=0; i<m_num_elements; i++) {
            int elem_idx = i+1;
            fout.write((char*)&elem_idx, sizeof(int));
            if (m_dim == 3) {
                const VectorF& val = field.segment(i*6, 6);
                Float tensor[9] = {
                        val[0], val[5], val[4],
                        val[5], val[1], val[3],
                        val[4], val[3], val[2] };
                fout.write((char*)tensor, sizeof(Float) * 9);
            } else if (m_dim == 2) {
                const VectorF& val = field.segment(i*3, 3);
                Float tensor[9] = {
                        val[0], val[2], zero,
                        val[2], val[1], zero,
                        zero,   zero, zero };
                fout.write((char*)tensor, sizeof(Float)*9);
            }
        }
    } else {
        for (size_t i=0; i<m_num_elements; i++) {
            int elem_idx = i+1;
            if (m_dim == 3) {
                const VectorF& val = field.segment(i*6, 6);
                fout << elem_idx
                     << " " << val[0]
                     << " " << val[5]
                     << " " << val[4]
                     << " " << val[5]
                     << " " << val[1]
                     << " " << val[3]
                     << " " << val[4]
                     << " " << val[3]
                     << " " << val[2]
                     << std::endl;
            } else if (m_dim == 2) {
                const VectorF& val = field.segment(i*3, 3);
                fout << elem_idx
                     << " " << val[0]
                     << " " << val[2]
                     << " " << zero
                     << " " << val[2]
                     << " " << val[1]
                     << " " << zero
                     << " " << zero
                     << " " << zero
                     << " " << zero
                     << std::endl;
            }
        }
    }

    fout << "$EndElementData" << std::endl;
    fout.flush();
}
}