#pragma once
#include "Core/AqCore.h"

class LayoutTransitioner
{
public:
	LayoutTransitioner(vkLib::Image image, vk::ImageLayout transition, vk::ImageLayout finalLay = vk::ImageLayout::eUndefined)
		: mImage(image)
	{
		mFinal = finalLay == vk::ImageLayout::eUndefined ? mImage.GetConfig().CurrLayout : finalLay;

		mImage.TransitionLayout(transition);
	}

	~LayoutTransitioner()
	{
		mImage.TransitionLayout(mFinal);
	}

private:
	vkLib::Image mImage;

	vk::ImageLayout mFinal;
};
