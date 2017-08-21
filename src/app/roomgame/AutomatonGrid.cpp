#include "AutomatonGrid.h"
#include "GPUCellularAutomaton.h"

AutomatonGrid::AutomatonGrid(size_t cols, size_t rows, float height, RoomSegmentMeshPool* meshpool) :
	MeshInstanceGrid(cols, rows, height, meshpool)
{
	automaton_ = 0;
	delayed_update_list_ = 0;
}

AutomatonGrid::~AutomatonGrid() {
	DelayedUpdate* dup = delayed_update_list_;
	while (dup) {
		DelayedUpdate* next = dup->next_;
		delete dup;
		dup = next;
	}
}

void AutomatonGrid::setCellularAutomaton(GPUCellularAutomaton* automaton) {
	automaton_ = automaton;
}

void AutomatonGrid::buildAt(size_t col, size_t row, GLuint buildStateBits, BuildMode buildMode) {
    // Called on user input (grid update -> automaton update)

    GridCell* c = getCellAt(col, row);
    if (!c) return;
    MeshInstanceGrid::buildAt(c, buildStateBits, buildMode);
    c->updateHealthPoints(vbo_, GridCell::MAX_HEALTH);
    // Route results to automaton
    automaton_->updateCell(c, c->getBuildState(), c->getHealthPoints());
}

void AutomatonGrid::updateCell(GridCell* c, GLuint state, int hp) {
	// Called on automaton transitions (automaton update -> grid update)
	if (c->getBuildState() == SIMULATED_STATE && state == GridCell::EMPTY) {
		DelayedUpdate* tmp = delayed_update_list_;
		delayed_update_list_ = new DelayedUpdate(1, c, state);
		delayed_update_list_->next_ = tmp;
		return;
	}
    MeshInstanceGrid::buildAt(c, state, InteractiveGrid::BuildMode::Replace);
    c->updateHealthPoints(vbo_, hp); // thinking of dynamic inner influence...
	// a fixed-on-cell health is not very practical
}

void AutomatonGrid::onTransition() {
	// Traverse delayed updates from previous transitions...
	DelayedUpdate* dup = delayed_update_list_;
	DelayedUpdate* last = 0;
	while (dup) {
		dup->wait_count_--;
		// ... and perform updates that are due
		if (dup->wait_count_ == 0) {
            MeshInstanceGrid::buildAt(dup->target_, dup->to_, InteractiveGrid::BuildMode::Additive);
			if (last) {
				last->next_ = dup->next_;
				delete dup;
				dup = last->next_;
			}
			else {
				delayed_update_list_ = dup->next_;
				delete dup;
				dup = delayed_update_list_;
			}
		}
		else {
			last = dup;
			dup = dup->next_;
		}
	}
}

void AutomatonGrid::populateCircleAtLastMousePosition(int radius) {
    /*
    glm::vec2 touchPositionNDC =
    glm::vec2(last_mouse_position_.x, 1.0 - last_mouse_position_.y)
    * glm::vec2(2.0, 2.0) - glm::vec2(1.0, 1.0);
    GridCell* startCell = getCellAt(touchPositionNDC);*/
    GridCell* startCell = pickCell(last_ray_start_point_, last_ray_intermediate_point_);
    if (!startCell) return;
    for (int x = -radius; x < radius; x++) {
        for (int y = -radius; y < radius; y++) {
            GridCell* c = getCellAt(startCell->getCol() + x, startCell->getRow() + y);
            if (!c) continue;
            if (c->getDistanceTo(startCell) > radius) continue;
            buildAt(c->getCol(), c->getRow(), SIMULATED_STATE, BuildMode::Additive);
        }
    }
}