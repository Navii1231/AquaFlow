#pragma once
#include "../Core/AqCore.h"
#include "../Core/SharedRef.h"

AQUA_BEGIN
EXEC_BEGIN

// Aqua Flow execution model

struct Operation;

enum class GraphTraversalState
{
	ePending                            = 0,
	eVisiting                           = 1,
	eVisited                            = 2,
};

enum class OpType
{
	eNone                               = 0,
	eCompute                            = 1,
	eGraphics                           = 2,
	eRayTracing                         = 3,
	eCopyOrTransfer                     = 4,
	eTransition                         = 5,
};

enum class State
{
	ePending                            = 0,
	eReady                              = 1,
	eExecute                            = 2,
};

enum class GraphError
{
	ePathDoesntExist                    = 1,
	ePathReferencedMoreThanOnce         = 2,
	eInvalidConnection                  = 3,
	eDependencyUponItself               = 4,
	eFoundEmbeddedCircuit               = 5,
	eInjectedOpDoesntExist              = 6,
};

struct DependencyMetaData
{
	std::string From;
	std::string To;
	vk::PipelineStageFlags WaitingStage;
};

struct InjectionMetaData
{
	vk::PipelineStageFlags WaitPoint;
	vkLib::Core::Ref<vk::Semaphore> Signal;
};

struct OpStates
{
	OpType Type = OpType::eNone;
	State Exec = State::ePending;

	GraphTraversalState TraversalState = GraphTraversalState::ePending;

	OpStates() = default;
	OpStates(OpType type) : Type(type) {}
};

EXEC_END
AQUA_END
