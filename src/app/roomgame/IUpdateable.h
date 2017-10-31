#pragma once
namespace roomgame {
	__interface IUpdateable
	{
	public:
		void Update(double deltaTime) = 0;
		void UpdateSlow(double deltaTime) = 0;
	};
}
