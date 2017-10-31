#include "UpdateManager.h"
#include <iostream>

namespace roomgame {

	UpdateManager::UpdateManager()
	{
		timer = 0;
		max_time = 1;
	}


	UpdateManager::~UpdateManager()
	{
		updateables.clear();
	}

	void UpdateManager::ManageUpdates(double deltaTime)
	{
		if (timer > max_time) {
            for (std::shared_ptr<IUpdateable> upd : updateables)
			{
				upd.get()->Update(deltaTime);
				upd.get()->UpdateSlow(deltaTime);
			}
			timer = 0;
		}
		else {
			for (std::shared_ptr<IUpdateable> upd : updateables)
			{
				upd.get()->Update(deltaTime);
			}
		}
		timer += deltaTime;
	}

	void UpdateManager::AddUpdateable(std::shared_ptr<IUpdateable> obj)
	{
		updateables.push_back(obj);
	}

	void UpdateManager::RemoveUpdateable(std::shared_ptr<IUpdateable> obj)
	{
		updateables.erase(std::remove(updateables.begin(), updateables.end(), obj), updateables.end());
	}

}
