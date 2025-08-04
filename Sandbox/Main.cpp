#include "ComputeTester/ComputePipelineTester.h"
#include "DeferredRendererTester/DeferredRendererTester.h"

enum class TestingFlagBits
{
	eGraphicsPipeline    = 0,
	eComputePipeline     = 1,
	eDeferredPipeline    = 2,
};

using TestingFlags = vk::Flags<TestingFlagBits>;

void TestGraphicsPipeline()
{
	WindowProps props{};
	props.width = 1600;
	props.height = 900;
	props.name = "Learning it the hard way";
	props.vSync = false;

	ApplicationCreateInfo appInfo{};
	appInfo.AppName = "Vulkan Engine Tester";
	appInfo.EngineName = "vkLib";
	appInfo.WindowInfo = props;

	appInfo.EnableValidationLayers = true;

	{
		//std::unique_ptr<Application> App = std::make_unique<GraphicsPipelineTester>(appInfo);
		//App->Run();
	}
}

void TestComputePipeline()
{
	WindowProps props{};
	props.width = 1600;
	props.height = 900;
	props.name = "Learning it the hard way";
	props.vSync = false;

	ApplicationCreateInfo appInfo{};
	appInfo.AppName = "Vulkan Engine Tester";
	appInfo.EngineName = "vkLib";
	appInfo.WindowInfo = props;
	appInfo.EnableValidationLayers = true;

	{
		std::unique_ptr<Application> App = std::make_unique<ComputePipelineTester>(appInfo);
		App->Run();
	}
}

void TestDeferredPipeline()
{
	WindowProps props{};
	props.width = 1600;
	props.height = 900;
	props.name = "Learning it the hard way";
	props.vSync = false;

	ApplicationCreateInfo appInfo{};
	appInfo.AppName = "Vulkan Engine Tester";
	appInfo.EngineName = "vkLib";
	appInfo.WindowInfo = props;

#if _DEBUG
	appInfo.EnableValidationLayers = true;
#endif
	{
		std::unique_ptr<Application> App = std::make_unique<DeferredRendererTester>(appInfo);
		App->Run();
	}
}

struct TestingFunction
{
	std::function<void()> mFunction;
	TestingFlagBits mFlag;
};

int main()
{
	std::vector<TestingFunction> testingFuncs(3);
	testingFuncs.push_back({ TestGraphicsPipeline, TestingFlagBits::eGraphicsPipeline });
	testingFuncs.push_back({ TestComputePipeline, TestingFlagBits::eComputePipeline });
	testingFuncs.push_back({ TestDeferredPipeline, TestingFlagBits::eDeferredPipeline });

	TestingFlags flags;
	flags |= TestingFlagBits::eDeferredPipeline;

	{
		std::vector<std::thread> testingThreads;

		for (auto& func : testingFuncs)
		{
			if(flags & func.mFlag)
				testingThreads.emplace_back(func.mFunction);
		}

		for(auto& thread : testingThreads)
			thread.join();
	}

	return 0;
}
