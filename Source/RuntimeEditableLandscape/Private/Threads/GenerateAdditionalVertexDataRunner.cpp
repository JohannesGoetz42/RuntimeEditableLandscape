// Fill out your copyright notice in the Description page of Project Settings.


#include "Threads/GenerateAdditionalVertexDataRunner.h"

#include "LandscapeGrassType.h"
#include "RuntimeLandscapeComponent.h"
#include "Threads/RuntimeLandscapeRebuildManager.h"

FGenerateAdditionalVertexDataRunner::FGenerateAdditionalVertexDataRunner(
	URuntimeLandscapeRebuildManager* RebuildManager)
{
	this->RebuildManager = RebuildManager;
}

FGenerateAdditionalVertexDataRunner::~FGenerateAdditionalVertexDataRunner()
{
	checkNoEntry();
}

void FGenerateAdditionalVertexDataRunner::GenerateGrassDataForVertex(const int32 VertexIndex, int32 X)
{
	// Don't add grass at first row or column, since it overlaps with the last row or column of neighboring component
	if (YCoordinate == 0 || X == 0)
	{
		RebuildManager->DataBuffer.GrassData[VertexIndex].InstanceTransformsRelative.Empty();
		return;
	}

	const ULandscapeGrassType* SelectedGrass = nullptr;
	float HighestWeight = 0;

	bool bIsLayerApplied = false;
	for (const auto& LayerWeightData : RebuildManager->Landscape->GetGroundTypeLayerWeightsAtVertexCoordinates(
		     RebuildManager->CurrentComponent->GetComponentIndex(), X, YCoordinate))
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
		const float VertexHeight = (RebuildManager->DataBuffer.VerticesRelative[VertexIndex]
			+ RebuildManager->CurrentComponent->GetComponentLocation()).Z;

		for (const FHeightBasedLandscapeData& HeightBasedData : RebuildManager->CurrentComponent->
		     GetParentLandscape()->GetHeightBasedData())
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
		GenerateGrassTransformsAtVertex(SelectedGrass, VertexIndex, HighestWeight);
	}
	// if no grass is selected, clean data carried over from previous run
	else
	{
		RebuildManager->DataBuffer.GrassData[VertexIndex].InstanceTransformsRelative.Empty();
	}
}

void FGenerateAdditionalVertexDataRunner::GenerateGrassTransformsAtVertex(const ULandscapeGrassType* SelectedGrass,
                                                                          const int32 VertexIndex, float Weight)
{
	const FVector& VertexRelativeLocation = RebuildManager->DataBuffer.VerticesRelative[VertexIndex];

	FLandscapeGrassVertexData& GrassData = RebuildManager->DataBuffer.GrassData[VertexIndex];
	for (const FGrassVariety& Variety : SelectedGrass->GrassVarieties)
	{
		int32 RemainingInstanceCount = FMath::RoundToInt32(
			RebuildManager->Landscape->GetAreaPerSquare() * Variety.GetDensity() * 0.00001f * Weight);

		GrassData.InstanceTransformsRelative.Empty(RemainingInstanceCount);
		GrassData.GrassVariety = Variety;
		while (RemainingInstanceCount > 0)
		{
			FVector GrassLocationRelative;
			GetRandomGrassLocation(VertexRelativeLocation, GrassLocationRelative);

			FRotator Rotation;
			GetRandomGrassRotation(Variety, Rotation);

			FVector Scale;
			GetRandomGrassScale(Variety, Scale);

			FTransform InstanceTransformRelative(Rotation, GrassLocationRelative, Scale);
			GrassData.InstanceTransformsRelative.Add(InstanceTransformRelative);
			--RemainingInstanceCount;
		}
	}
}

void FGenerateAdditionalVertexDataRunner::GetRandomGrassRotation(const FGrassVariety& Variety,
                                                                 FRotator& OutRotation) const
{
	if (Variety.RandomRotation)
	{
		float RandomRotation = FMath::RandRange(-180.0f, 180.0f);
		OutRotation = FRotator(0.0f, RandomRotation, 0.0f);
	}
}

void FGenerateAdditionalVertexDataRunner::GetRandomGrassLocation(const FVector& VertexRelativeLocation,
                                                                 FVector& OutGrassLocation) const
{
	float PosX = FMath::RandRange(-0.5f, 0.5f);
	float PosY = FMath::RandRange(-0.5f, 0.5f);

	float SideLength = RebuildManager->CurrentComponent->GetParentLandscape()->GetQuadSideLength();
	OutGrassLocation = VertexRelativeLocation + FVector(PosX * SideLength, PosY * SideLength, 0.0f);
}

void FGenerateAdditionalVertexDataRunner::GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale) const
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

void FGenerateAdditionalVertexDataRunner::DoThreadedWork()
{
	// skip first column of vertices (since it overlaps with last column of neighbor)
	int32 VertexIndex = StartIndex;
	for (int32 X = 0; X < RebuildManager->Landscape->GetComponentResolution().X + 1; ++X)
	{
		GenerateGrassDataForVertex(VertexIndex, X);
		++VertexIndex;
	}

	RebuildManager->NotifyRunnerFinished(this);
}
