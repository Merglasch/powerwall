#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "app\roomgame\IUpdateable.h"

namespace roomgame {
	class UpdateManager
	{
	public:
		UpdateManager();
		~UpdateManager();
		void ManageUpdates(double deltaTime);
		void AddUpdateable(std::shared_ptr<IUpdateable> obj);
		void RemoveUpdateable(std::shared_ptr<IUpdateable> obj);
	private:
		std::vector<std::shared_ptr<IUpdateable>> updateables;
		double timer;
		double max_time;
	};
}
