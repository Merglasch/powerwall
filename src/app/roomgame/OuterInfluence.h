#pragma once
#include "app\roomgame\IUpdateable.h"
#include "app\roomgame\RoomInteractiveGrid.h"
#include "glm\gtx\transform.hpp"
#include <random>
#include <ctime>

namespace roomgame {
	class OuterInfluence : public roomgame::IUpdateable
	{
	public:
		OuterInfluence();
		~OuterInfluence();

		// Geerbt �ber IUpdateable
		virtual void Update(double deltaTime) override;
		virtual void UpdateSlow(double deltaTime) override;

        glm::mat4 viewPersMat;
		SynchronizedGameMesh* meshComponent;
		RoomInteractiveGrid* grid;
	private:
        float movementType = 0; // 0 is patrolling movement, 1 is attacking movement
        float ChangeSpeed = 1.25f; // How fast the influence changes its movement pattern
        float speed;
        float distance;
        int attackChance;
        int attackChanceGrowth;
        std::default_random_engine rndGenerator;
        std::uniform_int_distribution<int> distributor100;
        GridCell* targetCell;
		int mode;
		double deltaTime;
		float actionStatus;
		glm::vec3 oldPosition;
		glm::vec3 targetPosition;
		glm::vec3 posDiff;
		void DecideNextAction();
		void calcPositions(bool init);
		void Patrol();
		void Attack();
		void Retreat();
		void Move();
        void ChooseTarget();
	};
}
