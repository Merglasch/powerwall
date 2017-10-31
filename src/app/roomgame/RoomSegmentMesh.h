#ifndef ROOM_SEGMENT_MESH_H
#define ROOM_SEGMENT_MESH_H
#include "core/gfx/mesh/MeshRenderable.h"
#include "core/gfx/mesh/SceneMeshNode.h"
#include "core/gfx/mesh/SubMesh.h"

#include "GameMesh.h"
#include <glm/gtc/matrix_inverse.hpp>
#include "core/gfx/Material.h"
#include "core/gfx/Texture.h"
#include "../Vertices.h"




/* Class for instanced meshes.
 * Especially intended for segments of rooms on a grid.
 * However usable for all instanced meshes on the grid so far.
 * 
*/
class RoomSegmentMesh : public SynchronizedInstancedMesh<roomgame::PerInstanceData> {
public:
    typedef roomgame::PerInstanceData Instance;
    /* Representation of a range in an instance buffer */
    struct InstanceBufferRange {
        RoomSegmentMesh* mesh_ = 0; // instanced mesh reference
        roomgame::InstanceBuffer* buffer_ = 0; // buffer reference
        int offset_instances_ = -1; // offset in units of instances
        int num_instances_ = -1; // instances in the range
    };
    /* Queue data structure for managing holes in buffers */
	struct NextFreeOffsetQueueElem {
		int offset_; // offset of the hole
		NextFreeOffsetQueueElem* el_; // next queue element
		NextFreeOffsetQueueElem(int off) : offset_(off), el_(0) {} // constructor
	};
private:
    //roomgame::InstanceBuffer room_ordered_buffer_; // instance buffer for finished room segments (feature not implemented yet)
	NextFreeOffsetQueueElem* next_free_offset_; // beginning of the hole offset queue
	NextFreeOffsetQueueElem* last_free_offset_; // end of the hole offset queue
public:
	RoomSegmentMesh(viscom::Mesh* mesh, viscom::GPUProgram* program, size_t pool_allocation_bytes);
	~RoomSegmentMesh();
    InstanceBufferRange addInstanceUnordered(Instance);
    InstanceBufferRange addInstanceUnordered_IMMEDIATE_GPU_UPLOAD(Instance); // deprecated because uncompatible with synchronization
	void removeInstanceUnordered(int offset_instances);
    InstanceBufferRange moveInstancesToRoomOrderedBuffer(std::initializer_list<int> offsets); // (feature not implemented yet)
	void renderAllInstances(std::function<void(void)> uniformSetter, const glm::mat4& view_projection, GLint isDebugMode, LightInfo* lightInfo = nullptr, glm::vec3& viewPos = glm::vec3(0, 0, 4));
};

#endif