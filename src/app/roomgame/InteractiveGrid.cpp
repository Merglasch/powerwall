#include "InteractiveGrid.h"


InteractiveGrid::InteractiveGrid(size_t columns, size_t rows, float height) {
	height_units_ = height;
	cell_size_ = height_units_ / float(rows);
	for (int x = 0; x < columns; x++) {
		std::vector<GridCell> column;
		for (int y = 0; y < rows; y++) {
			GridCell current(-1.0f + x * cell_size_, -1.0f + y * cell_size_, x, y);
			column.push_back(current);
		}
		cells_.push_back(column);
	}
	for (int x = 0; x < columns; x++) {
		for (int y = 0; y < rows; y++) {
			cells_[x][y].setNorthNeighbor((y == rows - 1) ? 0 : &cells_[x][y + 1]);
			cells_[x][y].setEastNeighbor((x == columns - 1) ? 0 : &cells_[x + 1][y]);
			cells_[x][y].setSouthNeighbor((y == 0) ? 0 : &cells_[x][y - 1]);
			cells_[x][y].setWestNeighbor((x == 0) ? 0 : &cells_[x - 1][y]);
		}
	}
	mvp_uniform_location_ = -1;
	translation_ = glm::vec3(0);
	num_vertices_ = 0;
	last_view_projection_ = glm::mat4(1);
}


InteractiveGrid::~InteractiveGrid() {
	for(GridInteraction* ia : interactions_) delete ia;
}


void InteractiveGrid::translate(float dx, float dy, float dz) {
	translation_ = glm::vec3(dx, dy, dz);
}


void InteractiveGrid::updateProjection(glm::mat4& p) {
	last_view_projection_ = p;
}


glm::vec2 InteractiveGrid::getNDC(glm::vec2 position) {
	// Apply grid translation
	glm::vec4 pos(position.x + translation_.x, position.y + translation_.y, 0.0f, 1.0f);
	// Apply camera projection
	pos = last_view_projection_ * pos;
	return glm::vec2(pos.x, pos.y) / pos.w;
}

GridCell* InteractiveGrid::getClosestWallCell(glm::vec2 pos) {
	//TODO: write
	return getCellAt(glm::vec2(0,0));
}


void InteractiveGrid::forEachCell(std::function<void(GridCell*)> callback) {
	for (std::vector<GridCell> &row : cells_) {
		for (GridCell &cell : row) {
			callback(&cell);
		}
	}
}


void InteractiveGrid::forEachCell(std::function<void(GridCell*,bool*)> callback) {
	bool found = false;
	for (std::vector<GridCell> &row : cells_) {
		for (GridCell &cell : row) {
			callback(&cell, &found);
			if (found) break;
		}
		if (found) break;
	}
}


void InteractiveGrid::forEachCellInRange(GridCell* leftLower, GridCell* rightUpper, std::function<void(GridCell*)> callback) {
	if (leftLower->getCol() > rightUpper->getCol() || leftLower->getRow() > rightUpper->getRow())
		return;
	for (size_t i = leftLower->getCol(); i <= rightUpper->getCol(); i++) {
		for (size_t j = leftLower->getRow(); j <= rightUpper->getRow(); j++) {
			callback(&cells_[i][j]);
		}
	}
}


void InteractiveGrid::forEachCellInRange(GridCell* leftLower, GridCell* rightUpper, std::function<void(GridCell*,bool*)> callback) {
	if (leftLower->getCol() > rightUpper->getCol() || leftLower->getRow() > rightUpper->getRow())
		return;
	bool found = false;
	for (size_t i = leftLower->getCol(); i <= rightUpper->getCol(); i++) {
		for (size_t j = leftLower->getRow(); j <= rightUpper->getRow(); j++) {
			callback(&cells_[i][j], &found);
			if (found) break;
		}
		if (found) break;
	}
}


bool InteractiveGrid::isInsideGrid(glm::vec2 positionNDC) {
	GridCell& leftUpperCell = cells_[0][cells_[0].size() - 1];
	glm::vec2 posLeftUpperNDC = getNDC(leftUpperCell.getPosition());
	if (positionNDC.x < posLeftUpperNDC.x || positionNDC.y > posLeftUpperNDC.y)
		return false;
	GridCell& rightLowerCell = cells_[cells_.size() - 1][0];
	glm::vec2 posRightLowerNDC = getNDC(glm::vec2(
		rightLowerCell.getXPosition() + cell_size_, rightLowerCell.getYPosition() - cell_size_ ));
	if (positionNDC.x > posRightLowerNDC.x || positionNDC.y < posRightLowerNDC.y)
		return false;
	return true;
}


bool InteractiveGrid::isInsideCell(glm::vec2 positionNDC, GridCell* cell) {
	glm::vec2 cellLeftUpperNDC = getNDC(cell->getPosition());
	glm::vec2 cellRightLowerNDC = getNDC(glm::vec2(
		cell->getXPosition() + cell_size_, cell->getYPosition() - cell_size_ ));
	if (positionNDC.x < cellLeftUpperNDC.x || positionNDC.y > cellLeftUpperNDC.y)
		return false;
	else if (positionNDC.x > cellRightLowerNDC.x || positionNDC.y < cellRightLowerNDC.y)
		return false;
	return true;
}


GridCell* InteractiveGrid::getCellAt(glm::vec2 positionNDC) {
	if (!isInsideGrid(positionNDC))
		return 0;
	size_t iLeftUpper = 0;
	size_t jLeftUpper = cells_[0].size() - 1;
	size_t iRightLower = cells_.size() - 1;
	size_t jRightLower = 0;
	while (iRightLower - iLeftUpper > 2 || jLeftUpper - jRightLower > 2) {
		size_t iMiddle = iLeftUpper + (iRightLower - iLeftUpper) / 2;
		size_t jMiddle = jRightLower + (jLeftUpper - jRightLower) / 2;
		glm::vec2 cellNDC = getNDC(cells_[iMiddle][jMiddle].getPosition());
		if (positionNDC.x < cellNDC.x)
			iRightLower = iMiddle;
		else
			iLeftUpper = iMiddle;
		if (positionNDC.y < cellNDC.y)
			jLeftUpper = jMiddle;
		else
			jRightLower = jMiddle;
	}
	for (size_t i = iLeftUpper; i <= iRightLower; i++) {
		for (size_t j = jRightLower; j <= jLeftUpper; j++) {
			if (isInsideCell(positionNDC, &cells_[i][j]))
				return &cells_[i][j];
		}
	}
	return &cells_[iLeftUpper][jLeftUpper];
}


GridCell* InteractiveGrid::getCellAt(size_t col, size_t row) {
	if (col >= cells_.size() || row >= cells_[0].size()) return 0;
	return &cells_[col][row];
}


void InteractiveGrid::uploadVertexData() {
	glGenVertexArrays(1, &vao_);
	glBindVertexArray(vao_);
	glGenBuffers(1, &vbo_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	size_t ncells = cells_.size() * cells_[0].size();
	size_t bytes_per_cell = GridCell::getVertexBytes();
	glBufferData(GL_ARRAY_BUFFER,
		ncells * bytes_per_cell,
		(void*)0,
		GL_STATIC_DRAW);
	GLintptr offset = 0;
	forEachCell([&](GridCell* cell) {
		cell->setVertexBufferOffset(offset);
		glBufferSubData(GL_ARRAY_BUFFER, offset,
			bytes_per_cell,
			cell->getVertexPointer());
		offset += bytes_per_cell;
	});
	GridCell::setVertexAttribPointer();
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	num_vertices_ = (GLsizei)ncells;
}


void InteractiveGrid::loadShader(viscom::GPUProgramManager mgr) {
	glEnable(GL_PROGRAM_POINT_SIZE);
	shader_ = mgr.GetResource("viewBuildStates",
		std::initializer_list<std::string>{ "viewBuildStates.vert", "viewBuildStates.frag" });
	mvp_uniform_location_ = shader_->getUniformLocation("MVP");
}


void InteractiveGrid::onFrame() {
	// Debug render
	glDisable(GL_DEPTH_TEST);
	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glUseProgram(shader_->getProgramId());
	last_view_projection_[3] += glm::vec4(translation_, 0);
	glUniformMatrix4fv(mvp_uniform_location_, 1, GL_FALSE, glm::value_ptr(last_view_projection_));
	glDrawArrays(GL_POINTS, 0, num_vertices_);
	glEnable(GL_DEPTH_TEST);
}


void InteractiveGrid::cleanup() {
	glDeleteBuffers(1, &vbo_);
	glDeleteVertexArrays(1, &vao_);
}

// TODO TEST What happens with concurrent input? Multitouch!?


void InteractiveGrid::onTouch(int touchID) {

}


void InteractiveGrid::onRelease(int touchID) {

}


void InteractiveGrid::onMouseMove(int touchID, double newx, double newy) {
	last_mouse_position_ = glm::dvec2(newx, newy);
}


void InteractiveGrid::buildAt(size_t col, size_t row, GridCell::BuildState buildState) {
	GridCell* maybeCell = getCellAt(col, row);
	if (!maybeCell) return;
	if (maybeCell->getBuildState() == buildState) return;
	maybeCell->updateBuildState(vbo_, buildState);
}


void InteractiveGrid::buildAtLastMousePosition(GridCell::BuildState buildState) {
	glm::vec2 touchPositionNDC =
		glm::vec2(last_mouse_position_.x, 1.0 - last_mouse_position_.y)
		* glm::vec2(2.0, 2.0) - glm::vec2(1.0, 1.0);
	GridCell* maybeCell = getCellAt(touchPositionNDC);
	if (!maybeCell) return;
	if (maybeCell->getBuildState() != GridCell::BuildState::EMPTY) return;
	if (maybeCell->getBuildState() == buildState) return;
	buildAt(maybeCell->getCol(), maybeCell->getRow(), buildState);
}


bool InteractiveGrid::isColumnEmptyBetween(size_t col, size_t startRow, size_t endRow) {
	if (endRow < startRow) {
		size_t tmp = endRow;
		endRow = startRow;
		startRow = tmp;
	}
	GridCell* maybeCell = 0;
	for (size_t i = startRow; i <= endRow; i++) {
		maybeCell = getCellAt(col, i);
		if (!maybeCell)
			return false;
		if (maybeCell->getBuildState() != GridCell::BuildState::EMPTY)
			return false;
	}
	return true;
}


bool InteractiveGrid::isRowEmptyBetween(size_t row, size_t startCol, size_t endCol) {
	if (endCol < startCol) {
		size_t tmp = endCol;
		endCol = startCol;
		startCol = tmp;
	}
	GridCell* maybeCell = 0;
	for (size_t j = startCol; j <= endCol; j++) {
		maybeCell = getCellAt(j, row);
		if (!maybeCell)
			return false;
		if (maybeCell->getBuildState() != GridCell::BuildState::EMPTY)
			return false;
	}
	return true;
}


float InteractiveGrid::getCellSize() {
	return cell_size_;
}


size_t InteractiveGrid::getNumColumns() {
	return cells_.size();
}


size_t InteractiveGrid::getNumRows() {
	return cells_[0].size();
}


size_t InteractiveGrid::getNumCells() {
	return cells_.size() * cells_[0].size();
}