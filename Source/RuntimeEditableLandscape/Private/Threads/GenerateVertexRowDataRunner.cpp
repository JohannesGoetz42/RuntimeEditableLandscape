// Fill out your copyright notice in the Description page of Project Settings.


#include "Threads/GenerateVertexRowDataRunner.h"

#include "RuntimeLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Threads/RuntimeLandscapeRebuildManager.h"

FGenerateVertexRowDataRunner::FGenerateVertexRowDataRunner(URuntimeLandscapeRebuildManager* RebuildManager)
{
	this->RebuildManager = RebuildManager; 
}

FGenerateVertexRowDataRunner::~FGenerateVertexRowDataRunner()
{
	checkNoEntry();
}

void FGenerateVertexRowDataRunner::DoThreadedWork()
{
	const float Y1 = YCoordinate + 1;
	const TArray<float>& HeightValues = RebuildManager->DataBuffer.HeightValues;
	const ARuntimeLandscape* Landscape = RebuildManager->Landscape;
	int32 VertexIndex = StartIndex;
	const FGenerationDataCache& DataCache = RebuildManager->GenerationDataCache;
	FRuntimeLandscapeRebuildBuffer& DataBuffer = RebuildManager->DataBuffer;
	// First row of vertices is handled differently
	if (VertexIndex == 0)
	{
		for (int32 X = 0; X <= Landscape->GetComponentResolution().X; X++)
		{
			const FVector Location(X * DataCache.VertexDistance, 0,
			                       HeightValues[VertexIndex] - Landscape->GetParentHeight());
			DataBuffer.Vertices[VertexIndex] = Location;
			GenerateGrassDataForVertex(Location, X, 0);
			const FVector2D UV0 = FVector2D(X * DataCache.UVIncrement, 0);
			DataBuffer.UV0Coords[VertexIndex] = UV0;
			DataBuffer.UV1Coords[VertexIndex] = UV0 * DataCache.UV1Scale + UV1Offset;
			VertexIndex++;
		}

		RebuildManager->RunningThreadCount--;
		return;
	}

	FVector Location(0, Y1 * DataCache.VertexDistance, HeightValues[VertexIndex] - Landscape->GetParentHeight());
	DataBuffer.Vertices[VertexIndex] = Location;
	GenerateGrassDataForVertex(Location, 0, YCoordinate);

	FVector2D UV0 = FVector2D(0, Y1 * DataCache.UVIncrement);
	DataBuffer.UV0Coords[VertexIndex] = UV0;
	DataBuffer.UV1Coords[VertexIndex] = UV0 * DataCache.UV1Scale + UV1Offset;
	VertexIndex++;

	// generate triangle strip in X direction
	for (int32 X = 0; X < Landscape->GetComponentResolution().X; X++)
	{
		Location = FVector((X + 1) * DataCache.VertexDistance, Y1 * DataCache.VertexDistance,
		                   HeightValues[VertexIndex] - Landscape->GetParentHeight());
		DataBuffer.Vertices[VertexIndex] = Location;
		GenerateGrassDataForVertex(Location, X, YCoordinate);

		UV0 = FVector2D((X + 1) * DataCache.UVIncrement,
		                Y1 * DataCache.UVIncrement);
		DataBuffer.UV0Coords[VertexIndex] = UV0;
		DataBuffer.UV1Coords[VertexIndex] = UV0 * DataCache.UV1Scale + UV1Offset;

		VertexIndex++;

		const int32 T1 = YCoordinate * (Landscape->GetComponentResolution().X + 1) + X;
		const int32 T2 = T1 + Landscape->GetComponentResolution().X + 1;
		const int32 T3 = T1 + 1;

		const TSet<int32>& VerticesInHole = RebuildManager->CurrentComponent->VerticesInHole;
		if (VerticesInHole.Contains(T1) || VerticesInHole.Contains(T2) || VerticesInHole.Contains(T3) ||
			VerticesInHole.Contains(T2 + 1))
		{
			continue;
		}

		int32 TriangleBaseIndex = VertexIndex * 6;
		// add upper-left triangle

		DataBuffer.Triangles[TriangleBaseIndex] = T1;
		DataBuffer.Triangles[TriangleBaseIndex + 1] = T2;
		DataBuffer.Triangles[TriangleBaseIndex + 2] = T3;

		// add lower-right triangle
		DataBuffer.Triangles[TriangleBaseIndex + 3] = T3;
		DataBuffer.Triangles[TriangleBaseIndex + 4] = T2;
		DataBuffer.Triangles[TriangleBaseIndex + 5] = T2 + 1;
	}

	RebuildManager->RunningThreadCount--;
}

void FGenerateVertexRowDataRunner::GenerateGrassDataForVertex(const FVector& VertexLocation, int32 X, int32 Y)
{
	const ULandscapeGrassType* SelectedGrass = nullptr;
	float HighestWeight = 0;

	bool bIsLayerApplied = false;
	for (const auto& LayerWeightData : RebuildManager->Landscape->GetGroundTypeLayerWeightsAtVertexCoordinates(
		     RebuildManager->CurrentComponent->Index, X, Y))
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
		const float VertexHeight = RebuildManager->CurrentComponent->GetComponentLocation().Z + VertexLocation.Z;
		for (const FHeightBasedLandscapeData& HeightBasedData : RebuildManager->CurrentComponent->ParentLandscape->
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

void FGenerateVertexRowDataRunner::GenerateGrassTransformsAtVertex(const ULandscapeGrassType* SelectedGrass,
                                                                   const FVector& VertexRelativeLocation,
                                                                   float Weight)
{
	// for (const FGrassVariety& Variety : SelectedGrass->GrassVarieties)
	// {
	// 	// UHierarchicalInstancedStaticMeshComponent* InstancedStaticMesh = FindOrAddGrassMesh(Variety);
	//
	// 	int32 RemainingInstanceCount = FMath::RoundToInt32(
	// 		RebuildManager->Landscape->GetAreaPerSquare() * Variety.GetDensity() * 0.000001f * Weight);
	// 	FLandscapeGrassVertexData GrassData;
	// 	GrassData.GrassVariety = Variety;
	//
	// 	while (RemainingInstanceCount > 0)
	// 	{
	// 		FVector GrassLocation;
	// 		GetRandomGrassLocation(VertexRelativeLocation, GrassLocation);
	//
	// 		FRotator Rotation;
	// 		GetRandomGrassRotation(Variety, Rotation);
	//
	// 		FVector Scale;
	// 		GetRandomGrassScale(Variety, Scale);
	//
	// 		FTransform InstanceTransform(Rotation, GrassLocation, Scale);
	// 		// InstancedStaticMesh->AddInstance(InstanceTransform);
	// 		GrassData.InstanceTransforms.Add(InstanceTransform);
	// 		--RemainingInstanceCount;
	// 	}
	// 	RebuildManager->DataBuffer.GrassData[VertexIndex] =
	// 		VertexData.GrassData.Add(GrassData);
	// }
}

void FGenerateVertexRowDataRunner::GetRandomGrassRotation(const FGrassVariety& Variety, FRotator& OutRotation) const
{
	if (Variety.RandomRotation)
	{
		float RandomRotation = FMath::RandRange(-180.0f, 180.0f);
		OutRotation = FRotator(0.0f, RandomRotation, 0.0f);
	}
}

void FGenerateVertexRowDataRunner::GetRandomGrassLocation(const FVector& VertexRelativeLocation,
                                                          FVector& OutGrassLocation) const
{
	float PosX = FMath::RandRange(-0.5f, 0.5f);
	float PosY = FMath::RandRange(-0.5f, 0.5f);

	float SideLength = RebuildManager->CurrentComponent->ParentLandscape->GetQuadSideLength();
	OutGrassLocation = VertexRelativeLocation + FVector(PosX * SideLength, PosY * SideLength, 0.0f);
}

void FGenerateVertexRowDataRunner::GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale) const
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
