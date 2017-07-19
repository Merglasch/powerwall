#ifndef MESH_INSTANCE_GRID
#define MESH_INSTANCE_GRID

#include "RoomInteractiveGrid.h"

class RoomSegmentMeshPool;
#include "RoomSegmentMeshPool.h"

/* Represents a grid with mesh instances at its cells
 * Has InteractiveGrid as base class.
 * Adds possibility to add/remove mesh instances through buildAt function.
 * Uses a meshpool to request instanced meshes.
 * Calls addInstance on requested meshes.
 */
class MeshInstanceGrid : public RoomInteractiveGrid {
protected:
	RoomSegmentMeshPool* meshpool_;
	void addInstanceAt(GridCell*, GLuint);
	void removeInstanceAt(GridCell*);
public:
	MeshInstanceGrid(size_t columns, size_t rows, float height, RoomSegmentMeshPool* meshpool);
	virtual void buildAt(size_t col, size_t row, GLuint buildState) override;
	void buildAt(GridCell*, GLuint);
	virtual void onMeshpoolInitialized();
};

#endif