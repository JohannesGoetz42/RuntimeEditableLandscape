// Fill out your copyright notice in the Description page of Project Settings.


#include "Threads/GenerateAdditionalVertexDataWorker.h"

#include "LandscapeGrassType.h"
#include "RuntimeLandscapeComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Threads/RuntimeLandscapeRebuildManager.h"

FGenerateAdditionalVertexDataWorker::FGenerateAdditionalVertexDataWorker(
	URuntimeLandscapeRebuildManager* RebuildManager)
{
	this->RebuildManager = RebuildManager;
}

FGenerateAdditionalVertexDataWorker::~FGenerateAdditionalVertexDataWorker()
{
	checkNoEntry();
}

void FGenerateAdditionalVertexDataWorker::GenerateGrassDataForVertex(const int32 VertexIndex, int32 X)
{
	// Don't add grass at first row or column, since it overlaps with the last row or column of neighboring component
	if (YCoordinate == 0 || X == 0)
	{
		RebuildManager->DataBuffer.AdditionalData[VertexIndex].ClearData();
		return;
	}

	const FGrassTypeSettings* SelectedGrass = nullptr;
	float HighestWeight = 0;

	bool bIsLayerApplied = false;
	for (const auto& LayerWeightData : RebuildManager->Landscape->GetGroundTypeLayerWeightsAtVertexCoordinates(
		     RebuildManager->CurrentComponent->GetComponentIndex(), X, YCoordinate))
	{
		if (LayerWeightData.Value >= HighestWeight && LayerWeightData.Value > 0.2f)
		{
			HighestWeight = LayerWeightData.Value;
			SelectedGrass = &LayerWeightData.Key->GrassTypeSettings;
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
				SelectedGrass = &HeightBasedData.Grass;
				HighestWeight = 1.0f;
			}
		}
	}

	// clean data carried over from previous run
	RebuildManager->DataBuffer.AdditionalData[VertexIndex].ClearData();

	GenerateGrassTransformsAtVertex(SelectedGrass, VertexIndex, HighestWeight);
}

void FGenerateAdditionalVertexDataWorker::GenerateGrassTransformsAtVertex(const FGrassTypeSettings* SelectedGrass,
                                                                             const int32 VertexIndex,
                                                                             float Weight) const
{
	if (!SelectedGrass || !SelectedGrass->GrassType)
	{
		return;
	}


	const FVector& Normal = RebuildManager->DataBuffer.Normals[VertexIndex];

	float Roll;
	float Pitch;
	UKismetMathLibrary::GetSlopeDegreeAngles(FVector::RightVector, Normal, FVector::UpVector, Pitch, Roll);
	// don't generate grass data if the vertex normal is steeper than the max
	if (SelectedGrass->MaxSlopeAngle > 0.0f
		&& (FMath::Abs(Roll) > SelectedGrass->MaxSlopeAngle
			|| FMath::Abs(Pitch) > SelectedGrass->MaxSlopeAngle))
	{
		return;
	}

	FRotator SurfaceAlignmentRotation = UKismetMathLibrary::MakeRotFromZ(Normal);
	const FVector& VertexRelativeLocation = RebuildManager->DataBuffer.VerticesRelative[VertexIndex];
	FLandscapeAdditionalData& AdditionalData = RebuildManager->DataBuffer.AdditionalData[VertexIndex];

	for (const FGrassVariety& Variety : SelectedGrass->GrassType->GrassVarieties)
	{
		FLandscapeGrassVertexData& GrassData = AdditionalData.GrassData.FindOrAdd(Variety.GrassMesh);

		float InstanceCount = RebuildManager->Landscape->GetAreaPerSquare() * Variety.GetDensity() * 0.000001f *
			Weight;
		int32 RemainingInstanceCount = FMath::FloorToInt(InstanceCount);

		// round up based on decimal remainder
		float Remainder = InstanceCount - RemainingInstanceCount;
		if (FMath::RandRange(0.0f, 1.0f) < Remainder)
		{
			++RemainingInstanceCount;
		}

		GrassData.InstanceTransformsRelative.Empty(RemainingInstanceCount);
		GrassData.GrassVariety = Variety;

		while (RemainingInstanceCount > 0)
		{
			FVector GrassLocationRelative;
			GetRandomGrassLocation(VertexRelativeLocation, GrassLocationRelative);

			FRotator Rotation = SurfaceAlignmentRotation;
			// GetRandomGrassRotation(Variety, Rotation);

			FVector Scale;
			GetRandomGrassScale(Variety, Scale);

			FTransform InstanceTransformRelative(Rotation, GrassLocationRelative, Scale);
			GrassData.InstanceTransformsRelative.Add(InstanceTransformRelative);
			--RemainingInstanceCount;
		}
	}
}

void FGenerateAdditionalVertexDataWorker::GetRandomGrassRotation(const FGrassVariety& Variety,
                                                                 FRotator& OutRotation) const
{
	if (Variety.RandomRotation)
	{
		float RandomRotation = FMath::RandRange(-180.0f, 180.0f);
		OutRotation = FRotator(0.0f, RandomRotation, 0.0f);
	}
}

void FGenerateAdditionalVertexDataWorker::GetRandomGrassLocation(const FVector& VertexRelativeLocation,
                                                                 FVector& OutGrassLocation) const
{
	float PosX = FMath::RandRange(-0.5f, 0.5f);
	float PosY = FMath::RandRange(-0.5f, 0.5f);

	float SideLength = RebuildManager->CurrentComponent->GetParentLandscape()->GetQuadSideLength();
	OutGrassLocation = VertexRelativeLocation + FVector(PosX * SideLength, PosY * SideLength, 0.0f);
}

void FGenerateAdditionalVertexDataWorker::GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale) const
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

void FGenerateAdditionalVertexDataWorker::DoThreadedWork()
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
