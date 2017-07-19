#ifndef ROOM_SEGMENT_MESH_POOL_H
#define ROOM_SEGMENT_MESH_POOL_H

#include <memory>
#include <stdexcept>
#include <functional>
#include "core/gfx/mesh/MeshRenderable.h"
#include "../Vertices.h"
#include "InteractiveGrid.h"
#include "RoomSegmentMesh.h"

/* Management class for room segment meshes and mesh instance shader
 * Mesh instances of one type can be arbitrary distributed across a grid.
 * There can be several types of meshes (room walls, corners, inner influence etc.).
 * The mesh pool uses build states to identifly mesh types and maps build states to meshes.
 * An interactive grid that received a build call uses the pool to get an appropriate mesh for a build state.
 * Mesh pool exists on all SGCT nodes
 * Used by nodes to create meshes (each including vertex and instance buffer)
 * Used by MeshInstanceGrid to get appropriate mesh for build state
 * Used by nodes to render all meshes by a single call
*/
class RoomSegmentMeshPool {
	// Map build state to multiple mesh variations
	// (when there is one mesh for multiple build states,
	// store the same pointer multiple times)
	std::unordered_map<GLuint, std::vector<RoomSegmentMesh*>> meshes_;
	// Store build states contiguously for faster rendering
	// (only store one representative build state for one mesh)
	std::vector<GLuint> render_list_;
	// Hold pointers to all meshes to control cleanup
	std::set<std::shared_ptr<viscom::Mesh>> owned_resources_;
	// The shader used by all meshes
	std::shared_ptr<viscom::GPUProgram> shader_;
	// Uniforms
	std::vector<GLint> uniform_locations_;
	std::vector<std::function<void(GLint)>> uniform_callbacks_;
	GLint depth_pass_flag_uniform_location_;
	GLint debug_mode_flag_uniform_location_;
public:
	RoomSegmentMeshPool(const size_t MAX_INSTANCES);
	~RoomSegmentMeshPool();
	// Functions for initialization
	void loadShader(viscom::GPUProgramManager mgr);
	void addMesh(std::vector<GLuint> types, std::shared_ptr<viscom::Mesh> mesh);
	void addMeshVariations(std::vector<GLuint> types, std::vector<std::shared_ptr<viscom::Mesh>> mesh_variations);
    // Function for uniform shader data
    void updateUniformEveryFrame(std::string uniform_name, std::function<void(GLint)> update_func);
	// Function for mesh requests
	RoomSegmentMesh* getMeshOfType(GLuint type);
	// Functions for rendering
	void renderAllMeshes(glm::mat4& view_projection, GLint isDepthPass = 0, GLint isDebugMode = 0);
	void renderAllMeshesExcept(glm::mat4& view_projection, GLuint type_not_to_render, GLint isDepthPass = 0, GLint isDebugMode = 0);
	void cleanup();
    // Functions for SGCT synchronization
    void preSync(); // master
    void encode(); // master
    void decode(); // slave
    void updateSyncedSlave();
    void updateSyncedMaster();
	// Getter
	GLint getUniformLocation(size_t index);
	GLuint getShaderID();
private:
	// Pool allocation bytes based on estimated number of instances
	const size_t POOL_ALLOC_BYTES_CORNERS;
	const size_t POOL_ALLOC_BYTES_WALLS;
	const size_t POOL_ALLOC_BYTES_FLOORS;
	const size_t POOL_ALLOC_BYTES_OUTER_INFLUENCE;
	const size_t POOL_ALLOC_BYTES_DEFAULT;
	size_t determinePoolAllocationBytes(GLuint type);
};

#endif