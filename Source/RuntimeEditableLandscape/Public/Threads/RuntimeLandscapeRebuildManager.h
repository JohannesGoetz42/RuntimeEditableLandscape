// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeGrassType.h"
#include "RuntimeLandscape.h"
#include "Components/ActorComponent.h"
#include "RuntimeLandscapeRebuildManager.generated.h"


struct FProcMeshTangent;
class FGenerateAdditionalVertexDataRunner;
class FGenerateVerticesRunner;
class ARuntimeLandscape;
class URuntimeLandscapeComponent;

enum ERuntimeLandscapeRebuildState : uint8
{
	RLRS_None,
	RLRS_BuildVertices,
	RLRS_BuildAdditionalData
};

struct FLandscapeGrassVertexData
{
	FGrassVariety GrassVariety;
	TArray<FTransform> InstanceTransformsRelative;
};

USTRUCT()
/**
 * Stores data required to rebuild a single runtime landscape component
 */
struct FRuntimeLandscapeRebuildBuffer
{
	GENERATED_BODY()

	// InputData
	TArray<float> HeightValues;

	// Vertices
	TArray<FVector> VerticesRelative;
	TArray<int32> Triangles;

	// UV
	TArray<FVector2D> UV0Coords;
	TArray<FVector2D> UV1Coords;
	FVector2D UV1Offset;

	// Tangents
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;

	// Additional data
	TArray<FLandscapeGrassVertexData> GrassData;

	ERuntimeLandscapeRebuildState RebuildState = ERuntimeLandscapeRebuildState::RLRS_None;
};

USTRUCT()
/**
 * Caches information required to rebuild the components 
 */
struct FGenerationDataCache
{
	GENERATED_BODY()

	FVector2D UV1Scale;
	float VertexDistance;
	float UVIncrement;
};

UCLASS(Hidden)
/**
 * Manages threads for rebuilding the landscape
 */
class RUNTIMEEDITABLELANDSCAPE_API URuntimeLandscapeRebuildManager : public UActorComponent
{
	GENERATED_BODY()

	friend class FGenerateVerticesRunner;
	friend class FGenerateAdditionalVertexDataRunner;

public:
	URuntimeLandscapeRebuildManager();
	void QueueRebuild(URuntimeLandscapeComponent* ComponentToRebuild);
	FORCEINLINE FQueuedThreadPool* GetThreadPool() const { return ThreadPool; }
	void NotifyRunnerFinished(const FGenerateAdditionalVertexDataRunner* FinishedRunner);

	void NotifyRunnerFinished(const FGenerateVerticesRunner* FinishedRunner)
	{
		StartGenerateAdditionalData();
	}

private:
	UPROPERTY(VisibleAnywhere)
	URuntimeLandscapeComponent* CurrentComponent = nullptr;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ARuntimeLandscape> Landscape;
	UPROPERTY(VisibleAnywhere)
	FGenerationDataCache GenerationDataCache;
	UPROPERTY(VisibleAnywhere)
	FRuntimeLandscapeRebuildBuffer DataBuffer;
	TQueue<URuntimeLandscapeComponent*> RebuildQueue;

	FQueuedThreadPool* ThreadPool;
	FGenerateVerticesRunner* VertexRunner;
	TArray<FGenerateAdditionalVertexDataRunner*> AdditionalDataRunners;
	std::atomic<int32> ActiveRunners;

	void Initialize()
	{
		if (AdditionalDataRunners.IsEmpty())
		{
			Landscape = Cast<ARuntimeLandscape>(GetOwner());
			check(Landscape);

			InitializeBuffer();
			InitializeGenerationCache();
			InitializeRunners();
		}
	}

	void InitializeGenerationCache();
	void InitializeRunners();
	void InitializeBuffer();

	/** 1st step: Rebuild vertex data on a single thread, since this is relatively fast */
	void StartRebuild();
	/** 2nd step: Rebuild additional data on multiple threads */
	void StartGenerateAdditionalData();

	void RebuildNextInQueue()
	{
		if (RebuildQueue.Dequeue(CurrentComponent))
		{
			StartRebuild();
		}
		else
		{
			CurrentComponent = nullptr;
			SetComponentTickEnabled(false);
		}
	}

	void CancelRebuild()
	{
		CurrentComponent = nullptr;
		ActiveRunners = 0;
		SetComponentTickEnabled(false);
	}

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
