// Fill out your copyright notice in the Description page of Project Settings.


#include "Threads/RuntimeLandscapeRebuildManager.h"

#include "KismetProceduralMeshLibrary.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Threads/GenerateAdditionalVertexDataWorker.h"
#include "Threads/GenerateVerticesWorker.h"

URuntimeLandscapeRebuildManager::URuntimeLandscapeRebuildManager() : Super()
{
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.1f;
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
	VertexRunner = new FGenerateVerticesWorker(this);
	for (int32 i = 0; i < Landscape->GetComponentResolution().Y + 1; ++i)
	{
		FGenerateAdditionalVertexDataWorker* AdditionalDataRunner = new FGenerateAdditionalVertexDataWorker(this);
		AdditionalDataRunners.Add(AdditionalDataRunner);
	}

	ThreadPool = FQueuedThreadPool::Allocate();
	int32 NumThreadsInThreadPool = FPlatformMisc::NumberOfWorkerThreadsToSpawn();
	verify(
		ThreadPool->Create(NumThreadsInThreadPool, 32 * 1024, TPri_Normal, TEXT("Runtime Landscape rebuild thread")));
}

void URuntimeLandscapeRebuildManager::InitializeBuffer()
{
	int32 VertexAmount = Landscape->GetTotalVertexAmountPerComponent();

	DataBuffer = FRuntimeLandscapeRebuildBuffer();
	DataBuffer.HeightValues.SetNumUninitialized(VertexAmount);
	DataBuffer.VerticesRelative.SetNumUninitialized(VertexAmount);
	DataBuffer.UV0Coords.SetNumUninitialized(VertexAmount);
	DataBuffer.UV1Coords.SetNumUninitialized(VertexAmount);

	// initialize the grass data with empty structs
	DataBuffer.GrassData.Empty(VertexAmount);
	for (int32 i = 0; i < VertexAmount; ++i)
	{
		DataBuffer.GrassData.Add(FLandscapeGrassVertexData());
	}

	DataBuffer.Triangles.Empty(
		Landscape->GetComponentResolution().X * Landscape->GetComponentResolution().Y * 6);

	// initialize triangle array, since the generation algorithm is always the same, this will always be the same for each component
	for (int32 Y = 0; Y < Landscape->GetComponentResolution().Y; ++Y)
	{
		for (int32 X = 0; X < Landscape->GetComponentResolution().X; ++X)
		{
			const int32 T1 = Y * (Landscape->GetComponentResolution().X + 1) + X;
			const int32 T2 = T1 + Landscape->GetComponentResolution().X + 1;
			const int32 T3 = T1 + 1;

			// add upper-left triangle
			DataBuffer.Triangles.Add(T1);
			DataBuffer.Triangles.Add(T2);
			DataBuffer.Triangles.Add(T3);

			// add lower-right triangle
			DataBuffer.Triangles.Add(T3);
			DataBuffer.Triangles.Add(T2);
			DataBuffer.Triangles.Add(T2 + 1);
		}
	}
}

void URuntimeLandscapeRebuildManager::StartRebuild()
{
	Initialize();

	UE_LOG(RuntimeEditableLandscape, Display, TEXT("Rebuilding Landscape component %s %i..."), *GetOwner()->GetName(),
	       CurrentComponent->Index);

	FIntVector2 SectionCoordinates;
	Landscape->GetComponentCoordinates(CurrentComponent->Index, SectionCoordinates);
	DataBuffer.UV1Offset = GenerationDataCache.UV1Scale * FVector2D(SectionCoordinates.X, SectionCoordinates.Y);

	// ensure the section data is valid
	if (!ensure(CurrentComponent->InitialHeightValues.Num() == Landscape->GetTotalVertexAmountPerComponent()))
	{
		UE_LOG(RuntimeEditableLandscape, Warning,
		       TEXT("Component %i could not generate valid data and will not be generated!"),
		       CurrentComponent->Index);
		return;
	}

	DataBuffer.RebuildState = ERuntimeLandscapeRebuildState::RLRS_BuildVertices;
	DataBuffer.HeightValues = CurrentComponent->InitialHeightValues;

	// TODO: Clean up. Do I need vertex colors?
	TArray<FColor> VertexColors;
	// TODO: Apply layer data on VertexRunner
	CurrentComponent->ApplyDataFromLayers(DataBuffer.HeightValues, VertexColors);

	ActiveRunners = 1;
	VertexRunner->QueueWork(DataBuffer.UV1Offset);
	SetComponentTickEnabled(true);
}

void URuntimeLandscapeRebuildManager::StartGenerateAdditionalData()
{
	DataBuffer.RebuildState = ERuntimeLandscapeRebuildState::RLRS_BuildAdditionalData;
	int32 VertexIndex = 0;
	// Start data generation runners
	ActiveRunners = Landscape->GetComponentResolution().Y + 1;

	for (int32 Y = 0; Y < Landscape->GetComponentResolution().Y + 1; Y++)
	{
		AdditionalDataRunners[Y]->QueueWork(Y, VertexIndex, DataBuffer.UV1Offset);
		VertexIndex += Landscape->GetComponentResolution().X + 1;
	}
}

void URuntimeLandscapeRebuildManager::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                                    FActorComponentTickFunction* ThisTickFunction)
{
	if (ActiveRunners < 1)
	{
		switch (DataBuffer.RebuildState)
		{
		case RLRS_BuildVertices:
			StartGenerateAdditionalData();
			break;
		case RLRS_BuildAdditionalData:
			CurrentComponent->FinishRebuild(DataBuffer);
			RebuildNextInQueue();
			break;
		default:
			checkNoEntry();
		}
	}
}
