#include "app/roomgame/InteractiveGrid.h"
#include "RoomInteractionManager.h"
#include "MeshInstanceBuilder.h"
#include "OuterInfluence.h"
#include <iostream>
namespace roomgame {
	const int PATROL = 0;
	const int ATTACK = 1;
	const int RETREAT = 2;
    const float DEFAULT_BASE_SPEED = 0.5f;
    const float ROT_SPEED_MULTIPLIER = 50.0f;
    const int DEFAULT_MIN_PATROL_TIME = 2;
    const int DEFAULT_MAX_PATROL_TIME = 11;

	OuterInfluence::OuterInfluence(std::shared_ptr<SourceLightManager> sourceLightManager): MeshComponent(nullptr), distance_(0), targetCell_(nullptr), deltaTime_(0)
    {
        sourceLightManager_ = sourceLightManager;
        Grid = nullptr;
        mode_ = 0;
        actionStatus_ = 0;
        oldPosition_ = glm::vec3(0, 0, 0);
        targetPosition_ = glm::vec3(2, 0, 0);
        posDiff_ = glm::vec3(0);
        ViewPersMat = glm::mat4(1);
        baseSpeed_ = DEFAULT_BASE_SPEED;
        speed_ = DEFAULT_BASE_SPEED;
        const auto seed1 = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
        rndGenerator_ = std::default_random_engine(seed1);
        minPatrolTime_ = DEFAULT_MIN_PATROL_TIME;
        maxPatrolTime_ = DEFAULT_MAX_PATROL_TIME;
        distributor100_ = std::uniform_int_distribution<int>(minPatrolTime_, maxPatrolTime_);
        distributor200_ = std::uniform_int_distribution<int>(0, 200);
    }


    OuterInfluence::~OuterInfluence()
	{
	}

	void OuterInfluence::CalcPositions(bool init = false) {
		for (auto i = 0; i < 5; i++) {
		    const auto tmpI = i + 0.1f;
		    const auto transPat = glm::translate(glm::vec3(sinf(tmpI*static_cast<float>(glfwGetTime())), cosf(tmpI*static_cast<float>(glfwGetTime()))*0.5f, cosf(tmpI*static_cast<float>(glfwGetTime()))*0.5f));
		    const auto transAtt = glm::translate(glm::vec3(cosf(tmpI*static_cast<float>(glfwGetTime()))*0.05f, sinf(tmpI*static_cast<float>(glfwGetTime()))*0.1, cosf(tmpI*static_cast<float>(glfwGetTime()))*0.05f));
		    const auto translation = glm::mix(transPat, transAtt, movementType_);
            if (i<MeshComponent->influencePositions_.size()) {
                MeshComponent->influencePositions_[i] = MeshComponent->model_matrix_*translation;
            }
            else {
				MeshComponent->influencePositions_.push_back(MeshComponent->model_matrix_*translation);
            }
		}
        if (mode_ == ATTACK) {
            movementType_ = min(movementType_ + changeSpeed_*(float)deltaTime_, 1);
            speed_ = min(exp(speed_ + changeSpeed_*0.2f*(float)deltaTime_)-1.0f, baseSpeed_ + 0.3f);
        }
        else if (mode_ == RETREAT) {
            movementType_ = max(movementType_ - changeSpeed_*(float)deltaTime_, 0);
            speed_ = min(speed_ + changeSpeed_*0.1f*(float)deltaTime_, baseSpeed_ - 0.1f);
        }else
        {
            speed_ = min(speed_ + changeSpeed_*0.1f*(float)deltaTime_, baseSpeed_ - 0.1f);
        }
	}

	void OuterInfluence::Update(double deltaTime)
	{
		this->deltaTime_ = deltaTime;
		Move();
		CalcPositions();
        if (mode_ == ATTACK) {
            Attack();
        }
        else if (mode_ == RETREAT) {
            Retreat();
        }
	}

	//Change Position
	void OuterInfluence::Move() const
	{
        if(mode_==PATROL){
            auto rotateMat = translate(glm::vec3(-30, 0, 0));
            rotateMat *= rotate(glm::radians(speed_*ROT_SPEED_MULTIPLIER*static_cast<float>(deltaTime_)), glm::vec3(0, 0, 1));
            rotateMat *= translate(glm::vec3(30, 0, 0));
            MeshComponent->transform(rotateMat);
        }
        else{
            auto targetMovMat = inverse(MeshComponent->model_matrix_);
            targetMovMat *= translate((posDiff_)*(speed_*static_cast<float>(deltaTime_)));
            targetMovMat *= MeshComponent->model_matrix_;
            MeshComponent->transform(targetMovMat);
        }
	}

	void OuterInfluence::UpdateSlow(double deltaTime)
	{
        //If patrolling, check if Attack should be initiated
        if(mode_==PATROL)
        {
            CheckForPatrolEnd();
        }
    }

    int OuterInfluence::getMaxPatrolTime()
    {
        return maxPatrolTime_;
    }

    void OuterInfluence::setMaxPatrolTime(int newChance)
    {
        glm::clamp(newChance, minPatrolTime_+1, 30);
        maxPatrolTime_ = newChance;
        distributor100_ = std::uniform_int_distribution<int>(minPatrolTime_, maxPatrolTime_);
    }

    int OuterInfluence::getMinPatrolTime()
    {
        return minPatrolTime_;
    }

    void OuterInfluence::setMinPatrolTime(int newChance)
    {
        glm::clamp(newChance, 2, maxPatrolTime_-1);
        minPatrolTime_ = newChance;
        distributor100_ = std::uniform_int_distribution<int>(minPatrolTime_, maxPatrolTime_);
    }

    float OuterInfluence::getBaseSpeed()
    {
        return baseSpeed_;
    }

    void OuterInfluence::setBaseSpeed(float speed)
    {
        baseSpeed_ = glm::clamp(speed, 0.2f, 1.0f);
    }

    void OuterInfluence::resetValues()
    {
        minPatrolTime_ = DEFAULT_MIN_PATROL_TIME;
        maxPatrolTime_ = DEFAULT_MAX_PATROL_TIME;
        baseSpeed_ = DEFAULT_BASE_SPEED;
    }

	void OuterInfluence::CheckForPatrolEnd() {
        if (currentPatrolTime_ >= patrolTime_) {
            currentPatrolTime_ = 0;
            oldPosition_ = glm::vec3(MeshComponent->model_matrix_[3][0], MeshComponent->model_matrix_[3][1], MeshComponent->model_matrix_[3][2]);
			mode_ = ATTACK;
            ChooseTarget();
        }
        else {
            currentPatrolTime_++;
        }
	}

    void OuterInfluence::EngageInNewRandomPatrol()
    {
        mode_ = PATROL;
        const auto randNumber = distributor100_(rndGenerator_);
        patrolTime_ = randNumber;
    }

    void OuterInfluence::ChooseTarget() {
        //Set the closest cell with wall build state as moveTarget (move towards it until collison)
        auto ndcCoords = glm::vec4(MeshComponent->model_matrix_[3][0], MeshComponent->model_matrix_[3][1], 0.0f, 1.0f);
        ndcCoords = ViewPersMat * ndcCoords;
        ndcCoords = ndcCoords / ndcCoords.w;
        ndcCoords = glm::vec4(Grid->pushNDCinsideGrid(glm::vec2(ndcCoords.x, ndcCoords.y)), ndcCoords.z, ndcCoords.w);
        auto tmp = Grid->getCellAt(glm::vec2(ndcCoords.x, ndcCoords.y));
        if (tmp == nullptr) {
            std::cout << "nullptr when choosing cell where Outer Influence is positioned" << std::endl;
            return;
        }
        auto cellDistance = 9999.0f;
        GridCell* closestWallCell = nullptr;
        const auto leftLower = Grid->getCellAt(0,0);
        const auto rightUpper = Grid->getCellAt(Grid->getNumRows()-1,Grid->getNumColumns()-1);
        std::vector<GridCell*> possibleTargets;
        Grid->forEachCellInRange(leftLower, rightUpper, static_cast<std::function<void(GridCell*)>>([&](GridCell* cell) {
            if ((cell->getBuildState() & GridCell::WALL) != 0  && (cell->getBuildState() & (GridCell::TEMPORARY | GridCell::SOURCE)) == 0) {
                possibleTargets.push_back(cell);
            }
        }));
        std::sort(possibleTargets.begin(),possibleTargets.end(),[&](GridCell* a, GridCell* b)
        {
            float distA = a->getDistanceTo(tmp);
            float distB = b->getDistanceTo(tmp);
            return distA < distB;
        });
        auto randNumber = distributor200_(rndGenerator_);
        randNumber = glm::clamp(randNumber,0,max(static_cast<int>(possibleTargets.size()-1),0));
        if(possibleTargets.size() > 0)
        {
            closestWallCell = possibleTargets[randNumber];
        }
        if (closestWallCell != nullptr) {
            speed_ = 0;
            targetPosition_ = glm::vec3(closestWallCell->getXPosition(), closestWallCell->getYPosition(), 0);
            targetPosition_ += Grid->getTranslation();
            posDiff_ = targetPosition_ - glm::vec3(MeshComponent->model_matrix_[3][0], MeshComponent->model_matrix_[3][1], 0.0f);
            distance_ = glm::length(posDiff_);
            targetCell_ = closestWallCell;
        }
        else {
            EngageInNewRandomPatrol();
        }
    }

	void OuterInfluence::Attack() {
        //If the target Cell is reached, change to retreat mode_ and mark the cell as source
	    const auto currentPos = glm::vec3(MeshComponent->model_matrix_[3][0], MeshComponent->model_matrix_[3][1], MeshComponent->model_matrix_[3][2]);
	    const auto dist = glm::distance(oldPosition_, currentPos);
        if (dist>distance_) {
            speed_ = 0;
            targetPosition_ = oldPosition_;
            oldPosition_ = currentPos;
            posDiff_ = targetPosition_ - currentPos;
            distance_ = glm::length(posDiff_);
            mode_ = RETREAT;
            auto bs = targetCell_->getBuildState();
            if (bs == GridCell::EMPTY || bs & GridCell::INSIDE_ROOM) return;
            targetCell_->updateHealthPoints(Grid->vbo_, GridCell::MIN_HEALTH);
            Grid->roomInteractionManager_->meshInstanceBuilder_->buildAt(targetCell_->getCol(), targetCell_->getRow(), GridCell::SOURCE, MeshInstanceBuilder::BuildMode::Additive);
            Grid->roomInteractionManager_->meshInstanceBuilder_->buildAt(targetCell_->getCol(), targetCell_->getRow(), GridCell::WALL, MeshInstanceBuilder::BuildMode::RemoveSpecific);
            const auto wPos = Grid->getWorldCoordinates(targetCell_->getPosition());
            if(Grid->roomInteractionManager_->sourceLightManager_==nullptr)
            {
                Grid->roomInteractionManager_->sourceLightManager_ = sourceLightManager_;
            }
            sourceLightManager_->sourcePositions_.push_back(wPos);
        }
	}

	void OuterInfluence::Retreat() {
		//If back at original position, change to patrol mode_
	    const auto currentPos = glm::vec3(MeshComponent->model_matrix_[3][0], MeshComponent->model_matrix_[3][1], MeshComponent->model_matrix_[3][2]);
	    const auto dist = glm::distance(oldPosition_, currentPos);
        if (dist>distance_) {
            speed_ = 0;
            EngageInNewRandomPatrol();
        }
    }
}
