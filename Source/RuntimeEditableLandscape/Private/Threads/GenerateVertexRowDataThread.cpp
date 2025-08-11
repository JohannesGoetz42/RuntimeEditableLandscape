// Fill out your copyright notice in the Description page of Project Settings.


#include "Threads/GenerateVertexRowDataThread.h"

#include "RuntimeLandscape.h"
#include "RuntimeLandscapeComponent.h"

FGenerateVertexRowDataThread::FGenerateVertexRowDataThread(URuntimeLandscapeComponent* LandscapeComponent,
                                                           const FVector2D& ComponentResolution,
                                                           const FVector2D& UV1Scale, const FVector2D& UV1Offset,
                                                           float VertexDistance,
                                                           float UVIncrement, int32 VertexStartIndex, int32 YCoordinate)
{
	this->LandscapeComponent = LandscapeComponent;
	this->ComponentResolution = ComponentResolution;
	this->UV1Scale = UV1Scale;
	this->UV1Offset = UV1Offset;
	this->VertexDistance = VertexDistance;
	this->UVIncrement = UVIncrement;
	this->YCoordinate = YCoordinate;
	this->VertexIndex = VertexStartIndex;

	Thread = FRunnableThread::Create(
		this, *FString::Printf(
			TEXT("FGenerateVertexRowDataThread %i, %i"), LandscapeComponent->Index, YCoordinate));
}

FGenerateVertexRowDataThread::~FGenerateVertexRowDataThread()
{
	if (Thread)
	{
		Thread->Kill();
		delete Thread;
	}
}

void FGenerateVertexRowDataThread::GenerateVertexData()
{
	const float Y1 = YCoordinate + 1;
	const TArray<float> HeightValues = LandscapeComponent->HeightValues;
	const ARuntimeLandscape* Landscape = LandscapeComponent->ParentLandscape;

	LandscapeComponent->GenerateVertexData(
		VertexData, FVector(0, Y1 * VertexDistance, HeightValues[VertexIndex] - Landscape->GetParentHeight()), 0,
		YCoordinate);

	FVector2D UV0 = FVector2D(0, Y1 * UVIncrement);
	VertexData.UV0Coords.Add(UV0);
	VertexData.UV1Coords.Add(UV0 * UV1Scale + UV1Offset);
	VertexIndex++;

	// generate triangle strip in X direction
	for (int32 X = 0; X < ComponentResolution.X; X++)
	{
		FVector VertexLocation = FVector((X + 1) * VertexDistance, Y1 * VertexDistance,
		                                 HeightValues[VertexIndex] - Landscape->GetParentHeight());
		LandscapeComponent->GenerateVertexData(VertexData, VertexLocation, X, YCoordinate);

		UV0 = FVector2D((X + 1) * UVIncrement, Y1 * UVIncrement);
		VertexData.UV0Coords.Add(UV0);
		VertexData.UV1Coords.Add(UV0 * UV1Scale + UV1Offset);

		VertexIndex++;

		int32 T1 = YCoordinate * (ComponentResolution.X + 1) + X;
		int32 T2 = T1 + ComponentResolution.X + 1;
		int32 T3 = T1 + 1;

		const TSet<int32>& VerticesInHole = LandscapeComponent->VerticesInHole;
		if (VerticesInHole.Contains(T1) || VerticesInHole.Contains(T2) || VerticesInHole.Contains(T3) ||
			VerticesInHole.Contains(T2 + 1))
		{
			continue;
		}

		// add upper-left triangle
		VertexData.Triangles.Add(T1);
		VertexData.Triangles.Add(T2);
		VertexData.Triangles.Add(T3);

		// add lower-right triangle
		VertexData.Triangles.Add(T3);
		VertexData.Triangles.Add(T2);
		VertexData.Triangles.Add(T2 + 1);
	}

	LandscapeComponent->ActiveGenerationThreads--;
}
