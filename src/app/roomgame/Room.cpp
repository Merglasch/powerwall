#include "Room.h"
#include "InteractiveGrid.h"

namespace roomgame
{
    Room::Room(GridCell* leftLowerCorner, GridCell* rightUpperCorner, std::shared_ptr<InteractiveGrid> grid, std::shared_ptr<MeshInstanceBuilder> buildHelper) {
        leftLowerCorner_ = leftLowerCorner;
        rightUpperCorner_ = rightUpperCorner;
        grid_ = grid;
        meshInstanceBuilder_ = buildHelper;
        isFinished_ = false;
    }

    Room::~Room() {

    }

    void Room::clear() {
        grid_->forEachCellInRange(leftLowerCorner_, rightUpperCorner_, [&](GridCell* cell) {
            meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::TEMPORARY | GridCell::INVALID, MeshInstanceBuilder::BuildMode::RemoveSpecific);
        });
    }

    void Room::invalidate() {
        grid_->forEachCellInRange(leftLowerCorner_, rightUpperCorner_, [&](GridCell* cell) {
            meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::TEMPORARY | GridCell::INVALID, MeshInstanceBuilder::BuildMode::Additive);
        });
    }

    void Room::validate() {
        grid_->forEachCellInRange(leftLowerCorner_, rightUpperCorner_, [&](GridCell* cell) {
            meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::TEMPORARY | GridCell::INVALID, MeshInstanceBuilder::BuildMode::RemoveSpecific);
        });
    }

    bool Room::checkValidity(bool firstRoom) {
        size_t hsize = getColSize();
        size_t vsize = getRowSize();

        bool sizeValid = (hsize >= MIN_SIZE) && (vsize >= MIN_SIZE) && (hsize * vsize <= MAX_SIZE);

        bool connectedToARoom = false;

        if (firstRoom) {
            connectedToARoom = true;
        }
        else {
            connectedToARoom = connected;
        }
        isValid = sizeValid && connectedToARoom && !collision && !infectedNeighbours;
        if (isValid) {
            validate();
        }
        else {
            invalidate();
        }
        return isValid;
    }

    size_t Room::getColSize() {
        return rightUpperCorner_->getCol() - leftLowerCorner_->getCol();
    }

    size_t Room::getRowSize() {
        return rightUpperCorner_->getRow() - leftLowerCorner_->getRow();
    }

    void Room::finish() {
        isFinished_ = true;
        //TODO Move mesh instances from unordered to room-ordered buffers (for each mesh that is part of the room)
        // 1) Get instance buffer range for each cell of this room
        // 2) Copy instance to ordered buffer
        // 3) Remove instance from unordered buffer
        // 4) Update cell's mesh instance buffer range
        // 5) Get the whole ordered buffer range and push into mesh_instances_ vector
        // 6) Prevent growing/shrinking/spanning of finished rooms
        // 7) Clearing a finished room should update build state for each cell but remove only 1 buffer range in the ordered buffer! How to do this?
    }


    void Room::updateCorners(GridCell* startCell, GridCell* endCell) {
        size_t minX = min(startCell->getCol(), endCell->getCol());
        size_t minY = min(startCell->getRow(), endCell->getRow());
        size_t maxX = max(startCell->getCol(), endCell->getCol());
        size_t maxY = max(startCell->getRow(), endCell->getRow());

        leftLowerCorner_ = grid_->getCellAt(minX, minY);
        rightUpperCorner_ = grid_->getCellAt(maxX, maxY);
    }

    void Room::fillRoom(GridCell* startCell, GridCell* endCell, bool temporary) {
        int temporaryCell = temporary ? GridCell::TEMPORARY : GridCell::EMPTY;
        //get corners
        size_t minX = min(startCell->getCol(), endCell->getCol());
        size_t minY = min(startCell->getRow(), endCell->getRow());
        size_t maxX = max(startCell->getCol(), endCell->getCol());
        size_t maxY = max(startCell->getRow(), endCell->getRow());

        GridCell* leftLowerCorner = grid_->getCellAt(minX, minY);
        GridCell* rightUpperCorner = grid_->getCellAt(maxX, maxY);
        GridCell* leftUpperCorner = grid_->getCellAt(minX, maxY);
        GridCell* rightLowerCorner = grid_->getCellAt(maxX, minY);

        if (temporary) {
            grid_->forEachCellInRange(leftLowerCorner_, rightUpperCorner_, [&](GridCell* cell) {
                meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::TEMPORARY, MeshInstanceBuilder::BuildMode::Additive);
            });
        }
        else {
            // Set build states
            meshInstanceBuilder_->buildAt(minX, minY, GridCell::LEFT | GridCell::BOTTOM | GridCell::CORNER, MeshInstanceBuilder::BuildMode::Replace);
            meshInstanceBuilder_->buildAt(maxX, maxY, GridCell::RIGHT | GridCell::TOP | GridCell::CORNER, MeshInstanceBuilder::BuildMode::Replace);
            meshInstanceBuilder_->buildAt(minX, maxY, GridCell::LEFT | GridCell::TOP | GridCell::CORNER, MeshInstanceBuilder::BuildMode::Replace);
            meshInstanceBuilder_->buildAt(maxX, minY, GridCell::RIGHT | GridCell::BOTTOM | GridCell::CORNER, MeshInstanceBuilder::BuildMode::Replace);

            grid_->forEachCellInRange(leftUpperCorner->getEastNeighbor(), rightUpperCorner->getWestNeighbor(), [&](GridCell* cell) {
                meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::WALL | GridCell::TOP, MeshInstanceBuilder::BuildMode::Replace);
            });
            grid_->forEachCellInRange(leftLowerCorner->getEastNeighbor(), rightLowerCorner->getWestNeighbor(), [&](GridCell* cell) {
                meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::WALL | GridCell::BOTTOM, MeshInstanceBuilder::BuildMode::Replace);
            });
            grid_->forEachCellInRange(leftLowerCorner->getNorthNeighbor(), leftUpperCorner->getSouthNeighbor(), [&](GridCell* cell) {
                meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::WALL | GridCell::LEFT, MeshInstanceBuilder::BuildMode::Replace);
            });
            grid_->forEachCellInRange(rightLowerCorner->getNorthNeighbor(), rightUpperCorner->getSouthNeighbor(), [&](GridCell* cell) {
                meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::WALL | GridCell::RIGHT, MeshInstanceBuilder::BuildMode::Replace);
            });

            GridCell* insideLeftLower = leftLowerCorner->getEastNeighbor()->getNorthNeighbor();
            GridCell* insideRightUpper = rightUpperCorner->getWestNeighbor()->getSouthNeighbor();
            grid_->forEachCellInRange(insideLeftLower, insideRightUpper, [&](GridCell* cell) {
                meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::INSIDE_ROOM, MeshInstanceBuilder::BuildMode::Replace);
            });
        }
    }
}
