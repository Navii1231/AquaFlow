#pragma once
#include "GraphConfig.h"
#include "Graph.h"

AQUA_BEGIN
EXEC_BEGIN

class Executioner
{
public:
	Executioner(vk::CommandBuffer cmd, const Operation& op)
		: mCmds(cmd), mOp(op)
	{
		mCmds.reset();
		mCmds.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	}

	~Executioner()
	{
		mCmds.end();
	}

private:
	vk::CommandBuffer mCmds;
	const Operation& mOp;
};

EXEC_END
AQUA_END

