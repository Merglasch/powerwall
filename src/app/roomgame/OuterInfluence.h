#pragma once
#include "app\roomgame\IUpdateable.h"
namespace roomgame {
	class OuterInfluence : public roomgame::IUpdateable
	{
	public:
		OuterInfluence();
		~OuterInfluence();

		// Geerbt �ber IUpdateable
		virtual void Update() override;
		virtual void UpdateSlow() override;
	};
}
