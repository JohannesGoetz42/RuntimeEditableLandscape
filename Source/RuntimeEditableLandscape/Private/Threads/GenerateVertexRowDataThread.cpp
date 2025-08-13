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

uint32 FGenerateVertexRowDataThread::Run()
{
	const float Y1 = YCoordinate + 1;
	const TArray<float> HeightValues = LandscapeComponent->HeightValues;
	const ARuntimeLandscape* Landscape = LandscapeComponent->ParentLandscape;

	// First row of vertices is handled differently
	if (VertexIndex == 0)
	{
		for (int32 X = 0; X <= ComponentResolution.X; X++)
		{
			GenerateVertexData(FVector(X * VertexDistance, 0, HeightValues[VertexIndex] - Landscape->GetParentHeight()),
			                   X, 0);
			const FVector2D UV0 = FVector2D(X * UVIncrement, 0);
			VertexData.UV0Coords.Add(UV0);
			VertexData.UV1Coords.Add(UV0 * UV1Scale + UV1Offset);
			VertexIndex++;
		}

		LandscapeComponent->ActiveGenerationThreads--;
		return 0;
	}


	GenerateVertexData(FVector(0, Y1 * VertexDistance, HeightValues[VertexIndex] - Landscape->GetParentHeight()), 0,
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
		GenerateVertexData(VertexLocation, X, YCoordinate);

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
	return 0;
}

void FGenerateVertexRowDataThread::GenerateVertexData(const FVector& VertexLocation, int32 X, int32 Y)
{
	VertexData.Vertices.Add(VertexLocation);
	GenerateGrassDataForVertex(VertexLocation, X, Y);
}

void FGenerateVertexRowDataThread::GenerateGrassDataForVertex(const FVector& VertexLocation, int32 X, int32 Y)
{
	const ULandscapeGrassType* SelectedGrass = nullptr;
	float HighestWeight = 0;

	bool bIsLayerApplied = false;
	for (const auto& LayerWeightData : LandscapeComponent->ParentLandscape->
	                                                       GetGroundTypeLayerWeightsAtVertexCoordinates(
		                                                       LandscapeComponent->Index, X, Y))
	{
		if (LayerWeightData.Value >= HighestWeight && LayerWeightData.Value > 0.2f)
		{
			HighestWeight = LayerWeightData.Value;
			SelectedGrass = LayerWeightData.Key->GrassType;
			bIsLayerApplied = true;
		}
	}

	// if no layer is applied, check if height based grass should be displayed
	if (!bIsLayerApplied)
	{
		const float VertexHeight = LandscapeComponent->GetComponentLocation().Z + VertexLocation.Z;
		for (const FHeightBasedLandscapeData& HeightBasedData : LandscapeComponent->ParentLandscape->
		     GetHeightBasedData())
		{
			if (HeightBasedData.MinHeight < VertexHeight && HeightBasedData.MaxHeight > VertexHeight)
			{
				SelectedGrass = HeightBasedData.Grass;
				HighestWeight = 1.0f;
			}
		}
	}

	if (SelectedGrass)
	{
		GenerateGrassTransformsAtVertex(SelectedGrass, VertexLocation, HighestWeight);
	}
}

void FGenerateVertexRowDataThread::GenerateGrassTransformsAtVertex(const ULandscapeGrassType* SelectedGrass,
                                                                   const FVector& VertexRelativeLocation,
                                                                   float Weight)
{
	for (const FGrassVariety& Variety : SelectedGrass->GrassVarieties)
	{
		// UHierarchicalInstancedStaticMeshComponent* InstancedStaticMesh = FindOrAddGrassMesh(Variety);

		int32 RemainingInstanceCount = FMath::RoundToInt32(
			LandscapeComponent->ParentLandscape->GetAreaPerSquare() * Variety.GetDensity() * 0.000001f * Weight);
		FLandscapeGrassVertexData GrassData;
		GrassData.GrassVariety = Variety;

		while (RemainingInstanceCount > 0)
		{
			FVector GrassLocation;
			GetRandomGrassLocation(VertexRelativeLocation, GrassLocation);

			FRotator Rotation;
			GetRandomGrassRotation(Variety, Rotation);

			FVector Scale;
			GetRandomGrassScale(Variety, Scale);

			FTransform InstanceTransform(Rotation, GrassLocation, Scale);
			// InstancedStaticMesh->AddInstance(InstanceTransform);
			GrassData.InstanceTransforms.Add(InstanceTransform);
			--RemainingInstanceCount;
		}

		VertexData.GrassData.Add(GrassData);
	}
}

void FGenerateVertexRowDataThread::GetRandomGrassRotation(const FGrassVariety& Variety, FRotator& OutRotation) const
{
	if (Variety.RandomRotation)
	{
		float RandomRotation = FMath::RandRange(-180.0f, 180.0f);
		OutRotation = FRotator(0.0f, RandomRotation, 0.0f);
	}
}

void FGenerateVertexRowDataThread::GetRandomGrassLocation(const FVector& VertexRelativeLocation,
                                                          FVector& OutGrassLocation) const
{
	float PosX = FMath::RandRange(-0.5f, 0.5f);
	float PosY = FMath::RandRange(-0.5f, 0.5f);

	float SideLength = LandscapeComponent->ParentLandscape->GetQuadSideLength();
	OutGrassLocation = VertexRelativeLocation + FVector(PosX * SideLength, PosY * SideLength, 0.0f);
}

void FGenerateVertexRowDataThread::GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale) const
{
	switch (Variety.Scaling)
	{
	case EGrassScaling::Uniform:
		OutScale = FVector(FMath::RandRange(Variety.ScaleX.Min, Variety.ScaleX.Max));
		break;
	case EGrassScaling::Free:
		OutScale.X = FMath::RandRange(Variety.ScaleX.Min, Variety.ScaleX.Max);
		OutScale.Y = FMath::RandRange(Variety.ScaleY.Min, Variety.ScaleY.Max);
		OutScale.Z = FMath::RandRange(Variety.ScaleZ.Min, Variety.ScaleZ.Max);
		break;
	case EGrassScaling::LockXY:
		OutScale.X = FMath::RandRange(Variety.ScaleX.Min, Variety.ScaleX.Max);
		OutScale.Y = OutScale.X;
		OutScale.Z = FMath::RandRange(Variety.ScaleZ.Min, Variety.ScaleZ.Max);
		break;
	default:
		ensureMsgf(false, TEXT("Scaling mode is not yet supported!"));
		OutScale = FVector::One();
	}
}
