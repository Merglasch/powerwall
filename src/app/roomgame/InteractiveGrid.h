#pragma once

#include <list>
#include <memory>
#include "core/gfx/GPUProgram.h"
#include "core/ApplicationNodeBase.h"
#include "GridInteraction.h"

namespace roomgame {
    class RoomInteractionManager;

    /* Renderable and clickable grid.
    * Container for grid cell objects.
    * Supports a simple debug visualization (see onFrame function).
    * Provides a number of helper functions.
    */
    class InteractiveGrid {
    protected:
        // Data members
        float height_units_;
        float cell_size_;
        // Render-related members
        std::shared_ptr<viscom::GPUProgram> shader_;
        GLint mvp_uniform_location_;
        glm::vec3 translation_;
        GLsizei num_vertices_;
        glm::mat4 last_view_projection_;
        // Input-related members
        glm::vec3 last_ray_start_point_;
        glm::vec3 last_ray_intermediate_point_;
    public:
        std::vector<std::vector<GridCell>> cells_;
        std::list<std::shared_ptr<GridInteraction>> interactions_;
        std::shared_ptr<roomgame::RoomInteractionManager> roomInteractionManager_;
        glm::vec3 grid_center_;
        GLuint vao_, vbo_;

        /* Computes cell positions by iteratively adding (height/rows, height/rows) to (-1, -1) */
        InteractiveGrid(size_t columns, size_t rows, float height);
        ~InteractiveGrid();


        // Helper functions
        void forEachCell(std::function<void(GridCell*)> callback);
        void forEachCell(std::function<void(GridCell*, bool*)> callback);
        void forEachCellInRange(GridCell* leftLower, GridCell* rightUpper, std::function<void(GridCell*)> callback);
        void forEachCellInRange(GridCell* leftLower, GridCell* rightUpper, std::function<void(GridCell*, bool*)> callback);

        /* Binary search on cells */
        GridCell* getCellAt(glm::vec2 positionNDC);

        /* Picking with ray-plane-intersection */
        GridCell* pickCell(glm::vec3 rayStartPoint, glm::vec3 rayIntermediatePoint);

        bool isInsideGrid(glm::vec2 positionNDC);
        bool isInsideCell(glm::vec2 positionNDC, GridCell* cell);
        bool isColumnEmptyBetween(size_t col, size_t startRow, size_t endRow);
        bool isRowEmptyBetween(size_t row, size_t startCol, size_t endCol);
        glm::vec2 pushNDCinsideGrid(glm::vec2 positionNDC);

        // Getters
        float getCellSize();
        size_t getNumColumns();
        size_t getNumRows();
        size_t getNumCells();
        GridCell* getCellAt(size_t col, size_t row);
        glm::vec3 getTranslation() { return translation_; }


        // Render functions
        void uploadVertexData();
        void loadShader(viscom::GPUProgramManager);
        void onFrame();
        void cleanup();


        // Input and interaction functions
        void onTouch(int touchID);
        void onRelease(int touchID);
        void onMouseMove(int touchID, glm::vec3 rayStartPoint, glm::vec3 rayIntermediatePoint);

        /* Transform original cell position into what the user sees */
        glm::vec2 getNDC(glm::vec2 cellPosition);

        /* Transform cell position into absolute world coordinates */
        glm::vec3 getWorldCoordinates(glm::vec2 cellPosition);

        /* Remember camera projection for later being able to perform cell selection on user input */
        void updateProjection(glm::mat4&);

    };
}
