#include "RoomSegmentMeshPool.h"
#include "InteractiveGrid.h"
#include "MeshInstanceBuilder.h"
#include "AutomatonUpdater.h"
namespace roomgame
{
    MeshInstanceBuilder::MeshInstanceBuilder(RoomSegmentMeshPool* meshpool)
    {
        meshpool_ = meshpool;
    }

    /* Called only on master (resulting instance buffer is synced) */
    void MeshInstanceBuilder::addInstanceAt(GridCell* c, GLuint buildStateBits) {
        RoomSegmentMesh::Instance instance;
        float cell_size_ = interactiveGrid_->getCellSize();
        instance.scale = cell_size_ / 1.98f; // assume model extends [-1,1]^3
        instance.translation = interactiveGrid_->getTranslation(); // grid translation
        instance.translation += glm::vec3(c->getPosition(), 0.0f); // + relative cell translation
        instance.translation += glm::vec3(cell_size_ / 2.0f, -cell_size_ / 2.0f, 0.0f); // + origin to middle of cell
        instance.health = c->getHealthPoints();

        // get a mesh instance for all given buildstate bits that have a mapping in the meshpool
        meshpool_->filter(buildStateBits, [&](GLuint renderableBuildState) {

            // make the shader see only the subset of build states for which this mesh was added to the pool
            instance.buildState = renderableBuildState;
            
            RoomSegmentMesh* mesh = meshpool_->getMeshOfType(renderableBuildState);
            if (!mesh) {
                return;
            }
            RoomSegmentMesh::InstanceBufferRange bufrange = mesh->addInstanceUnordered(instance);
            c->pushMeshInstance(bufrange);
        });
    }

    /* Called only on master (resulting instance buffer is synced) */
    void MeshInstanceBuilder::removeInstanceAt(GridCell* c) {
        RoomSegmentMesh::InstanceBufferRange bufrange;
        while ((bufrange = c->popMeshInstance()).mesh_) {
            bufrange.mesh_->removeInstanceUnordered(bufrange.offset_instances_);
        }
    }

    bool MeshInstanceBuilder::deleteNeighbouringWalls(GridCell* cell, bool simulate) {
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
                auto wallReplacementFunc = [&](GLuint oldState) {
                    GLuint modified = oldState & (GridCell::EMPTY | GridCell::INVALID | GridCell::SOURCE | GridCell::INFECTED | GridCell::OUTER_INFLUENCE);
                    modified |= GridCell::INSIDE_ROOM;
                    return modified;
                };
                buildAt(cell->getCol(), cell->getRow(), wallReplacementFunc);
                buildAt(neighbour->getCol(), neighbour->getRow(), wallReplacementFunc);
                return true;
            }
        }
        return false;
    }


    void MeshInstanceBuilder::buildAt(GridCell* c, std::function<GLuint(GLuint)> buildStateModifyFunction) {
        GLuint current = c->getBuildState();
        GLuint newSt = buildStateModifyFunction(current);
        if (current == newSt) return;
        else if (current == GridCell::EMPTY) addInstanceAt(c, newSt);
        else if (newSt == GridCell::EMPTY) removeInstanceAt(c);
        else {
            removeInstanceAt(c);
            addInstanceAt(c, newSt);
        }
        if ((newSt & (GridCell::SOURCE | GridCell::INFECTED | GridCell::TEMPORARY)) == 0)
        {
            c->updateHealthPoints(interactiveGrid_->vbo_, GridCell::MAX_HEALTH);
        }
        c->setBuildState(newSt);
        c->updateBuildState(interactiveGrid_->vbo_);
        automatonUpdater_->updateAutomatonAt(c, newSt, c->getHealthPoints());
    }

    void MeshInstanceBuilder::buildAt(GridCell* c, GLuint newState, BuildMode buildMode) {
        buildAt(c,[&](GLuint oldState)
        {
            GLuint modified;
            GLuint newStateInternal = newState;
            switch (buildMode) {
            case BuildMode::Additive:
                modified = oldState | newStateInternal;
                break;
            case BuildMode::Replace:
                modified = newStateInternal;
                break;
            case BuildMode::RemoveSpecific:
                modified = (oldState | newStateInternal) ^ newStateInternal;
                break;
            }
            return modified;
        });
    }

    void MeshInstanceBuilder::buildAt(size_t col, size_t row, std::function<GLuint(GLuint)> buildStateModifyFunction) {
        GridCell* maybeCell = interactiveGrid_->getCellAt(col, row);
        if (maybeCell) buildAt(maybeCell, buildStateModifyFunction);
    }

    void MeshInstanceBuilder::buildAt(size_t col, size_t row, GLuint newState, BuildMode buildMode) {
        GridCell* maybeCell = interactiveGrid_->getCellAt(col, row);
        if (maybeCell) buildAt(maybeCell, newState, buildMode);
    }
}
