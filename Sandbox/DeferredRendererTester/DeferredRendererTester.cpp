#include "DeferredRendererTester.h"
#include "Geometry3D/MeshLoader.h"

#include "Utils/CompilerErrorChecker.h"

#include "LayoutTransitioner.h"

constexpr glm::vec2 sScrSize = { 2048, 2048 };

DeferredRendererTester::DeferredRendererTester(const ApplicationCreateInfo& createInfo)
	: Application(createInfo), mRenderableManager(*mContext)
{
	CreateResources();
	PrepareSyncStuff();
	PrepareFramebufferFactory();
	PrepareMaterials();
	PrepareRenderer();
}

DeferredRendererTester::~DeferredRendererTester()
{
	mContext->WaitIdle();
	mCmdAlloc.Free(mDisplayCommandBuffer);
}

bool DeferredRendererTester::OnStart()
{
	glm::mat4 moved = glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 0.0f, 0.0f));

	mRenderableManager.LoadModel("Sphere", moved);
	mRenderableManager.LoadModel("Cube", glm::translate(glm::mat4(1.0), glm::vec3(0.0f, 0.0f, 5.0f)));
	mRenderableManager.LoadModel("WineGlass", glm::mat4(1.0f));
	mRenderableManager.LoadModel("Cup", glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 0.0f, 0.0f)));
	mRenderableManager.LoadModel("Plane", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -5.0f, 0.0f)));

	mRenderer.SubmitRenderable("Plane",
		mRenderableManager.GetModelMatrices()[mRenderableManager["Plane"][0].VertexData.ModelIdx],
		mRenderableManager["Plane"][0], mMaterialInstances[0]);
	
	mRenderer.SubmitRenderable("WineGlass",
		mRenderableManager.GetModelMatrices()[mRenderableManager["WineGlass"][0].VertexData.ModelIdx],
		mRenderableManager["WineGlass"][0], mMaterialInstances[0]);

	AquaFlow::Line line{};
	line.Begin.Position = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	line.End.Position = glm::vec4(0.0f, 0.0f, 10.0f, 1.0f);
	line.Thickness = 1.0f;

	AquaFlow::Curve curve{};
	curve.Points.emplace_back(0.0f, 0.0f, 0.0f, 1.0f);
	curve.Points.emplace_back(0.0f, 2.0f, 3.0f, 1.0f);
	curve.Points.emplace_back(0.0f, 4.0f, 2.0f, 1.0f);
	curve.Points.emplace_back(5.0f, -5.0f, 7.0f, 1.0f);

	mRenderer.SubmitLines("Line", line);
	//mRenderer.SubmitBezierCurves("Curve", curve);

	mRenderer.PrepareMaterialNetwork();

	mEditorCamera.SetLinearSpeed(1.0f);

	mRenderer.ActivateRenderables({ "Plane", "WineGlass", "Line" });
	//mRenderer.ActivateRenderables({ "Plane", "WineGlass" });
	//mRenderer.ActivateRenderables({ "Line", "Curve" });
	//mRenderer.ActivateRenderables({ "Line" });

	mDisplayImageSignal = mContext->CreateSemaphore();

	mRenderer.SetCamera(mEditorCamera.GetProjectionMatrix(), mEditorCamera.GetViewMatrix());
	mRenderer.UploadRenderables();
	mRenderer.UpdateDescriptors();

	mRenderer.InsertPostEventDependency(mDisplayImageSignal);

	return true;
}

bool DeferredRendererTester::OnUpdate(float elaspedTime)
{
	// Reset all the fences before continuing
	mExec.WaitIdle();

	UpdateCamera(elaspedTime);

	AquaFlow::CameraInfo cameraInfo = mEnv->GetDirCameras()[0];

	// these functions will run per frame
	mRenderer.SetCamera(mEditorCamera.GetProjectionMatrix(), mEditorCamera.GetViewMatrix());
	//mRenderer.SetCamera(cameraInfo.Projection, cameraInfo.View);
	mRenderer.IssueDrawCall();

	//auto displayImage = probeFramebuffer.GetColorAttachments().front();

	auto displayImage = mRenderer.GetPostprocessbuffer().GetColorAttachments()[0];
	//auto displayImage = mRenderer.GetDepthView();
	//auto displayImage = mRenderer.GetShadingbuffer().GetColorAttachments()[0];
	//auto displayImage = mRenderer.GetGBuffer().GetColorAttachments()[0];

	//static auto displayImage = mImageLoader.CreateImageBuffer(
	//	"C:\\Users\\Navjot Singh\\Desktop\\Blender Masterpieces\\Pink Mug 2.png");

	DisplayOnScreen(*displayImage);

	return true;
}

void DeferredRendererTester::DisplayOnScreen(vkLib::Image image)
{
	auto frameIdx = mSwapchain->AcquireNextFrame(mImageAcquired);

	_STL_ASSERT(frameIdx.result == vk::Result::eSuccess, "Failed to acquire a swapchain image");

	auto frame = mSwapchain->GetSwapchainData().Frames[frameIdx.value];

	auto targetImage = frame->RenderTarget.GetColorAttachments()[0];

	targetImage->TransitionLayout(vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits::eTopOfPipe);

	vkLib::ImageBlitInfo blitInfo{};
	blitInfo.Filter = vk::Filter::eLinear;

	if(image.GetConfig().Format == vk::Format::eD24UnormS8Uint)
		blitInfo.Filter = vk::Filter::eNearest;

	mDisplayCommandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	targetImage->BeginCommands(mDisplayCommandBuffer);
	targetImage->RecordBlit(image, blitInfo);
	targetImage->EndCommands();

	mDisplayCommandBuffer.end();

	vk::PipelineStageFlags waitMasks[2] = { vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe };
	std::vector<vk::Semaphore> waitPoints({ *mImageAcquired, *mDisplayImageSignal });

	vk::SubmitInfo copyImages{};
	copyImages.setCommandBuffers(mDisplayCommandBuffer);
	copyImages.setSignalSemaphores(frame->ImageRendered);
	copyImages.setWaitSemaphores(waitPoints);
	copyImages.setWaitDstStageMask(waitMasks);

	mExec[6]->Submit(copyImages);

	vk::PresentInfoKHR presentInfo{};

	uint32_t swapchainImageIndices[1] = { frameIdx.value };

	presentInfo.setImageIndices(swapchainImageIndices);
	presentInfo.setSwapchains(*mSwapchain->GetHandle());
	presentInfo.setWaitSemaphores(frame->ImageRendered);

	mExec[7]->PresentKHR(presentInfo);
}

void DeferredRendererTester::CreateResources()
{
	mResourcePool = mContext->CreateResourcePool();
	mCmdAlloc = mContext->CreateCommandPools()[0];

	mExec = mContext->FetchExecutor(0, vkLib::QueueAccessType::eGeneric);

	mMaterials = mResourcePool.CreateBuffer<AquaFlow::PBRMaterial>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
	mDirLightSrcs = mResourcePool.CreateBuffer<AquaFlow::DirectionalLightSrc>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mCameraBuf = mResourcePool.CreateBuffer<AquaFlow::CameraInfo>(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
	mLightCameras = mResourcePool.CreateBuffer<AquaFlow::CameraInfo>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mSpareSampler = mResourcePool.CreateSampler({});

	mImageAcquired = mContext->CreateSemaphore();
	mDisplayCommandBuffer = mCmdAlloc.Allocate();

	mImageLoader.SetResourcePool(mResourcePool);

	mLightCameras.Resize(1);
	mCameraBuf.Resize(1);
}

void DeferredRendererTester::UpdateCamera(float elaspedTime)
{
	AquaFlow::CameraMovementFlags cameraMovement{};

	if (glfwGetKey(mWindow->GetNativeHandle(), GLFW_KEY_W))
		cameraMovement.SetFlag(AquaFlow::CameraMovement::eForward);
	if (glfwGetKey(mWindow->GetNativeHandle(), GLFW_KEY_S))
		cameraMovement.SetFlag(AquaFlow::CameraMovement::eBackward);
	if (glfwGetKey(mWindow->GetNativeHandle(), GLFW_KEY_A))
		cameraMovement.SetFlag(AquaFlow::CameraMovement::eLeft);
	if (glfwGetKey(mWindow->GetNativeHandle(), GLFW_KEY_D))
		cameraMovement.SetFlag(AquaFlow::CameraMovement::eRight);
	if (glfwGetKey(mWindow->GetNativeHandle(), GLFW_KEY_Q))
		cameraMovement.SetFlag(AquaFlow::CameraMovement::eDown);
	if (glfwGetKey(mWindow->GetNativeHandle(), GLFW_KEY_E))
		cameraMovement.SetFlag(AquaFlow::CameraMovement::eUp);

	bool MouseButtonDown = glfwGetMouseButton(mWindow->GetNativeHandle(), GLFW_MOUSE_BUTTON_1);

	std::chrono::nanoseconds timeElasped((uint32_t)(elaspedTime * AquaFlow::EditorCamera::NanoSecondsTicksInOneSecond()));

	mEditorCamera.OnUpdate(timeElasped, cameraMovement, mWindow->GetCursorPosition(), MouseButtonDown);

	mCameraBuf.Clear();
	mCameraBuf << AquaFlow::CameraInfo(mEditorCamera.GetProjectionMatrix(), mEditorCamera.GetViewMatrix());

	//mLightCamera.Clear();
	//mLightCamera << AquaFlow::CameraInfo(mEditorCamera.GetProjectionMatrix(), mEditorCamera.GetViewMatrix());
}

void DeferredRendererTester::PrepareFramebufferFactory()
{
	mFramebufferFactory.SetContextBuilder(mContext->FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));

	// We're gonna be using HDR
	mFramebufferFactory.AddColorAttribute(ENTRY_POSITION, "RGBA32F");
	mFramebufferFactory.SetDepthAttribute("DEPTH", "D24UN_S8U");

	mFramebufferFactory.SetAllColorProperties(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
		vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
	mFramebufferFactory.SetDepthProperties(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);

	auto error = mFramebufferFactory.Validate();
}

void DeferredRendererTester::PrepareRenderer()
{
	AquaFlow::ShadowCascadeFeature shadowFeature{};
	shadowFeature.BaseResolution = { 2048, 2048 };
	shadowFeature.CascadeDepth = 500.0f;
	shadowFeature.CascadeDivisions = 10;

	mEnv = std::make_shared<AquaFlow::Environment>();

	AquaFlow::DirectionalLightInfo lightSrc{};
	lightSrc.SrcInfo.Color = { 10.0f, 10.0f, 10.0f, 10.0f };
	lightSrc.SrcInfo.Direction = { 1.0f, -1.0f, 1.0f, 1.0f };

	lightSrc.CubeSize = glm::vec3(25.0f, 25.0f, 25.0f);
	lightSrc.Position = glm::vec3(0.0f, -0.0f, 0.0f);

	auto SkyboxImage = mImageLoader.CreateImageBuffer("C:\\Users\\Navjot Singh\\Desktop\\CubeMaps\\Eq11.jpg");

	mEnv->SubmitLightSrc(lightSrc);
	mEnv->SubmitSkybox(SkyboxImage.GetIdentityImageView());
	mEnv->SetSkyboxSampler(mSpareSampler);

	mRenderer.SetCtx(*mContext);
	mRenderer.SetEnvironment(mEnv);
	mRenderer.SetShadowConfig(shadowFeature);
	mRenderer.EnableFeatures(AquaFlow::RenderingFeature::eShadow);

	mFramebufferFactory.SetTargetSize({ 2048, 2048 });

	mRenderer.SetShadingbuffer(*mFramebufferFactory.CreateFramebuffer());

	mRenderer.PrepareFeatures();
}

void DeferredRendererTester::PrepareSyncStuff()
{
	//mDeferFinishedSignal = mContext->CreateSemaphore();
	//mShadingFinishedSignal = mContext->CreateSemaphore();

	mDisplayImageSignal = mContext->CreateSemaphore();
}

void DeferredRendererTester::PrepareMaterials()
{
	AquaFlow::MatCore::MaterialAssembler assembler{};
	assembler.SetPipelineBuilder(mContext->MakePipelineBuilder());

	std::string frontEndString, backEndString, bsdfModuleString, bsdfUtility;

	vkLib::ReadFile(mShaderDir + "FrontEnd.glsl", frontEndString);
	vkLib::ReadFile(mShaderDir + "BackEnd.glsl", backEndString);
	vkLib::ReadFile(mShaderDir + "CookTorranceBSDF.glsl", bsdfModuleString);
	vkLib::ReadFile(mShaderDir + "CommonBSDFs.glsl", bsdfUtility);

	frontEndString += bsdfUtility;

	mMaterialSystem.SetResourcePool(mResourcePool);
	mMaterialSystem.SetAssembler(assembler);
	mMaterialSystem.AddImport("CookTorranceBSDF", bsdfModuleString);
	mMaterialSystem.SetFrontEndView(frontEndString);
	mMaterialSystem.SetBackEndView(backEndString);

	AquaFlow::DeferGFXMaterialCreateInfo materialInfo{};
	materialInfo.GFXConfig.CanvasScissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(2048, 2048));
	materialInfo.GFXConfig.CanvasView = vk::Viewport(0.0f, 0.0f, 2048, 2048, 0.0f, 1.0f);

	materialInfo.GFXConfig.DepthBufferingState.DepthBoundsTestEnable = false;
	materialInfo.GFXConfig.DepthBufferingState.DepthCompareOp = vk::CompareOp::eLess;
	materialInfo.GFXConfig.DepthBufferingState.DepthTestEnable = false;
	materialInfo.GFXConfig.DepthBufferingState.DepthWriteEnable = false;

	materialInfo.GFXConfig.DynamicStates.push_back(vk::DynamicState::eScissor);
	materialInfo.GFXConfig.DynamicStates.push_back(vk::DynamicState::eViewport);
	materialInfo.GFXConfig.DynamicStates.push_back(vk::DynamicState::eFrontFace);
	materialInfo.GFXConfig.DynamicStates.push_back(vk::DynamicState::eDepthTestEnable);

	materialInfo.GFXConfig.TargetContext = mFramebufferFactory.GetContext();

	materialInfo.ShaderCode = R"(
		import CookTorranceBSDF

		vec3 Evaluate(BSDFInput bsdfInput)
		{
			CookTorranceBSDFInput cookTorrInput;
			cookTorrInput.ViewDir = bsdfInput.ViewDir;
			cookTorrInput.LightDir = bsdfInput.LightDir;
			cookTorrInput.Normal = bsdfInput.Normal;
			cookTorrInput.Metallic = 0.1;
			cookTorrInput.Roughness = 0.4;
			cookTorrInput.BaseColor = vec3(0.4, 0.0, 0.4);
			cookTorrInput.BaseColor = vec3(1.0, 0.0, 1.0);
			cookTorrInput.RefractiveIndex = 7.5;
			cookTorrInput.TransmissionWeight = 0.0;

			return CookTorranceBRDF(cookTorrInput);
		}
	)";

	auto instance = mMaterialInstances.emplace_back(mMaterialSystem.BuildDeferGFXInstance(materialInfo));
}
