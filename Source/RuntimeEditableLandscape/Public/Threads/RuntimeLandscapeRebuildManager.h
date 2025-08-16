// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeGrassType.h"
#include "RuntimeLandscape.h"
#include "Components/ActorComponent.h"
#include "RuntimeLandscapeRebuildManager.generated.h"


class FGenerateVertexRowDataThread;
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

	FVector2D ComponentResolution;
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

	friend class FGenerateVertexRowDataThread;

public:
	URuntimeLandscapeRebuildManager();
	void QueueRebuild(URuntimeLandscapeComponent* ComponentToRebuild);

private:
	UPROPERTY(VisibleAnywhere)
	URuntimeLandscapeComponent* CurrentComponent = nullptr;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ARuntimeLandscape> Landscape;
	UPROPERTY(VisibleAnywhere)
	FGenerationDataCache GenerationDataCache;
	UPROPERTY(VisibleAnywhere)
	FRuntimeLandscapeRebuildBuffer DataBuffer;
	UPROPERTY(VisibleAnywhere)
	uint8 RunningThreadCount;

	TArray<FGenerateVertexRowDataThread*> GenerationThreads;
	TQueue<URuntimeLandscapeComponent*> RebuildQueue;

	virtual void OnComponentCreated() override
	{
		Super::OnComponentCreated();
		Landscape = Cast<ARuntimeLandscape>(GetOwner());
		check(Landscape);
	}

	void InitializeGenerationCache();
	void InitializeThreads();
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
		}
	}

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
