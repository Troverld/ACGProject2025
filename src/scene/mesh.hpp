#pragma once

#include "object.hpp"
#include "triangle.hpp"
#include "../accel/BVH.hpp"
#include "tiny_obj_loader.h"
#include <iostream>
#include <vector>
#include <string>
#include <glm/gtc/matrix_transform.hpp> 

/**
 * @brief Represents a triangle mesh loaded from a file (e.g., .obj).
 * Manages its own internal BVH for efficient intersection within the mesh.
 */
class Mesh : public Object {
public:
    /**
     * @brief Loads a mesh from an OBJ file and applies transformations.
     * 
     * @param filename Path to the .obj file.
     * @param mat The material to apply to the entire mesh.
     * @param translate Position offset.
     * @param scale Uniform scale factor.
     * @param rotate_axis Axis to rotate around.
     * @param rotate_degrees Rotation angle in degrees.
     */
    Mesh(const std::string& filename, std::shared_ptr<Material> mat,
         glm::vec3 translate = glm::vec3(0.0f), 
         float scale = 1.0f, 
         glm::vec3 rotate_axis = glm::vec3(0,1,0), 
         float rotate_degrees = 0.0f) 
    {
        load_obj(filename, mat, translate, scale, rotate_axis, rotate_degrees);
    }

    /**
     * @brief Intersects the mesh by delegating to its internal BVH.
     */
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        if (!bvh_root) return false;
        return bvh_root->intersect(r, t_min, t_max, rec);
    }

    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        if (!bvh_root) return false;
        return bvh_root->bounding_box(time0, time1, output_box);
    }

    // Meshes are usually complex, sampling them as a single area light is inefficient/complex.
    // So we don't implement pdf_value/random_pointing_vector here directly.
    // If a Mesh needs to emit light, the Scene logic promotes its individual Triangles to lights,
    // or we need a MeshAreaLight wrapper. Currently defaults to 0.
    
    virtual Material* get_material() const override { return nullptr; } // Material is managed by triangles

private:
    std::shared_ptr<BVHNode> bvh_root;
    std::vector<std::shared_ptr<Object>> triangles;

    void load_obj(const std::string& filename, std::shared_ptr<Material> mat,
                  glm::vec3 translation, float scale, glm::vec3 rot_axis, float rot_deg) 
    {
        tinyobj::ObjReaderConfig reader_config;
        reader_config.mtl_search_path = "./"; // Path to material files

        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(filename, reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
            return;
        }

        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        // We ignore materials from the OBJ file for now and use the provided one.
        // auto& materials = reader.GetMaterials(); 

        // Precompute Rotation Matrix
        float rad = glm::radians(rot_deg);
        glm::mat4 trans_mat = glm::mat4(1.0f);
        trans_mat = glm::translate(trans_mat, translation);
        trans_mat = glm::rotate(trans_mat, rad, rot_axis);
        trans_mat = glm::scale(trans_mat, glm::vec3(scale));

        // Helper to transform points
        auto transform_point = [&](float x, float y, float z) {
            glm::vec4 p(x, y, z, 1.0f);
            p = trans_mat * p;
            return glm::vec3(p);
        };

        // Helper to transform normals (using Inverse Transpose for non-uniform scale, 
        // but for uniform scale, rotation matrix is sufficient).
        glm::mat3 normal_mat = glm::mat3(glm::transpose(glm::inverse(trans_mat)));
        auto transform_normal = [&](float x, float y, float z) {
            return glm::normalize(normal_mat * glm::vec3(x, y, z));
        };

        std::cout << "[Mesh] Loading " << filename << " (" << shapes.size() << " shapes)..." << std::endl;

        for (size_t s = 0; s < shapes.size(); s++) {
            size_t index_offset = 0;
            
            // Loop over faces (polygons)
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
                
                // We assume triangulated mesh (tinyobjloader can triangulate automatically if requested, 
                // usually standard OBJs are triangles).
                if (fv != 3) {
                    index_offset += fv;
                    continue; // Skip non-triangles for simplicity
                }

                glm::vec3 v[3];
                glm::vec3 n[3];
                glm::vec2 uv[3];
                bool has_normals = true;

                for (size_t v_idx = 0; v_idx < 3; v_idx++) {
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v_idx];

                    // Vertex Position
                    v[v_idx] = transform_point(
                        attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                        attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                        attrib.vertices[3 * size_t(idx.vertex_index) + 2]
                    );

                    // Vertex Normal
                    if (idx.normal_index >= 0) {
                        n[v_idx] = transform_normal(
                            attrib.normals[3 * size_t(idx.normal_index) + 0],
                            attrib.normals[3 * size_t(idx.normal_index) + 1],
                            attrib.normals[3 * size_t(idx.normal_index) + 2]
                        );
                    } else {
                        has_normals = false;
                    }

                    // Texture Coordinates
                    if (idx.texcoord_index >= 0) {
                        uv[v_idx] = glm::vec2(
                            attrib.texcoords[2 * size_t(idx.texcoord_index) + 0],
                            attrib.texcoords[2 * size_t(idx.texcoord_index) + 1]
                        );
                    } else {
                        uv[v_idx] = glm::vec2(0.0f);
                    }
                }

                if (has_normals) {
                    triangles.push_back(std::make_shared<Triangle>(
                        v[0], v[1], v[2], n[0], n[1], n[2], mat, uv[0], uv[1], uv[2]
                    ));
                } else {
                    triangles.push_back(std::make_shared<Triangle>(
                        v[0], v[1], v[2], mat, uv[0], uv[1], uv[2]
                    ));
                }

                index_offset += fv;
            }
        }

        if (!triangles.empty()) {
            std::cout << "[Mesh] Building Internal BVH for " << triangles.size() << " triangles..." << std::endl;
            bvh_root = std::make_shared<BVHNode>(triangles, 0.0f, 1.0f);
        }
    }
};