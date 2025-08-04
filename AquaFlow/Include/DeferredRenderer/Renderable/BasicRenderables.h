#pragma once
#include "FactoryConfig.h"

AQUA_BEGIN

struct Point
{
	glm::vec4 Position{};
	glm::vec4 Color{ 1.0f, 0.0f, 1.0f, 1.0f };
};

struct Line
{
	Point Begin;
	Point End;
	float Thickness = 1.0f;
};

using Plane = std::array<Point, 4>;

struct Curve
{
	std::vector<glm::vec4> Points;
	glm::vec4 Color{ 1.0f, 0.0f, 1.0f, 1.0f };
	float Thickness = 1.0f;
};

AQUA_END
