// Fill out your copyright notice in the Description page of Project Settings.


#include "Threads/RuntimeLandscapeRebuildManager.h"

#include "KismetProceduralMeshLibrary.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Threads/GenerateVertexRowDataThread.h"

URuntimeLandscapeRebuildManager::URuntimeLandscapeRebuildManager() : Super()
{
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
	}
}

void URuntimeLandscapeRebuildManager::InitializeGenerationCache()
{
	GenerationDataCache.UV1Scale = FVector2D::One() / Landscape->GetComponentAmount();
	GenerationDataCache.VertexDistance = Landscape->GetQuadSideLength();
	GenerationDataCache.UVIncrement = 1 / Landscape->GetComponentResolution().X;
}

void URuntimeLandscapeRebuildManager::InitializeThreads()
{
	for (int32 i = 0; i <= GenerationDataCache.ComponentResolution.Y + 1; ++i)
	{
		auto* Thread = new FGenerateVertexRowDataThread(this, i);
		GenerationThreads.Add(Thread);
	}
}

void URuntimeLandscapeRebuildManager::InitializeBuffer()
{
	int32 VertexAmount = Landscape->GetTotalVertexAmountPerComponent();

	DataBuffer = FRuntimeLandscapeRebuildBuffer();
	DataBuffer.HeightValues.SetNumUninitialized(VertexAmount);
	DataBuffer.Vertices.SetNumUninitialized(VertexAmount);
	DataBuffer.UV0Coords.SetNumUninitialized(VertexAmount);
	DataBuffer.UV1Coords.SetNumUninitialized(VertexAmount);
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
	// Start data generation threads
	RunningThreadCount = GenerationDataCache.ComponentResolution.Y + 1;

	const FVector2D UV1Offset = GenerationDataCache.UV1Scale * FVector2D(SectionCoordinates.X, SectionCoordinates.Y);
	for (int32 Y = -1; Y < GenerationDataCache.ComponentResolution.Y; Y++)
	{
		GenerationThreads[Y + 1]->InitializeRun(Y, VertexIndex, UV1Offset);
		VertexIndex += GenerationDataCache.ComponentResolution.X + 1;
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
