#pragma once
#include "app/roomgame/IUpdateable.h"
#include <random>

namespace roomgame {
    class InteractiveGrid;
	class OuterInfluence : public roomgame::IUpdateable
	{
	public:
		OuterInfluence(std::shared_ptr<SourceLightManager> sourceLightManager);
	    virtual ~OuterInfluence();

		// Geerbt über IUpdateable
	    void Update(double deltaTime) override;
	    void UpdateSlow(double deltaTime) override;

        // Modify the outer influence on the fly
        int getMaxPatrolTime();
        void setMaxPatrolTime(int newChance);
        int getMinPatrolTime();
        void setMinPatrolTime(int newChance);
        int getCurrentPatrolTime() { return currentPatrolTime_; }
        int getPatrolTime() { return patrolTime_; }

        float getBaseSpeed();
        void setBaseSpeed(float speed);

        void resetValues();

        glm::mat4 ViewPersMat;
		SynchronizedGameMesh* MeshComponent;
		std::shared_ptr<InteractiveGrid> Grid;
	private:
        std::shared_ptr<SourceLightManager> sourceLightManager_;
        float movementType_ = 0; // 0 is patrolling movement, 1 is attacking movement
        float changeSpeed_ = 1.f; // How fast the influence changes its movement pattern
        float speed_;
        float distance_;
        float baseSpeed_;
        int currentPatrolTime_;
        int patrolTime_;
        int minPatrolTime_;
        int maxPatrolTime_;
        std::default_random_engine rndGenerator_;
        std::uniform_int_distribution<int> distributor100_;
        std::uniform_int_distribution<int> distributor200_;
        GridCell* targetCell_;
		int mode_;
		double deltaTime_;
		float actionStatus_;
		glm::vec3 oldPosition_;
		glm::vec3 targetPosition_;
		glm::vec3 posDiff_;
		void CalcPositions(bool init);
		void CheckForPatrolEnd();
	    void EngageInNewRandomPatrol();
	    void Attack();
		void Retreat();
		void Move() const;
        void ChooseTarget();
	};
}
