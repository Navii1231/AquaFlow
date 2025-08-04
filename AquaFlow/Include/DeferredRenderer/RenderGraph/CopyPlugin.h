#pragma once
#include "RenderPlugin.h"

AQUA_BEGIN

class CopyPlugin : public RenderPlugin
{
public:
	CopyPlugin() = default;
	~CopyPlugin() = default;

	void SetSrcImageView(vkLib::ImageView view) { mSrc = view; }
	void SetDstImageView(vkLib::ImageView view) { mDst = view; }

	void SetBlitInfo(const vkLib::ImageBlitInfo& blitInfo) { mBlitInfo = blitInfo; }

	virtual void AddPlugin(EXEC_NAMESPACE::GraphBuilder& graph, const std::string& name) override
	{
		vkLib::ImageView src = mSrc, dst = mDst;
		vkLib::ImageBlitInfo blitInfo = mBlitInfo;

		graph[name] = CreateOp(name, EXEC_NAMESPACE::OpType::eCopyOrTransfer);

		graph[name].Fn = [src, dst, blitInfo](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
			{
				EXEC_NAMESPACE::Executioner exec(cmd, op);

				dst->BeginCommands(cmd);

				dst->RecordBlit(*src, blitInfo);

				dst->EndCommands();
			};
	}

private:
	vkLib::ImageView mSrc;
	vkLib::ImageView mDst;

	vkLib::ImageBlitInfo mBlitInfo;
};

AQUA_END
