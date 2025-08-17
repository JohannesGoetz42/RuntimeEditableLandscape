// Fill out your copyright notice in the Description page of Project Settings.


#include "Threads/RuntimeLandscapeRebuildManager.h"

#include "KismetProceduralMeshLibrary.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Threads/GenerateVertexRowDataRunner.h"

URuntimeLandscapeRebuildManager::URuntimeLandscapeRebuildManager() : Super()
{
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.2f;
}

void URuntimeLandscapeRebuildManager::QueueRebuild(URuntimeLandscapeComponent* ComponentToRebuild)
{
	if (CurrentComponent)
	{
		RebuildQueue.Enqueue(ComponentToRebuild);
	}
	else
	{
		CurrentComponent = ComponentToRebuild;
		StartRebuild();
		SetComponentTickEnabled(true);
	}
}

void URuntimeLandscapeRebuildManager::InitializeGenerationCache()
{
	GenerationDataCache.UV1Scale = FVector2D::One() / Landscape->GetComponentAmount();
	GenerationDataCache.VertexDistance = Landscape->GetQuadSideLength();
	GenerationDataCache.UVIncrement = 1 / Landscape->GetComponentResolution().X;
}

void URuntimeLandscapeRebuildManager::InitializeRunners()
{
	for (int32 i = 0; i <= Landscape->GetComponentResolution().Y + 1; ++i)
	{
		auto* Thread = new FGenerateVertexRowDataRunner(this);
		GenerationThreads.Add(Thread);
	}

	ThreadPool = FQueuedThreadPool::Allocate();
	int32 NumThreadsInThreadPool = FPlatformMisc::NumberOfWorkerThreadsToSpawn();
	verify(ThreadPool->Create(NumThreadsInThreadPool));
}

void URuntimeLandscapeRebuildManager::InitializeBuffer()
{
	int32 VertexAmount = Landscape->GetTotalVertexAmountPerComponent();

	DataBuffer = FRuntimeLandscapeRebuildBuffer();
	DataBuffer.HeightValues.SetNumUninitialized(VertexAmount);
	DataBuffer.Vertices.SetNumUninitialized(VertexAmount);
	DataBuffer.UV0Coords.SetNumUninitialized(VertexAmount);
	DataBuffer.UV1Coords.SetNumUninitialized(VertexAmount);
	DataBuffer.Triangles.SetNumUninitialized(VertexAmount * 6);
}

void URuntimeLandscapeRebuildManager::StartRebuild()
{
	UE_LOG(RuntimeEditableLandscape, Display, TEXT("Rebuilding Landscape component %s %i..."), *GetOwner()->GetName(),
	       CurrentComponent->Index);

	FIntVector2 SectionCoordinates;
	Landscape->GetComponentCoordinates(CurrentComponent->Index, SectionCoordinates);

	// ensure the section data is valid
	if (!ensure(CurrentComponent->InitialHeightValues.Num() == Landscape->GetTotalVertexAmountPerComponent()))
	{
		UE_LOG(RuntimeEditableLandscape, Warning,
		       TEXT("Component %i could not generate valid data and will not be generated!"),
		       CurrentComponent->Index);
		return;
	}

	DataBuffer.HeightValues = CurrentComponent->InitialHeightValues;

	// TODO: Clean up. Do I need vertex colors?
	TArray<FColor> VertexColors;
	CurrentComponent->ApplyDataFromLayers(DataBuffer.HeightValues, VertexColors);

	int32 VertexIndex = 0;
	// Start data generation runners
	RunningThreadCount = Landscape->GetComponentResolution().Y + 1;

	const FVector2D UV1Offset = GenerationDataCache.UV1Scale * FVector2D(SectionCoordinates.X, SectionCoordinates.Y);
	for (int32 Y = -1; Y < Landscape->GetComponentResolution().Y; Y++)
	{
		GenerationThreads[Y + 1]->QueueWork(Y, VertexIndex, UV1Offset);
		VertexIndex += Landscape->GetComponentResolution().X + 1;
	}
}

void URuntimeLandscapeRebuildManager::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                                    FActorComponentTickFunction* ThisTickFunction)
{
	if (RunningThreadCount == 0)
	{
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(DataBuffer.Vertices, DataBuffer.Triangles,
		                                                       DataBuffer.UV0Coords, DataBuffer.Normals,
		                                                       DataBuffer.Tangents);
		CurrentComponent->FinishRebuild(DataBuffer);
	}
}
