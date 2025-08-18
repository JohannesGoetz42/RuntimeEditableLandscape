// Fill out your copyright notice in the Description page of Project Settings.


#include "Threads/GenerateVerticesRunner.h"

#include "RuntimeLandscape.h"
#include "Threads/RuntimeLandscapeRebuildManager.h"

FGenerateVerticesRunner::FGenerateVerticesRunner(URuntimeLandscapeRebuildManager* RebuildManager)
{
	this->RebuildManager = RebuildManager;
}

FGenerateVerticesRunner::~FGenerateVerticesRunner()
{
	checkNoEntry();
}

void FGenerateVerticesRunner::DoThreadedWork()
{
	const TArray<float>& HeightValues = RebuildManager->DataBuffer.HeightValues;
	const ARuntimeLandscape* Landscape = RebuildManager->Landscape;
	int32 VertexIndex = 0;
	const FGenerationDataCache& DataCache = RebuildManager->GenerationDataCache;
	FRuntimeLandscapeRebuildBuffer& DataBuffer = RebuildManager->DataBuffer;

	// First row of vertices is handled differently
	for (int32 X = 0; X <= Landscape->GetComponentResolution().X; X++)
	{
		const FVector Location(X * DataCache.VertexDistance, 0,
		                       HeightValues[VertexIndex] - Landscape->GetParentHeight());
		DataBuffer.VerticesRelative[VertexIndex] = Location;
		const FVector2D UV0 = FVector2D(X * DataCache.UVIncrement, 0);
		DataBuffer.UV0Coords[VertexIndex] = UV0;
		DataBuffer.UV1Coords[VertexIndex] = UV0 * DataCache.UV1Scale + UV1Offset;
		VertexIndex++;
	}

	for (int32 Y = 0; Y < Landscape->GetComponentResolution().Y; Y++)
	{
		const float Y1 = Y + 1;
		FVector Location(0, Y1 * DataCache.VertexDistance, HeightValues[VertexIndex] - Landscape->GetParentHeight());
		DataBuffer.VerticesRelative[VertexIndex] = Location;

		FVector2D UV0 = FVector2D(0, Y1 * DataCache.UVIncrement);
		DataBuffer.UV0Coords[VertexIndex] = UV0;
		DataBuffer.UV1Coords[VertexIndex] = UV0 * DataCache.UV1Scale + UV1Offset;
		VertexIndex++;

		// generate triangle strip in X direction
		for (int32 X = 0; X < Landscape->GetComponentResolution().X; X++)
		{
			Location = FVector((X + 1) * DataCache.VertexDistance, Y1 * DataCache.VertexDistance,
			                   HeightValues[VertexIndex] - Landscape->GetParentHeight());
			DataBuffer.VerticesRelative[VertexIndex] = Location;

			UV0 = FVector2D((X + 1) * DataCache.UVIncrement,
			                Y1 * DataCache.UVIncrement);
			DataBuffer.UV0Coords[VertexIndex] = UV0;
			DataBuffer.UV1Coords[VertexIndex] = UV0 * DataCache.UV1Scale + UV1Offset;

			VertexIndex++;

			//
			// const TSet<int32>& VerticesInHole = RebuildManager->CurrentComponent->VerticesInHole;
			// if (VerticesInHole.Contains(T1) || VerticesInHole.Contains(T2) || VerticesInHole.Contains(T3) ||
			// 	VerticesInHole.Contains(T2 + 1))
			// {
			// 	continue;
			// }
		}
	}

	RebuildManager->NotifyRunnerFinished(this);
}
