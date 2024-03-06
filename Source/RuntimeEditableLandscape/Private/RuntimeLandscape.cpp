// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"

#include "Landscape.h"
#include "ProceduralMeshComponent.h"

ARuntimeLandscape::ARuntimeLandscape()
{
	LandscapeMesh = CreateDefaultSubobject<UProceduralMeshComponent>("Landscape mesh");
}

void ARuntimeLandscape::BeginPlay()
{
	Super::BeginPlay();
	GenerateMesh();
}

void ARuntimeLandscape::PrepareHeightValues(TArray<FSectionData>& OutSectionData) const
{
	switch (LandscapeMode)
	{
	case ELandscapeMode::LM_HeightMap:
		if (ReadHeightValuesFromHeightMap(OutSectionData))
		{
			return;
		}
		break;
	case ELandscapeMode::LM_CopyFromLandscape:
		if (ReadHeightValuesFromLandscape(OutSectionData))
		{
			return;
		}
		break;
	}

	const int32 VertexAmountPerSection = GetVertexAmountPerSection();
	for (FSectionData& Data : OutSectionData)
	{
		Data.HeightValues.Init(0, VertexAmountPerSection - Data.HeightValues.Num());
	}
}

bool ARuntimeLandscape::ReadHeightValuesFromLandscape(TArray<FSectionData>& OutSectionData) const
{
	TArray<float> LandscapeHeightData;
	int32 SizeX;
	int32 SizeY;
	LandscapeToCopyFrom->GetHeightValues(SizeX, SizeY, LandscapeHeightData);
	//
	const FVector2D SizePerSection = GetSizePerSection();
	const FBox ParentLandscapeBounds = LandscapeToCopyFrom->GetCompleteBounds();
	const FVector2D ParentLandscapeSize = FVector2D(ParentLandscapeBounds.Max.X - ParentLandscapeBounds.Min.X,
	                                                ParentLandscapeBounds.Max.Y - ParentLandscapeBounds.Min.Y);
	UE_LOG(LogTemp, Display, TEXT("LandscapeSize: %s"), *ParentLandscapeSize.ToString());
	// const FVector2D ParentToLocalOffset = FVector2D(GetActorLocation() - ParentLandscapeBounds.Min);
	// if (ParentToLocalOffset.X < 0 || ParentToLocalOffset.Y < 0)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("Runtime landscape '%s' is outside of it's parents bounds!"), *GetName());
	// 	return false;
	// }

	// const FVector2D WorldUnitsPerParentVertex = FVector2D(ParentLandscapeSize.X / SizeX, ParentLandscapeSize.Y / SizeY);
	const FIntVector2 SampleInterval = FIntVector2(
		SizeX / MeshResolution.X * (LandscapeSize.X / ParentLandscapeSize.X),
		SizeY / MeshResolution.Y * (LandscapeSize.Y / ParentLandscapeSize.Y));
	// const FVector2D SampleIntervalFloat = FVector2D(
	// 	SizeX / MeshResolution.X * (LandscapeSize.X / ParentLandscapeSize.X),
	// 	SizeY / MeshResolution.Y * (LandscapeSize.Y / ParentLandscapeSize.Y));

	// compensate sample error to avoid scaling issues
	// FVector2D SampleError = FVector2D(
	// 	SizeX / MeshResolution.X * (LandscapeSize.X / ParentLandscapeSize.X),
	// 	SizeY / MeshResolution.Y * (LandscapeSize.Y / ParentLandscapeSize.Y));
	// SampleError.X -= SampleInterval.X;
	// SampleError.Y -= SampleInterval.Y;

	UE_LOG(LogTemp, Display, TEXT("#"));
	UE_LOG(LogTemp, Display, TEXT("#"));
	UE_LOG(LogTemp, Display, TEXT("IntMAX: %i"), INT32_MAX);
	for (FSectionData& SectionData : OutSectionData)
	{
		FIntVector2 SectionCoordinates;
		GetSectionCoordinates(SectionData.SectionIndex, SectionCoordinates);

		float WorldOffsetX = GetActorLocation().X - LandscapeToCopyFrom->GetActorLocation().X + SectionCoordinates.X *
			SizePerSection.X;
		float WorldOffsetY = GetActorLocation().Y - LandscapeToCopyFrom->GetActorLocation().Y + SectionCoordinates.Y *
			SizePerSection.Y;

		int32 SectionStartOffsetX = SizeX / ParentLandscapeSize.X * WorldOffsetX;
		int32 SectionStartOffsetY = SizeY / ParentLandscapeSize.Y * WorldOffsetY;
		// calculate the section start offset in the height data
		// const FVector2D SectionRelativeOffset = SizePerSection * FVector2D(SectionCoordinates.X, SectionCoordinates.Y);
		// const int32 SectionStartOffsetX = FMath::FloorToInt(
		// 	(ParentToLocalOffset.X + SectionRelativeOffset.X) / WorldUnitsPerParentVertex.X);
		// const int32 SectionStartOffsetY = FMath::FloorToInt(
		// 	(ParentToLocalOffset.Y + SectionRelativeOffset.Y) / WorldUnitsPerParentVertex.Y * SizeX);

		FVector2D CurrentSampleError;
		// const float TEST = (ParentToLocalOffset.Y + SectionRelativeOffset.Y) / WorldUnitsPerParentVertex.Y;
		// UE_LOG(LogTemp, Display, TEXT("Section: %i"), SectionData.SectionIndex);
		// UE_LOG(LogTemp, Display, TEXT("		StartOffsetX: %i"), SectionStartOffsetX);
		// UE_LOG(LogTemp, Display, TEXT("		StartOffsetY: %i"), SectionStartOffsetY);
		UE_LOG(LogTemp, Display, TEXT("		SampleIntX: %i"), SampleInterval.X);
		UE_LOG(LogTemp, Display, TEXT("		SampleIntY: %i"), SampleInterval.Y);
		// UE_LOG(LogTemp, Display, TEXT("		SampleIntX: %f"), SampleIntervalFloat.X);
		// UE_LOG(LogTemp, Display, TEXT("		SampleIntY: %f"), SampleIntervalFloat.Y);
		// UE_LOG(LogTemp, Display, TEXT("		SampleErrorX: %f"), SampleError.X);
		// UE_LOG(LogTemp, Display, TEXT("		SampleErrorY: %f"), SampleError.Y);
		// UE_LOG(LogTemp, Display, TEXT("		TEST: %f"), TEST);

		int32 SampleCorrectionCountX = 0;
		for (int32 Y = 0; Y < GetVertexAmountPerSectionRow(); Y++)
		{
			for (int32 X = 0; X < GetVertexAmountPerSectionColumn(); X++)
			{
				// TODO: Remove if statement after finishing implementation
				// int32 VertexIndex = SectionStartOffsetX + SectionStartOffsetY + X * (SampleInterval.X + SampleError.X) + Y *
				// 	SizeX * (SampleInterval.Y + SampleError.Y);
				// int32 VertexIndexFloat = SectionStartOffsetX + SectionStartOffsetY + X * SampleIntervalFloat.X + Y *
				// 	SizeX * SampleIntervalFloat.Y;
				//
				// int32 VertexIndex = SectionStartOffsetX + SectionStartOffsetY + X * SampleInterval.X + Y *
				// 	SizeX * SampleInterval.Y;
				// const int32 VertexIndex = (SectionStartOffsetX + SectionStartOffsetY) + X * SampleInterval.X + Y *
				// 	SampleInterval.X * SizeX;
				const int32 VertexIndex = SectionStartOffsetY * SizeX + SectionStartOffsetX + X * SampleInterval.X +
					SizeX * SampleInterval.Y * Y;
				//UE_LOG(LogTemp, Display, TEXT("IndexDiff: %i, float: %i diff: %i"), VertexIndex, VertexIndexFloat, VertexIndexFloat - VertexIndex);
				//UE_LOG(LogTemp, Display, TEXT("		at: x %i, y %i section %i"), X, Y, SectionData.SectionIndex);
				if (LandscapeHeightData.IsValidIndex(VertexIndex))
				{
					// SectionData.HeightValues.Add(
					// LandscapeHeightData[SampleInterval.X * X + OffsetY + SectionStartOffsetX]);

					// compensate sample error
					// CurrentSampleError.X += SampleError.X;
					// if (CurrentSampleError.X > 1.0f)
					// {
					// 	VertexIndex++;
					// 	CurrentSampleError.X--;
					// 	SampleCorrectionCountX++;
					// 	UE_LOG(LogTemp, Display, TEXT("		Compensated sample error: %i at %i, %i"), SampleCorrectionCountX, X, Y);
					// }

					SectionData.HeightValues.Add(
						LandscapeHeightData[VertexIndex]);
				}
				else
				{
					SectionData.HeightValues.Add(0.0f);
					UE_LOG(LogTemp, Warning, TEXT("InvalidIndex %i!"), VertexIndex);
					UE_LOG(LogTemp, Warning, TEXT("X: %i, Y %i, SampleX: %i, SampleY: %i"), X, X, SampleInterval.X,
					       SampleInterval.Y);
					UE_LOG(LogTemp, Warning, TEXT("  StartOffsetX: %i StartOffsetY: %i"), SectionStartOffsetX,
					       SectionStartOffsetY);
					UE_LOG(LogTemp, Warning, TEXT("  MaxIndex: %i"), LandscapeHeightData.Num() - 1);
				}
			}

			// reset the sample error correction to ensure it is at the same column for each row
			CurrentSampleError.X = 0.0f;
		}
	}

	return true;
}

bool ARuntimeLandscape::ReadHeightValuesFromHeightMap(TArray<FSectionData>& OutSectionData) const
{
	UE_LOG(LogTemp, Warning, TEXT("ARuntimeLandscape::ReadHeightValuesFromHeightMap is not yet implemented!"));
	return false;
}

TArray<int32> ARuntimeLandscape::GetSectionsInArea(const FBox2D& Area) const
{
	const FVector2D SectionSize = GetSizePerSection();

	const int32 MinColumn = FMath::FloorToInt(Area.Min.X / SectionSize.X);
	const int32 MaxColumn = FMath::CeilToInt(Area.Max.X / SectionSize.X);

	const int32 MinRow = FMath::FloorToInt(Area.Min.Y / SectionSize.Y);
	const int32 MaxRow = FMath::CeilToInt(Area.Max.Y / SectionSize.Y);

	TArray<int32> Result;
	const int32 ExpectedAmount = (MaxColumn - MinColumn) * (MaxRow - MinRow);
	Result.Reserve(ExpectedAmount);

	for (int32 Y = MinRow; Y <= MaxRow; Y++)
	{
		const int32 RowOffset = Y * SectionAmount.X;
		for (int32 X = MinColumn; X <= MaxColumn; X++)
		{
			Result.Add(RowOffset + X);
		}
	}

	check(Result.Num() == ExpectedAmount);
	return Result;
}

void ARuntimeLandscape::GetSectionCoordinates(int32 SectionId, FIntVector2& OutCoordinateResult) const
{
	OutCoordinateResult.X = SectionId % FMath::RoundToInt(SectionAmount.X);
	OutCoordinateResult.Y = FMath::FloorToInt(SectionId / SectionAmount.X);
}

FBox2D ARuntimeLandscape::GetSectionBounds(int32 SectionIndex) const
{
	const FVector2D SectionSize = LandscapeSize / SectionAmount;
	FIntVector2 SectionCoordinates;
	GetSectionCoordinates(SectionIndex, SectionCoordinates);

	return FBox2D(
		FVector2D(SectionCoordinates.X * SectionSize.X, SectionCoordinates.Y * SectionSize.Y),
		FVector2D((SectionCoordinates.X + 1) * SectionSize.X, (SectionCoordinates.Y + 1) * SectionSize.Y));
}

//
// FBox2D ARuntimeLandscape::GetLandscapeBounds() const
// {
// 	const FVector2D LandscapeHorizontalLocation = FVector2D(GetActorLocation());
// 	return FBox2D(FVector2D(GetActorLocation()), LandscapeHorizontalLocation + LandscapeSize);
// }

void ARuntimeLandscape::GenerateMesh() const
{
	const int32 RequiredSectionAmount = SectionAmount.X * SectionAmount.Y;
	TArray<int32> SectionsToGenerate;
	SectionsToGenerate.Reserve(RequiredSectionAmount);

	for (int32 i = 0; i < RequiredSectionAmount; i++)
	{
		SectionsToGenerate.Add(i);
	}

	LandscapeMesh->ClearAllMeshSections();
	GenerateSections(SectionsToGenerate);
}

void ARuntimeLandscape::GenerateSections(const TArray<int32>& SectionsToGenerate) const
{
	TArray<FSectionData> SectionDataSets = TArray<FSectionData>();
	const int32 VertexAmountPerSection = GetVertexAmountPerSection();

	for (const int32 Section : SectionsToGenerate)
	{
		SectionDataSets.Add(FSectionData(Section));
	}

	PrepareHeightValues(SectionDataSets);

	const FVector2D SectionResolution = MeshResolution / SectionAmount;

	for (const FSectionData& SectionData : SectionDataSets)
	{
		// ensure the section data is valid
		if (!ensure(SectionData.HeightValues.Num() == VertexAmountPerSection))
		{
			UE_LOG(LogTemp, Warning, TEXT("Section %i could not generate valid data and will not be generated!"),
			       SectionData.SectionIndex);
			continue;
		}

		const FVector2D SectionOffset = GetSectionBounds(SectionData.SectionIndex).Min;
		TArray<FVector> Vertices;
		Vertices.Reserve(VertexAmountPerSection);
		TArray<int32> Triangles;
		Triangles.Reserve(SectionResolution.X * SectionResolution.Y * 2);

		const FVector2D VertexDistance = LandscapeSize / MeshResolution;


		int32 VertexIndex = 0;
		// generate first row of points
		for (uint32 X = 0; X <= SectionResolution.X; X++)
		{
			Vertices.Add(FVector(X * VertexDistance.X + SectionOffset.X, SectionOffset.Y,
			                     SectionData.HeightValues[VertexIndex]));
			VertexIndex++;
		}

		for (uint32 Y = 0; Y < SectionResolution.Y; Y++)
		{
			const float Y1 = Y + 1;
			Vertices.Add(FVector(SectionOffset.X, Y1 * VertexDistance.Y + SectionOffset.Y,
			                     SectionData.HeightValues[VertexIndex]));
			VertexIndex++;

			// generate triangle strip in X direction
			for (uint32 X = 0; X < SectionResolution.X; X++)
			{
				Vertices.Add(FVector((X + 1) * VertexDistance.X + SectionOffset.X,
				                     Y1 * VertexDistance.Y + SectionOffset.Y,
				                     SectionData.HeightValues[VertexIndex]));
				VertexIndex++;

				int32 T1 = Y * (SectionResolution.X + 1) + X;
				int32 T2 = T1 + SectionResolution.X + 1;
				int32 T3 = T1 + 1;

				// add upper-left triangle
				Triangles.Add(T1);
				Triangles.Add(T2);
				Triangles.Add(T3);

				// add lower-right triangle
				Triangles.Add(T3);
				Triangles.Add(T2);
				Triangles.Add(T2 + 1);
			}
		}

		// if The vertex amount is differs, something is wrong with the algorithm
		check(Vertices.Num() == VertexAmountPerSection);
		// if The triangle amount is differs, something is wrong with the algorithm
		check(Triangles.Num() == SectionResolution.X * SectionResolution.Y * 6);

		const TArray<FVector> Normals;
		const TArray<FVector2D> UV0;
		const TArray<FProcMeshTangent> Tangents;
		TArray<FColor> VertexColors;

#if WITH_EDITORONLY_DATA
		if (bEnableDebug && DebugMaterial)
		{
			LandscapeMesh->SetMaterial(SectionData.SectionIndex, DebugMaterial);
			FIntVector2 SectionCoordinates;
			GetSectionCoordinates(SectionData.SectionIndex, SectionCoordinates);
			const bool bIsEvenRow = SectionCoordinates.Y % 2 == 0;
			const bool bIsEvenColumn = SectionCoordinates.X % 2 == 0;
			const FColor SectionColor = bIsEvenColumn && bIsEvenRow || !bIsEvenColumn && !bIsEvenRow
				                            ? DebugColor1
				                            : DebugColor2;
			VertexColors.Init(SectionColor, Vertices.Num());
		}
#endif

		LandscapeMesh->CreateMeshSection(SectionData.SectionIndex, Vertices, Triangles, Normals, UV0, VertexColors,
		                                 Tangents,
		                                 false);
	}
}

#if WITH_EDITOR
void ARuntimeLandscape::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MeshResolution.X = FMath::RoundToInt(MeshResolution.X);
	MeshResolution.Y = FMath::RoundToInt(MeshResolution.Y);

	GenerateMesh();
}
#endif
