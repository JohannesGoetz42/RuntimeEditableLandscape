// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeGrassType.h"
#include "RuntimeLandscape.h"
#include "Components/ActorComponent.h"
#include "RuntimeLandscapeRebuildManager.generated.h"


class FGenerateVertexRowDataRunner;
class ARuntimeLandscape;
class URuntimeLandscapeComponent;

struct FLandscapeGrassVertexData
{
	FGrassVariety GrassVariety;
	TArray<FTransform> InstanceTransforms;
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
	TArray<FVector> Vertices;
	TArray<int32> Triangles;

	// UV
	TArray<FVector2D> UV0Coords;
	TArray<FVector2D> UV1Coords;
	// Tangents
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;

	// Additional data
	TArray<FLandscapeGrassVertexData> GrassData;
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

	friend class FGenerateVertexRowDataRunner;

public:
	URuntimeLandscapeRebuildManager();
	void QueueRebuild(URuntimeLandscapeComponent* ComponentToRebuild);
	FORCEINLINE FQueuedThreadPool* GetThreadPool() const { return ThreadPool; }
	void NotifyRunnerFinished(const FGenerateVertexRowDataRunner* FinishedRunner);

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
	TArray<FGenerateVertexRowDataRunner*> GenerationRunners;
	std::atomic<int32> ActiveRunners;

	virtual void OnComponentCreated() override
	{
		Super::OnComponentCreated();
		Landscape = Cast<ARuntimeLandscape>(GetOwner());
		check(Landscape);

		InitializeBuffer();
		InitializeGenerationCache();
		InitializeRunners();
	}

	void InitializeGenerationCache();
	void InitializeRunners();
	void InitializeBuffer();

	void StartRebuild();

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
