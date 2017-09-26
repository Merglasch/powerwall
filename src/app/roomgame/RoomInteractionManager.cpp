#include "InteractiveGrid.h"
#include "MeshInstanceBuilder.h"
#include "AutomatonUpdater.h"
#include "RoomInteractionManager.h"
#include "../../../extern/fwcore/extern/assimp/code/FBXDocument.h"

namespace roomgame
{
    RoomInteractionManager::RoomInteractionManager() {
        healAmount_ = DEFAULT_HEAL_AMOUNT;
    }

    RoomInteractionManager::~RoomInteractionManager() {
        for (Room* r : rooms_) delete r;
    }

    void RoomInteractionManager::startNewRoom(int touchID, GridCell* touchedCell)
    {
        Room* room = new Room(touchedCell, touchedCell,interactiveGrid_, meshInstanceBuilder_);
        interactiveGrid_->interactions_.push_back(std::make_unique<GridInteraction>(touchID, touchedCell, room));
        room->updateCorners(touchedCell, touchedCell);
        meshInstanceBuilder_->buildAt(touchedCell->getCol(), touchedCell->getRow(), GridCell::INVALID | GridCell::TEMPORARY, MeshInstanceBuilder::BuildMode::Additive); // room covering one cell is invalid
    }

    void RoomInteractionManager::TryRepair(GridCell* touchedCell)
    {
        //std::cout << "Touched infected cell" << std::endl;
        GLuint north = touchedCell->getNorthNeighbor()->getBuildState();
        GLuint east = touchedCell->getEastNeighbor()->getBuildState();
        GLuint south = touchedCell->getSouthNeighbor()->getBuildState();
        GLuint west = touchedCell->getWestNeighbor()->getBuildState();

        GLuint test = GridCell::SOURCE | GridCell::INFECTED;

        GLuint currentHealth = touchedCell->getHealthPoints();
        GLuint updatedHealth = min(currentHealth + static_cast<GLuint>((GridCell::MAX_HEALTH - GridCell::MIN_HEALTH) * healAmount_), GridCell::MAX_HEALTH);

        GLuint andSides = north & south & east & west;

        if (andSides & test) {
            //std::cout << "Cell is in the middle of 4 infected cells" << std::endl;
            return;
        }
        else if (touchedCell->getBuildState() & GridCell::SOURCE) {
            if (((north & test) && !(north & GridCell::SOURCE)) |
                ((south & test) && !(south & GridCell::SOURCE)) |
                ((east & test) && !(east & GridCell::SOURCE)) |
                ((west & test) && !(west & GridCell::SOURCE))) {
                //std::cout << "Try to cure Source Cell but there are infected cells nerby" << std::endl;
            }
            else {
                //std::cout << "Cure source Cell" << std::endl;
                touchedCell->updateHealthPoints(interactiveGrid_->vbo_, updatedHealth);
                automatonUpdater_->updateAutomatonAt(touchedCell, touchedCell->getBuildState(), touchedCell->getHealthPoints());
                if (currentHealth >= GridCell::MAX_HEALTH) {
                    meshInstanceBuilder_->buildAt(touchedCell->getCol(), touchedCell->getRow(), GridCell::WALL, MeshInstanceBuilder::BuildMode::Additive);
                    meshInstanceBuilder_->buildAt(touchedCell->getCol(), touchedCell->getRow(), GridCell::SOURCE | GridCell::INFECTED, MeshInstanceBuilder::BuildMode::RemoveSpecific);
                    sourceLightManager_->DeleteClosestSourcePos(interactiveGrid_->getWorldCoordinates(touchedCell->getPosition()));
                }

            }
        }
        else {
            //std::cout << "Cure infected Cell" << std::endl;
            touchedCell->updateHealthPoints(interactiveGrid_->vbo_, updatedHealth);
            automatonUpdater_->updateAutomatonAt(touchedCell, touchedCell->getBuildState(), touchedCell->getHealthPoints());
            if (currentHealth >= GridCell::MAX_HEALTH) {
                meshInstanceBuilder_->buildAt(touchedCell->getCol(), touchedCell->getRow(), GridCell::INFECTED, MeshInstanceBuilder::BuildMode::RemoveSpecific);
            }
        }
    }

    void RoomInteractionManager::StartNewRoomInteractionAtTouchedCell(int touchID, GridCell* touchedCell) {
        // is the touched cell still empty?
        // ...then start room creation
        if (touchedCell->getBuildState() == GridCell::EMPTY) {
            startNewRoom(touchID, touchedCell);
        }
        // check if infected or source
        else if (touchedCell->getBuildState() & (GridCell::SOURCE | GridCell::INFECTED)) {
            TryRepair(touchedCell);
        }
    }

    void RoomInteractionManager::ResetHealAmount()
    {
        healAmount_ = DEFAULT_HEAL_AMOUNT;
    }

    void RoomInteractionManager::checkConnection(Room* newRoom, int lengthX, int lengthY, int posX, int posY) {
        bool connected = false;
        for (Room* r : rooms_) {
            if (r == newRoom) {
                continue;
            }
            if ((posX == (int)(r->leftLowerCorner_->getCol()) - 1) ||
                (posX == (int)(r->rightUpperCorner_->getCol()) + lengthX + 1)) {
                if (posY - (int)r->rightUpperCorner_->getRow() <= lengthY - 2 &&
                    posY - (int)r->leftLowerCorner_->getRow() >= 2) {
                    connected = true;
                    break;
                }
            }
            if ((posY == (int)(r->leftLowerCorner_->getRow()) - 1) ||
                (posY == (int)(r->rightUpperCorner_->getRow()) + lengthY + 1)) {
                if (posX - (int)r->rightUpperCorner_->getCol() <= lengthX - 2 &&
                    posX - (int)r->leftLowerCorner_->getCol() >= 2) {
                    connected = true;
                    break;
                }
            }
        }
        //set result
        newRoom->connected = connected;
    }

    void RoomInteractionManager::checkForNearInfections(Room* newRoom)
    {
        //Check for infected neighbouring rooms
        bool infectedNeighbours = false;
        int left2neighbour = static_cast<int>(newRoom->leftLowerCorner_->getCol() - 1);
        int bottom2neighbour = static_cast<int>(newRoom->leftLowerCorner_->getRow() - 1);
        int right2neighbour = static_cast<int>(newRoom->rightUpperCorner_->getCol() + 1);
        int top2neighbour = static_cast<int>(newRoom->rightUpperCorner_->getRow() + 1);

        interactiveGrid_->forEachCellInRange(&interactiveGrid_->cells_[left2neighbour][bottom2neighbour], &interactiveGrid_->cells_[right2neighbour][top2neighbour], static_cast<std::function<void(GridCell*)>>([&](GridCell* cell)
        {
            int column = static_cast<int>(cell->getCol());
            int row = static_cast<int>(cell->getRow());
            if (column == left2neighbour || column == right2neighbour || row == top2neighbour || row == bottom2neighbour)
            {
                if ((cell->getBuildState() & GridCell::INFECTED) != 0)
                {
                    infectedNeighbours = true;
                }
            }
        }));
        newRoom->infectedNeighbours = infectedNeighbours;
    }

    bool RoomInteractionManager::checkRoomPosition(Room* newRoom) {
        checkForNearInfections(newRoom);

        int lengthX = (int)newRoom->getColSize();
        int lengthY = (int)newRoom->getRowSize();
        int posX = (int)newRoom->rightUpperCorner_->getCol();
        int posY = (int)newRoom->rightUpperCorner_->getRow();
        //Check for collisions
        for (Room* r : rooms_) {
            if (r == newRoom) {
                continue;
            }
            if ((posX >= (int)(r->leftLowerCorner_->getCol())) &&
                (posX <= (int)(r->rightUpperCorner_->getCol()) + lengthX)) {

                if (posY >= r->leftLowerCorner_->getRow() &&
                    posY <= r->rightUpperCorner_->getRow() + lengthY) {
                    newRoom->collision = true;
                    return true;
                }
            }
        }
        //If there are no collisions...
        //...check for Connections
        checkConnection(newRoom, lengthX, lengthY, posX, posY);

        newRoom->collision = false;
        return false;
    }

    void RoomInteractionManager::AdjustTemporaryRoomSize(GridCell* hoveredCell, std::shared_ptr<GridInteraction> interac) {
        if (interac->getLastCell() == hoveredCell) return; // return if cursor was still inside last cell
        interac->setLastCell(hoveredCell);
        interac->getRoom()->clear();
        interac->getRoom()->updateCorners(interac->getStartCell(), hoveredCell);
        checkRoomPosition(interac->getRoom());
        interac->getRoom()->checkValidity(firstRoom);
        interac->getRoom()->fillRoom(interac->getStartCell(), interac->getLastCell(), true);

    }

    void RoomInteractionManager::FinalizeTemporaryRoom(std::shared_ptr<GridInteraction> interac) {
        // check result and remove interaction
        Room* room = interac->getRoom();
        if (room->isValid) { // if room valid, i.e. big enough, non-colliding, connecting, then store it
            if (rooms_.size() == 0) {//First room doesn't need to be connecting
                firstRoom = false;
            }
            room->clear();
            interac->getRoom()->fillRoom(interac->getRoom()->leftLowerCorner_, interac->getRoom()->rightUpperCorner_, false);
            room->finish();
            rooms_.push_back(room);
        }
        else { // if room invalid, discard
            room->clear();
            delete room;
            room = nullptr;
        }
        if (room != nullptr) {
            interactiveGrid_->forEachCellInRange(interac->getRoom()->leftLowerCorner_, interac->getRoom()->rightUpperCorner_, static_cast<std::function<void(GridCell*)>>([&](GridCell* cell) {
                meshInstanceBuilder_->deleteNeighbouringWalls(cell, false);
            }));
        }

        interactiveGrid_->interactions_.remove(interac);
    }

    void RoomInteractionManager::updateHealthPoints(GridCell* cell, unsigned int hp) {
        cell->updateHealthPoints(interactiveGrid_->vbo_, hp);
    }


    void RoomInteractionManager::reset() {
        for (Room* r : rooms_) delete r;
        rooms_.clear();
        firstRoom = true;
    }
}
