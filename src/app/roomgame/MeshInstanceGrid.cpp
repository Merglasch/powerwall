#include "RoomSegmentMeshPool.h"
#include "InteractiveGrid.h"
#include "MeshInstanceGrid.h"
#include "AutomatonGrid.h"
namespace roomgame
{
    MeshInstanceGrid::MeshInstanceGrid(RoomSegmentMeshPool* meshpool)
    {
        meshpool_ = meshpool;
    }

    /* Called only on master (resulting instance buffer is synced) */
    void MeshInstanceGrid::addInstanceAt(GridCell* c, GLuint buildStateBits) {
        RoomSegmentMesh::Instance instance;
        float cell_size_ = interactiveGrid_->getCellSize();
        instance.scale = cell_size_ / 1.98f; // assume model extends [-1,1]^3
        instance.translation = interactiveGrid_->getTranslation(); // grid translation
        instance.translation += glm::vec3(c->getPosition(), 0.0f); // + relative cell translation
        instance.translation += glm::vec3(cell_size_ / 2.0f, -cell_size_ / 2.0f, 0.0f); // + origin to middle of cell
        instance.health = c->getHealthPoints();
        // get a mesh instance for all given buildstate bits that have a mapping in the meshpool
        meshpool_->filter(buildStateBits, [&](GLuint renderableBuildState) {
            instance.buildState = renderableBuildState; // helps differentiating 2 meshes on 1 cell in shader
            RoomSegmentMesh* mesh = meshpool_->getMeshOfType(renderableBuildState);
            if (!mesh) {
                return;
            }
            RoomSegmentMesh::InstanceBufferRange bufrange = mesh->addInstanceUnordered(instance);
            c->pushMeshInstance(bufrange);
        });
    }

    /* Called only on master (resulting instance buffer is synced) */
    void MeshInstanceGrid::removeInstanceAt(GridCell* c) {
        RoomSegmentMesh::InstanceBufferRange bufrange;
        while ((bufrange = c->popMeshInstance()).mesh_) {
            bufrange.mesh_->removeInstanceUnordered(bufrange.offset_instances_);
        }
    }

    bool MeshInstanceGrid::deleteNeighbouringWalls(GridCell* cell, bool simulate) {
        GLuint cellState = cell->getBuildState();
        if ((cellState & GridCell::WALL) != 0) {
            GridCell* neighbour = nullptr;
            GLuint neighbourCellState;
            if ((cellState & GridCell::RIGHT) != 0) {
                neighbour = cell->getEastNeighbor();
                if (neighbour == nullptr) {
                    return false;
                }
                neighbourCellState = neighbour->getBuildState();
                if ((neighbourCellState & GridCell::WALL) == 0) {
                    neighbour = nullptr;
                }
            }
            else if ((cellState & GridCell::LEFT) != 0) {
                neighbour = cell->getWestNeighbor();
                if (neighbour == nullptr) {
                    return false;
                }
                neighbourCellState = neighbour->getBuildState();
                if ((neighbourCellState & GridCell::WALL) == 0) {
                    neighbour = nullptr;
                }
            }
            else if ((cellState & GridCell::TOP) != 0) {
                neighbour = cell->getNorthNeighbor();
                if (neighbour == nullptr) {
                    return false;
                }
                neighbourCellState = neighbour->getBuildState();
                if ((neighbourCellState & GridCell::WALL) == 0) {
                    neighbour = nullptr;
                }
            }
            else if ((cellState & GridCell::BOTTOM) != 0) {
                neighbour = cell->getSouthNeighbor();
                if (neighbour == nullptr) {
                    return false;
                }
                neighbourCellState = neighbour->getBuildState();
                if ((neighbourCellState & GridCell::WALL) == 0) {
                    neighbour = nullptr;
                }
            }
            if (neighbour != nullptr) {
                if (simulate) {
                    return true;
                }
                buildAt(cell->getCol(), cell->getRow(), [&](GridCell* c) {
                    GLuint old = c->getBuildState();
                    GLuint modified = old & (GridCell::EMPTY | GridCell::INVALID | GridCell::SOURCE | GridCell::INFECTED | GridCell::OUTER_INFLUENCE);
                    modified |= GridCell::INSIDE_ROOM;
                    c->setBuildState(modified);
                });
                buildAt(neighbour->getCol(), neighbour->getRow(), [&](GridCell* c) {
                    GLuint old = c->getBuildState();
                    GLuint modified = old & (GridCell::EMPTY | GridCell::INVALID | GridCell::SOURCE | GridCell::INFECTED | GridCell::OUTER_INFLUENCE);
                    modified |= GridCell::INSIDE_ROOM;
                    c->setBuildState(modified);
                });
                return true;
            }
        }
        return false;
    }


    void MeshInstanceGrid::buildAt(GridCell* c, std::function<void(GridCell*)> callback) {
        GLuint current = c->getBuildState();
        callback(c);
        GLuint newSt = c->getBuildState();
        c->setBuildState(current);
        if (current == newSt) return;
        else if (current == GridCell::EMPTY) addInstanceAt(c, newSt);
        else if (newSt == GridCell::EMPTY) removeInstanceAt(c);
        else {
            removeInstanceAt(c);
            addInstanceAt(c, newSt);
        }
        c->setBuildState(newSt);
        c->updateBuildState(interactiveGrid_->vbo_);
        c->updateHealthPoints(interactiveGrid_->vbo_, GridCell::MAX_HEALTH);
        automatonGrid_->updateAutomatonAt(c, newSt, c->getHealthPoints());
    }

    void MeshInstanceGrid::buildAt(GridCell* c, GLuint newState, BuildMode buildMode) {
        GLuint current = c->getBuildState();
        GLuint moddedState;
        switch (buildMode) {
        case BuildMode::Additive:
            moddedState = current | newState;
            break;
        case BuildMode::Replace:
            moddedState = newState;
            break;
        case BuildMode::RemoveSpecific:
            moddedState = (current | newState) ^ newState;
            break;
        }
        if (current == moddedState) return;
        else if (current == GridCell::EMPTY) addInstanceAt(c, moddedState);
        else if (moddedState == GridCell::EMPTY) removeInstanceAt(c);
        else {
            removeInstanceAt(c);
            addInstanceAt(c, moddedState);
        }
        c->setBuildState(moddedState);
        c->updateBuildState(interactiveGrid_->vbo_);
        c->updateHealthPoints(interactiveGrid_->vbo_, GridCell::MAX_HEALTH);
        automatonGrid_->updateAutomatonAt(c, moddedState, c->getHealthPoints());
    }

    void MeshInstanceGrid::buildAt(size_t col, size_t row, std::function<void(GridCell*)> callback) {
        GridCell* maybeCell = interactiveGrid_->getCellAt(col, row);
        if (maybeCell) buildAt(maybeCell, callback);
    }

    void MeshInstanceGrid::buildAt(size_t col, size_t row, GLuint newState, BuildMode buildMode) {
        GridCell* maybeCell = interactiveGrid_->getCellAt(col, row);
        if (maybeCell) buildAt(maybeCell, newState, buildMode);
    }
}
