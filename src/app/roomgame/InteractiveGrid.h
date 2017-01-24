#ifndef INTERACTIVE_GRID_H_
#define INTERACTIVE_GRID_H_

#include <list>
#include <memory>
#include <sgct/Engine.h>
#include "core/gfx/GPUProgram.h"
#include "core/ApplicationNode.h"
#include "GridInteraction.h"

//template < int ARRAY_LEN >

class InteractiveGrid {
	// Data members
	float height_units_ = 2.0f;
	float cell_size_;
	float z_distance_;
	std::vector<std::vector<GridCell>> cells_; //TODO better use array with templated size
	// Render-related members
	GLuint vao_, vbo_;
	std::shared_ptr<viscom::GPUProgram> shader_;
	GLint mvp_uniform_location_, z_distance_uniform_location_;
	glm::mat4 model_matrix_;
	GLsizei num_vertices_;
	glm::mat4 last_sgctMVP_;
	// Input-related members
	glm::dvec2 last_mouse_position_; //TODO maybe replace with TUIO touch position
	std::list<GridInteraction*> interactions_;
public:
	InteractiveGrid(int columns, int rows);
	~InteractiveGrid();
	// Helper functions
	void forEachCell(std::function<void(GridCell*)> callback);
	void forEachCell(std::function<void(GridCell*,bool*)> callback);
	GridCell* getCellAt(glm::vec2 positionNDC);
	bool isInsideGrid(glm::vec2 positionNDC);
	bool isInsideCell(glm::vec2 positionNDC, GridCell* cell);
	glm::vec2 getNDC(glm::vec2 position);
	// Render functions
	void uploadVertexData();
	void loadShader(viscom::ApplicationNode* appNode);
	void render(glm::mat4 sgctMVP);
	void cleanup();
	// Input functions
	void onTouch(int touchID);
	void onRelease(int touchID);
	void onMouseMove(int touchID, double newx, double newy);

	void addRoom(GridCell* startCell, GridCell* endCell);
};

#endif