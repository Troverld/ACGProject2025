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

class MovingMesh : public Object {
public:
    MovingMesh(const std::string& filename, 
               glm::vec3 cen0, glm::vec3 cen1, 
               float _time0, float _time1,
               std::shared_ptr<Material> mat = nullptr,
               float scale = 1.0f, 
               glm::vec3 rotate_axis = glm::vec3(0,1,0), 
               float rotate_degrees = 0.0f)
        : center0(cen0), center1(cen1), time0(_time0), time1(_time1), mat_ptr(mat)
    {
        // Load the mesh at the origin (0,0,0) locally
        load_obj(filename, mat, glm::vec3(0.0f), scale, rotate_axis, rotate_degrees);
    }

    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        if (!bvh_root) return false;

        // Move the ray into the local frame of the mesh at time t
        glm::vec3 current_center = center_at(r.time());
        Ray moved_ray(r.origin() - current_center, r.direction(), r.time());

        if (!bvh_root->intersect(moved_ray, t_min, t_max, rec))
            return false;

        // Transform hit point back to world space
        rec.p += current_center;
        
        // Normal does not need transformation as we only translate
        rec.set_face_normal(moved_ray, rec.normal);
        
        return true;
    }

    virtual bool bounding_box(float _t0, float _t1, AABB& output_box) const override {
        if (!bvh_root) return false;

        // Get the static bounding box of the mesh in local space
        AABB local_box;
        if (!bvh_root->bounding_box(0, 0, local_box)) return false;

        glm::vec3 shift0 = center_at(_t0);
        glm::vec3 shift1 = center_at(_t1);

        AABB box0(local_box.min_point() + shift0, local_box.max_point() + shift0);
        AABB box1(local_box.min_point() + shift1, local_box.max_point() + shift1);

        output_box = surrounding_box(box0, box1);
        return true;
    }

    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& area) const override {
        if (!triangle_distribution) {
            area = 0.0f;
            return;
        }

        // 1. Select a triangle based on area
        float pdf_choice;
        float u_remapped;
        int idx = triangle_distribution->sample_discrete(random_float(), pdf_choice, u_remapped);

        // 2. Sample the selected triangle (in local space)
        triangles[idx]->sample_surface(pos, normal, area);
        
        // 3. Shift position based on a random time within the interval
        float time = random_float(time0, time1);
        pos += center_at(time);
        
        // Area remains the same under translation
        area = sum_area;
    }

    virtual glm::vec3 random_pointing_vector(const glm::vec3& origin) const override { return glm::vec3(0.0f); }
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& wi) const override { return 0.0f; }
    
    virtual Material* get_material() const override { return mat_ptr.get(); }

    virtual void set_light_id(int id) override {
        Object::set_light_id(id); 
        for (auto& tri : triangles) tri->set_light_id(id);
    }

    glm::vec3 center_at(float time) const {
        return center0 + ((time - time0) / (time1 - time0)) * (center1 - center0);
    }

private:
    glm::vec3 center0, center1;
    float time0, time1;

    std::shared_ptr<BVHNode> bvh_root;
    std::shared_ptr<Material> mat_ptr;
    std::vector<std::shared_ptr<Object>> triangles;
    std::unique_ptr<Distribution1D> triangle_distribution;
    std::vector<std::shared_ptr<Material>> obj_materials; 
    float sum_area = 0.0f;

    void load_obj(const std::string& filename, std::shared_ptr<Material> global_mat,
                  glm::vec3 translation, float scale, glm::vec3 rot_axis, float rot_deg) 
    {
        tinyobj::ObjReaderConfig reader_config;
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

        auto fallback_mat = std::make_shared<Lambertian>(glm::vec3(0.5f));

        if (!global_mat) {
            std::cout << "[MovingMesh] Loading materials from MTL..." << std::endl;
            for (const auto& m : materials) {
                std::shared_ptr<Texture> albedo_tex;
                if (!m.diffuse_texname.empty()) {
                    std::string tex_path = base_dir + m.diffuse_texname;
                    albedo_tex = std::make_shared<ImageTexture>(tex_path.c_str());
                } else {
                    albedo_tex = std::make_shared<SolidColor>(glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]));
                }

                std::shared_ptr<Texture> normal_tex = nullptr;
                std::string normal_path = "";
                
                if (!m.normal_texname.empty()) {
                    normal_path = m.normal_texname;
                } else if (!m.bump_texname.empty()) {
                    normal_path = m.bump_texname; 
                }

                if (!normal_path.empty()) {
                    std::string full_path = base_dir + normal_path;
                    normal_tex = std::make_shared<ImageTexture>(full_path.c_str());
                }

                obj_materials.push_back(std::make_shared<Lambertian>(albedo_tex, normal_tex));
            }
        }

        float rad = glm::radians(rot_deg);
        glm::mat4 trans_mat = glm::mat4(1.0f);
        trans_mat = glm::translate(trans_mat, translation);
        trans_mat = glm::rotate(trans_mat, rad, rot_axis);
        trans_mat = glm::scale(trans_mat, glm::vec3(scale));

        auto transform_point = [&](float x, float y, float z) {
            glm::vec4 p(x, y, z, 1.0f);
            p = trans_mat * p;
            return glm::vec3(p);
        };

        glm::mat3 normal_mat = glm::mat3(glm::transpose(glm::inverse(trans_mat)));
        auto transform_normal = [&](float x, float y, float z) {
            return glm::normalize(normal_mat * glm::vec3(x, y, z));
        };

        std::cout << "[MovingMesh] Processing geometry for " << filename << " (" << shapes.size() << " shapes)..." << std::endl;
        std::vector<float> triangle_areas;

        for (size_t s = 0; s < shapes.size(); s++) {
            size_t index_offset = 0;
            
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
                
                if (fv != 3) {
                    index_offset += fv;
                    continue; 
                }

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

                for (size_t v_idx = 0; v_idx < 3; v_idx++) {
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v_idx];

                    v[v_idx] = transform_point(
                        attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                        attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                        attrib.vertices[3 * size_t(idx.vertex_index) + 2]
                    );

                    if (idx.normal_index >= 0) {
                        n[v_idx] = transform_normal(
                            attrib.normals[3 * size_t(idx.normal_index) + 0],
                            attrib.normals[3 * size_t(idx.normal_index) + 1],
                            attrib.normals[3 * size_t(idx.normal_index) + 2]
                        );
                    } else {
                        has_normals = false;
                    }

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

        sum_area = std::accumulate(triangle_areas.begin(), triangle_areas.end(), 0.0f);

        if (!triangles.empty()) {
            std::cout << "[MovingMesh] Building BVH for " << triangles.size() << " triangles..." << std::endl;
            bvh_root = std::make_shared<BVHNode>(triangles, 0.0f, 1.0f);
            triangle_distribution = std::make_unique<Distribution1D>(triangle_areas.data(), triangle_areas.size());
        }
    }
};