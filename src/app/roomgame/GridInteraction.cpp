#include "GridInteraction.h"
namespace roomgame
{
    GridInteraction::GridInteraction(int touchID, GridCell* start_cell, Room* room) {
        touchID_ = touchID;
        is_touched_ = true;
        last_timestamp_ = glfwGetTime();
        start_cell_ = start_cell;
        last_cell_ = start_cell;
        room_ = room;
    }

    GridInteraction::~GridInteraction() {
        // Do not free room pointer here
        // because valid rooms should still live when interaction is deleted
        // (grid manages room pointers)
    }

    void GridInteraction::update(glm::dvec2 position) {
        last_timestamp_ = glfwGetTime();
        last_position_ = position;
    }

    void GridInteraction::setLastCell(GridCell* cell) {
        last_timestamp_ = glfwGetTime();
        last_cell_ = cell;
    }

    bool GridInteraction::testTemporalAndSpatialProximity(GridInteraction* other) {
        //TODO use this class to resolve discontinuaties
        return false;
    }

    GridCell* GridInteraction::getStartCell() {
        return start_cell_;
    }

    GridCell* GridInteraction::getLastCell() {
        return last_cell_;
    }

    int GridInteraction::getTouchID() {
        return touchID_;
    }

    Room* GridInteraction::getRoom() {
        return room_;
    }
}
