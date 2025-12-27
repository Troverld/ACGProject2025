#pragma once

#include "triangle.hpp"
#include "../accel/BVH.hpp"
#include "tiny_obj_loader.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp> 
#include "../material/diffuse.hpp"
#include "../texture/image_texture.hpp"
#include "../texture/solid_color.hpp"

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
     * @param mat The material to apply to the entire mesh. If nullptr, materials will be loaded from the .mtl file.
     * @param translate Position offset.
     * @param scale Uniform scale factor.
     * @param rotate_axis Axis to rotate around.
     * @param rotate_degrees Rotation angle in degrees.
     */
    Mesh(const std::string& filename, std::shared_ptr<Material> mat = nullptr,
         glm::vec3 translate = glm::vec3(0.0f), 
         float scale = 1.0f, 
         glm::vec3 rotate_axis = glm::vec3(0,1,0), 
         float rotate_degrees = 0.0f) 
    {
        mat_ptr = mat;
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

    /**
     * @brief Sample a point uniformly on the surface of the mesh.
     * Uses an area-weighted distribution to select a triangle, then samples the triangle.
     */
    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& area) const override {
        if (!triangle_distribution) {
            area = 0.0f;
            return;
        }

        // 1. Select a triangle based on area
        float pdf_choice;
        float u_remapped;
        int idx = triangle_distribution->sample_discrete(random_float(), pdf_choice, u_remapped);

        // 2. Sample the selected triangle
        // Note: The object in 'triangles' is technically shared_ptr<Object>, cast to Triangle if needed
        // but virtual function call works since Triangle implements it.
        triangles[idx]->sample_surface(pos, normal, area);
        area = sum_area;
    }

    virtual glm::vec3 random_pointing_vector(const glm::vec3& origin) const override{return glm::vec3(0.0f);}
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& wi) const override {return 0.0f;}
    
    virtual Material* get_material() const override { return mat_ptr.get(); } // Material is managed by triangles

    /**
     * @brief Propagate light ID to all contained triangles.
     * This ensures that when a ray hits a specific triangle, rec.object->get_light_id() returns the correct value.
     */
    virtual void set_light_id(int id) override {
        Object::set_light_id(id); // Set ID for the Mesh itself
        for (auto& tri : triangles) tri->set_light_id(id);
    }

private:
    std::shared_ptr<BVHNode> bvh_root;
    std::shared_ptr<Material> mat_ptr;
    std::vector<std::shared_ptr<Object>> triangles;
    std::unique_ptr<Distribution1D> triangle_distribution;
    std::vector<std::shared_ptr<Material>> obj_materials; // Added: Store materials loaded from OBJ/MTL
    float sum_area = 0.0f;

    
    /**
     * @brief Parses the OBJ file, loads materials (if not overridden), and builds geometry.
     */
    void load_obj(const std::string& filename, std::shared_ptr<Material> global_mat,
                  glm::vec3 translation, float scale, glm::vec3 rot_axis, float rot_deg) 
    {
        // 1. Setup TinyObjReader
        tinyobj::ObjReaderConfig reader_config;
        // Search for .mtl in the same directory as the .obj
        auto last_slash = filename.find_last_of("/\\");
        std::string base_dir = (last_slash != std::string::npos) ? filename.substr(0, last_slash + 1) : "./";
        reader_config.mtl_search_path = base_dir; 

        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(filename, reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader Error: " << reader.Error();
            }
            return;
        }

        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader Warning: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        auto& materials = reader.GetMaterials(); 

        // 2. Load Materials from MTL (Only if no global override is provided)
        // We use the default gray fallback if a face has no material assigned.
        auto fallback_mat = std::make_shared<Lambertian>(glm::vec3(0.5f));

        if (!global_mat) {
            std::cout << "[Mesh] Loading materials from MTL..." << std::endl;
            for (const auto& m : materials) {
                // A. Diffuse / Albedo
                std::shared_ptr<Texture> albedo_tex;
                if (!m.diffuse_texname.empty()) {
                    std::string tex_path = base_dir + m.diffuse_texname;
                    albedo_tex = std::make_shared<ImageTexture>(tex_path.c_str());
                } else {
                    albedo_tex = std::make_shared<SolidColor>(glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]));
                }

                // B. Normal Map
                // Blender typically exports normal maps to 'norm' or 'map_Kn'. 
                // TinyObjLoader might parse generic bump maps into bump_texname.
                std::shared_ptr<Texture> normal_tex = nullptr;
                std::string normal_path = "";
                
                if (!m.normal_texname.empty()) {
                    normal_path = m.normal_texname;
                } else if (!m.bump_texname.empty()) {
                    normal_path = m.bump_texname; // Fallback if encoded as bump
                }

                if (!normal_path.empty()) {
                    std::string full_path = base_dir + normal_path;
                    normal_tex = std::make_shared<ImageTexture>(full_path.c_str());
                }

                // C. Construct Lambertian Material
                // Note: Roughness maps are ignored as requested, treating everything as Diffuse.
                obj_materials.push_back(std::make_shared<Lambertian>(albedo_tex, normal_tex));
            }
        }

        // 3. Precompute Transformation Matrix
        float rad = glm::radians(rot_deg);
        glm::mat4 trans_mat = glm::mat4(1.0f);
        trans_mat = glm::translate(trans_mat, translation);
        trans_mat = glm::rotate(trans_mat, rad, rot_axis);
        trans_mat = glm::scale(trans_mat, glm::vec3(scale));

        // Helper: Transform Point
        auto transform_point = [&](float x, float y, float z) {
            glm::vec4 p(x, y, z, 1.0f);
            p = trans_mat * p;
            return glm::vec3(p);
        };

        // Helper: Transform Normal (Inverse Transpose)
        glm::mat3 normal_mat = glm::mat3(glm::transpose(glm::inverse(trans_mat)));
        auto transform_normal = [&](float x, float y, float z) {
            return glm::normalize(normal_mat * glm::vec3(x, y, z));
        };

        std::cout << "[Mesh] Processing geometry for " << filename << " (" << shapes.size() << " shapes)..." << std::endl;
        std::vector<float> triangle_areas;

        // 4. Iterate Shapes and Faces
        for (size_t s = 0; s < shapes.size(); s++) {
            size_t index_offset = 0;
            
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
                
                // Only process triangles
                if (fv != 3) {
                    index_offset += fv;
                    continue; 
                }

                // Determine Material
                std::shared_ptr<Material> face_mat;
                if (global_mat) {
                    face_mat = global_mat;
                } else {
                    int mat_id = shapes[s].mesh.material_ids[f];
                    if (mat_id >= 0 && mat_id < static_cast<int>(obj_materials.size())) {
                        face_mat = obj_materials[mat_id];
                    } else {
                        face_mat = fallback_mat;
                    }
                }

                glm::vec3 v[3];
                glm::vec3 n[3];
                glm::vec2 uv[3];
                bool has_normals = true;

                // Extract Vertex Data
                for (size_t v_idx = 0; v_idx < 3; v_idx++) {
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v_idx];

                    // Position
                    v[v_idx] = transform_point(
                        attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                        attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                        attrib.vertices[3 * size_t(idx.vertex_index) + 2]
                    );

                    // Normal
                    if (idx.normal_index >= 0) {
                        n[v_idx] = transform_normal(
                            attrib.normals[3 * size_t(idx.normal_index) + 0],
                            attrib.normals[3 * size_t(idx.normal_index) + 1],
                            attrib.normals[3 * size_t(idx.normal_index) + 2]
                        );
                    } else {
                        has_normals = false;
                    }

                    // UV
                    if (idx.texcoord_index >= 0) {
                        uv[v_idx] = glm::vec2(
                            attrib.texcoords[2 * size_t(idx.texcoord_index) + 0],
                            attrib.texcoords[2 * size_t(idx.texcoord_index) + 1]
                        );
                    } else {
                        uv[v_idx] = glm::vec2(0.0f);
                    }
                }

                // Create Triangle
                if (has_normals) {
                    auto tri = std::make_shared<Triangle>(
                        v[0], v[1], v[2], n[0], n[1], n[2], face_mat, uv[0], uv[1], uv[2]
                    );
                    triangles.push_back(tri);
                    triangle_areas.push_back(tri->area);
                } else {
                    auto tri = std::make_shared<Triangle>(
                        v[0], v[1], v[2], face_mat, uv[0], uv[1], uv[2]
                    );
                    triangles.push_back(tri);
                    triangle_areas.push_back(tri->area);
                }

                index_offset += fv;
            }
        }

        // 5. Finalize Geometry
        sum_area = std::accumulate(triangle_areas.begin(), triangle_areas.end(), 0.0f);

        if (!triangles.empty()) {
            std::cout << "[Mesh] Building BVH for " << triangles.size() << " triangles..." << std::endl;
            bvh_root = std::make_shared<BVHNode>(triangles, 0.0f, 1.0f);
            triangle_distribution = std::make_unique<Distribution1D>(triangle_areas.data(), triangle_areas.size());
        }
    }
};