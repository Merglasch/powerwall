#pragma once

#include "Room.h"
#include "SourceLightManager.h"
#include "GridInteraction.h"

namespace roomgame
{
    class InteractiveGrid;
    class MeshInstanceBuilder;
    /* Class that manages rooms that span over grid cells.
    * Adds possibility to create rooms by click and drag as well as repairing rooms by clicking
    * Holds a list of rooms.
    */
    class RoomInteractionManager {
    public:
        std::shared_ptr<InteractiveGrid> interactiveGrid_;
        std::shared_ptr<MeshInstanceBuilder> meshInstanceBuilder_;
        AutomatonUpdater* automatonUpdater_;
        std::vector<Room*> rooms_;
        void StartNewRoomInteractionAtTouchedCell(int touchID, GridCell*);
        void AdjustTemporaryRoomSize(GridCell*, std::shared_ptr<GridInteraction> interac);
        void FinalizeTemporaryRoom(std::shared_ptr<GridInteraction> interac);
        void ResetHealAmount();
        std::shared_ptr<roomgame::SourceLightManager> sourceLightManager_ = nullptr;
        bool checkRoomPosition(Room* newRoom);
        RoomInteractionManager();
        void updateHealthPoints(GridCell* cell, unsigned int hp);
        void reset();
        ~RoomInteractionManager();
        void startNewRoom(int touchID, GridCell* touchedCell);
        void TryRepair(GridCell* touchedCell);
        float healAmount_;
        bool getFirstRoom();
    private:
        const float DEFAULT_HEAL_AMOUNT = 0.25f;
        bool firstRoom = true;
        void checkConnection(Room* newRoom, int lengthX, int lengthY, int posX, int posY);
        void checkForNearInfections(Room * newRoom);
    };
}
