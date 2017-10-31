#include "RoomSegmentMeshPool.h"
namespace roomgame
{
    RoomSegmentMeshPool::RoomSegmentMeshPool(const size_t MAX_INSTANCES) :
        // Estimate number of instances for room segments to minimize allocation cost (rather questionable heuristic)
        POOL_ALLOC_BYTES_CORNERS((MAX_INSTANCES) * sizeof(RoomSegmentMesh::Instance)),
        POOL_ALLOC_BYTES_WALLS((MAX_INSTANCES) * sizeof(RoomSegmentMesh::Instance)),
        POOL_ALLOC_BYTES_FLOORS((MAX_INSTANCES) * sizeof(RoomSegmentMesh::Instance)),
        POOL_ALLOC_BYTES_OUTER_INFLUENCE((MAX_INSTANCES) * sizeof(RoomSegmentMesh::Instance)),
        POOL_ALLOC_BYTES_DEFAULT(MAX_INSTANCES * sizeof(RoomSegmentMesh::Instance))
    {
        shader_ = 0;
    }

    RoomSegmentMeshPool::~RoomSegmentMeshPool() {}

    void RoomSegmentMeshPool::cleanup() {
        for (GLuint i : render_list_)
            for (RoomSegmentMesh* mesh : meshes_[i])
                delete mesh;
        owned_resources_.clear();
    }

    void RoomSegmentMeshPool::loadShader(viscom::GPUProgramManager mgr, std::shared_ptr<viscom::GPUProgram> instanceShader) {
        shader_ = instanceShader;
        depth_pass_flag_uniform_location_ = shader_->getUniformLocation("isDepthPass");
        debug_mode_flag_uniform_location_ = shader_->getUniformLocation("isDebugMode");
    }

    void RoomSegmentMeshPool::addMesh(std::vector<GLuint> types, std::shared_ptr<viscom::Mesh> mesh) {
        // Map one mesh to possibly multiple build states
        // (first build state is considered representative for the mesh)
        size_t pool_allocation_bytes = determinePoolAllocationBytes(types[0]); // use pool alloc bytes of representative build state
        RoomSegmentMesh* meshptr = new RoomSegmentMesh(mesh.get(), shader_.get(), pool_allocation_bytes);
        for (GLuint type : types) {
            // Copy the mesh pointer for each build state
            // (ensures that a mesh for a requested build state can quickly be found)
            meshes_[type].push_back(meshptr);
        }
        if (owned_resources_.find(mesh) == owned_resources_.end()) {
            // Store representative build state in render list
            // (ensures each mesh is only rendered once)
            render_list_.push_back(types[0]);
            // Hold the mesh resource in memory as long as the mesh pool lives
            owned_resources_.insert(mesh);
        }
    }

    void RoomSegmentMeshPool::addMeshVariations(std::vector<GLuint> types, std::vector<std::shared_ptr<viscom::Mesh>> mesh_variations) {
        // Map possibly multiple meshes to possibly multiple build states
        // (if one of these build states is requested, a mesh is chosen randomly to create variation)
        for (std::shared_ptr<viscom::Mesh> variation : mesh_variations)
            addMesh(types, variation);
    }

    void RoomSegmentMeshPool::updateUniformEveryFrame(std::string uniform_name, std::function<void(GLint)> update_func) {
        uniform_locations_.push_back(shader_->getUniformLocation(uniform_name));
        uniform_callbacks_.push_back(update_func);
    }

    RoomSegmentMesh* RoomSegmentMeshPool::getMeshOfType(GLuint type) {
        // Ignore orientation bits (so that walls and corners can be added once and reused)
        type &= ~(GridCell::TOP | GridCell::BOTTOM | GridCell::RIGHT | GridCell::LEFT);
        // Request mapped mesh(es)
        std::vector<RoomSegmentMesh*> mesh_variations;
        try {
            mesh_variations = meshes_.at(type);
        }
        catch (std::out_of_range) {
            // If the pool does not have the requested type
            printf("Pool has no mesh for type %d\n", (int)type);
            return 0;
        }
        unsigned int variation = 0;
        if (mesh_variations.size() > 1) {
            srand((unsigned int)time(0));
            variation = rand() % mesh_variations.size();
        }
        return mesh_variations[variation];
    }

    void RoomSegmentMeshPool::filter(GLuint buildStateBits, std::function<void(GLuint)> callback) {
        // Aggregate buildstate bits of all mesh mappings, that overlap with the filter
        // Example 1: If meshpool has one mesh for buildstates A and B,
        //            A|B is formed and the overlap with buildStateBits returned in callback
        // Example 2: If meshpool has different meshes for buildstates A and B,
        //            callback is called twice with an overlap of each builstate with buildStateBits
        std::unordered_map<RoomSegmentMesh*, GLuint> aggregatedBuildStates;
        for (std::pair<GLuint, std::vector<RoomSegmentMesh*>> stateMeshMapping : meshes_) {
            GLuint mappedBuildStateBits = stateMeshMapping.first;
            RoomSegmentMesh* mappedMesh = stateMeshMapping.second[0];
            GLuint overlap = mappedBuildStateBits & buildStateBits;
            if (overlap)
                aggregatedBuildStates[mappedMesh] |= overlap; // aggregation
        }
        // Attach present orientation bits (so that shader can rotate room segments)
        GLuint orientationBits = buildStateBits & (GridCell::TOP | GridCell::BOTTOM | GridCell::RIGHT | GridCell::LEFT);
        for (std::pair<RoomSegmentMesh*, GLuint> aggBuildStateBits : aggregatedBuildStates)
            callback(aggBuildStateBits.second | orientationBits);
    }

    void RoomSegmentMeshPool::renderAllMeshes(glm::mat4& view_projection, GLint isDepthPass, GLint isDebugMode, LightInfo* lightInfo, glm::vec3& viewPos) {
        for (GLuint i : render_list_) {
            for (RoomSegmentMesh* mesh : meshes_[i]) {
                mesh->renderAllInstances([&]() {
                    for (unsigned int i = 0; i < uniform_locations_.size(); i++) uniform_callbacks_[i](uniform_locations_[i]);
                    glUniform1i(depth_pass_flag_uniform_location_, isDepthPass);
                }, view_projection, isDebugMode, lightInfo, viewPos);
            }
        }
    }

    void RoomSegmentMeshPool::renderAllMeshesExcept(glm::mat4& view_projection, GLuint type_not_to_render, GLint isDepthPass, GLint isDebugMode, LightInfo* lightInfo, glm::vec3& viewPos) {
        for (GLuint i : render_list_) {
            if ((i & type_not_to_render) != 0) continue;
            for (RoomSegmentMesh* mesh : meshes_[i]) {
                mesh->renderAllInstances([&]() {
                    for (unsigned int i = 0; i < uniform_locations_.size(); i++) uniform_callbacks_[i](uniform_locations_[i]);
                    glUniform1i(depth_pass_flag_uniform_location_, isDepthPass);
                }, view_projection, isDebugMode, lightInfo, viewPos);
            }
        }
    }

    void RoomSegmentMeshPool::preSync() { // master
        for (GLuint i : render_list_) {
            for (RoomSegmentMesh* mesh : meshes_[i]) {
                mesh->preSync();
            }
        }
    }

    void RoomSegmentMeshPool::encode() { // master
        for (GLuint i : render_list_) {
            for (RoomSegmentMesh* mesh : meshes_[i]) {
                mesh->encode();
            }
        }
    }

    void RoomSegmentMeshPool::decode() { // slave
        for (GLuint i : render_list_) {
            for (RoomSegmentMesh* mesh : meshes_[i]) {
                mesh->decode();
            }
        }
    }

    void RoomSegmentMeshPool::updateSyncedSlave() {
        for (GLuint i : render_list_) {
            for (RoomSegmentMesh* mesh : meshes_[i]) {
                mesh->updateSyncedSlave();
            }
        }
    }

    void RoomSegmentMeshPool::updateSyncedMaster() {
        for (GLuint i : render_list_) {
            for (RoomSegmentMesh* mesh : meshes_[i]) {
                mesh->updateSyncedMaster();
            }
        }
    }

    GLint RoomSegmentMeshPool::getUniformLocation(size_t index) {
        return uniform_locations_[index];
    }

    GLuint RoomSegmentMeshPool::getShaderID() {
        return shader_->getProgramId();
    }

    size_t RoomSegmentMeshPool::determinePoolAllocationBytes(GLuint type) {
        if ((type & GridCell::CORNER) != 0) {
            return POOL_ALLOC_BYTES_CORNERS;
        }
        else if ((type & GridCell::INSIDE_ROOM) != 0) {
            return POOL_ALLOC_BYTES_FLOORS;
        }
        else if ((type & GridCell::WALL) != 0) {
            return POOL_ALLOC_BYTES_WALLS;
        }
        else if ((type & GridCell::OUTER_INFLUENCE) != 0) {
            return POOL_ALLOC_BYTES_OUTER_INFLUENCE;
        }
        else {
            return POOL_ALLOC_BYTES_DEFAULT;
        }
    }
}
