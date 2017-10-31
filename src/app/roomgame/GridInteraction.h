#pragma once

#include <glm/glm.hpp>
#include "GridCell.h"
namespace roomgame
{
    class InteractiveGrid;
    class Room;

    class GridInteraction {
        // TUIO ID
        int touchID_;
        // Touched-flag, that is independent of unintentional releases/discontinuaties
        bool is_touched_;
        // Last-values can help resolve discontinuaties
        glm::dvec2 last_position_;
        double last_timestamp_;
        GridCell* last_cell_;
        GridCell* start_cell_;
        Room* room_;
    public:
        GridInteraction(int touchID, GridCell* start_cell, Room* room);
        ~GridInteraction();
        void update(glm::dvec2 position);
        void setLastCell(GridCell* cell);
        bool testTemporalAndSpatialProximity(GridInteraction* other);
        GridCell* getStartCell();
        GridCell* getLastCell();
        int getTouchID();
        Room* getRoom();
    };
}
