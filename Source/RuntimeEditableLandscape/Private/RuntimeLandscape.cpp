// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"

#include "Landscape.h"
#include "ProceduralMeshComponent.h"

// Sets default values
ARuntimeLandscape::ARuntimeLandscape()
{
	LandscapeMesh = CreateDefaultSubobject<UProceduralMeshComponent>("Landscape mesh");
}

void ARuntimeLandscape::BeginPlay()
{
	Super::BeginPlay();
	GenerateMesh();
}

}

void ARuntimeLandscape::PrepareHeightValues(TArray<float>& OutHeightValues) const
{
	const int32 ExpectedValueCount = (MeshResolution.X + 1) * (MeshResolution.Y + 1);
	OutHeightValues.Reserve(ExpectedValueCount);

	TArray<FColor> HeightColorData;
	switch (LandscapeMode)
	{
	case ELandscapeMode::LM_HeightMap:
		ReadHeightValuesFromHeightMap(HeightColorData);
		break;
	case ELandscapeMode::LM_CopyFromLandscape:
		//ReadHeightValuesFromLandscape(HeightColorData);
		ReadHeightValuesFromLandscape(OutHeightValues);
		return;
	}


	if (HeightColorData.Num() != ExpectedValueCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("Runtime landscape '%s' has incorrect height data size!"), *GetName());
		return;
	}

	for (int32 i = 0; i < ExpectedValueCount; i++)
	{
		const FColor& HeightColor = HeightColorData[i];
		float HeightValue = HeightColor.G * HeightScale;
		OutHeightValues.Add(HeightValue);
	}

	check(OutHeightValues.Num() == ExpectedValueCount);
}

void ARuntimeLandscape::ReadHeightValuesFromLandscape(TArray<float>& OutHeightData) const
{
	TArray<float> LandscapeHeightData;
	int32 SizeX;
	int32 SizeY;
	LandscapeToCopyFrom->GetHeightValues(SizeX, SizeY, LandscapeHeightData);
	const int32 SampleIntervalX = SizeX / MeshResolution.X;
	const int32 SampleIntervalY = SizeY / MeshResolution.Y;

	for (int32 Y = 0; Y < MeshResolution.Y + 1; Y++)
	{
		const int32 OffsetY = Y * SizeX * SampleIntervalY;
		for (int32 X = 0; X < MeshResolution.X + 1; X++)
		{
			// TODO: Remove if statement after finishing implementation
			if (LandscapeHeightData.IsValidIndex(SampleIntervalX * X + OffsetY))
			{
				OutHeightData.Add(LandscapeHeightData[SampleIntervalX * X + OffsetY]);
			}
			else
			{
				OutHeightData.Add(0.0f);
				ensure(false);
			}
		}
	}
}

void ARuntimeLandscape::ReadHeightValuesFromHeightMap(TArray<FColor>& OutHeightColorData) const
{
	UE_LOG(LogTemp, Warning, TEXT("ARuntimeLandscape::ReadHeightValuesFromHeightMap is not yet implemented!"));
	OutHeightColorData.Init(FColor::Black, (MeshResolution.X + 1) * (MeshResolution.Y + 1));
}

TArray<int32> ARuntimeLandscape::GetSectionsInArea(const FBox2D& Area) const
{
	const float SectionSizeX = LandscapeSize.X / SectionAmount.X;
	const float SectionSizeY = LandscapeSize.Y / SectionAmount.Y;

	const int32 MinColumn = FMath::FloorToInt(Area.Min.X / SectionSizeX);
	const int32 MaxColumn = FMath::CeilToInt(Area.Max.X / SectionSizeX);

	const int32 MinRow = FMath::FloorToInt(Area.Min.Y / SectionSizeY);
	const int32 MaxRow = FMath::CeilToInt(Area.Max.Y / SectionSizeY);

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

FBox2D ARuntimeLandscape::GetLandscapeBounds() const
{
	const FVector2D LandscapeHorizontalLocation = FVector2D(GetActorLocation());
	return FBox2D(FVector2D(GetActorLocation()), LandscapeHorizontalLocation + LandscapeSize);
}

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
	for (const int32 SectionIndex : SectionsToGenerate)
	{
		const FVector2D SectionResolution = MeshResolution / SectionAmount;
		const FVector2D SectionOffset = GetSectionBounds(SectionIndex).Min;
		const int32 ExpectedVertexAmount = (SectionResolution.X + 1) * (SectionResolution.Y + 1);
		TArray<FVector> Vertices;
		Vertices.Reserve(ExpectedVertexAmount);
		TArray<int32> Triangles;
		Triangles.Reserve(SectionResolution.X * SectionResolution.Y * 2);

		const FVector2D VertexDistance = LandscapeSize / MeshResolution;

		TArray<float> HeightValues;
		PrepareHeightValues(HeightValues);
		if (HeightValues.Num() < ExpectedVertexAmount)
		{
			HeightValues.Init(0, ExpectedVertexAmount - HeightValues.Num());
		}

		int32 VertexIndex = 0;
		// generate first row of points
		for (uint32 X = 0; X <= SectionResolution.X; X++)
		{
			Vertices.Add(FVector(X * VertexDistance.X + SectionOffset.X, SectionOffset.Y, HeightValues[VertexIndex]));
			VertexIndex++;
		}

		for (uint32 Y = 0; Y < SectionResolution.Y; Y++)
		{
			const float Y1 = Y + 1;
			Vertices.Add(FVector(SectionOffset.X, Y1 * VertexDistance.Y + SectionOffset.Y, HeightValues[VertexIndex]));
			VertexIndex++;

			// generate triangle strip in X direction
			for (uint32 X = 0; X < SectionResolution.X; X++)
			{
				Vertices.Add(FVector((X + 1) * VertexDistance.X + SectionOffset.X,
				                     Y1 * VertexDistance.Y + SectionOffset.Y,
				                     HeightValues[VertexIndex]));
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
		check(Vertices.Num() == ExpectedVertexAmount);
		// if The triangle amount is differs, something is wrong with the algorithm
		check(Triangles.Num() == SectionResolution.X * SectionResolution.Y * 6);

		const TArray<FVector> Normals;
		const TArray<FVector2D> UV0;
		const TArray<FProcMeshTangent> Tangents;
		TArray<FColor> VertexColors;

#if WITH_EDITORONLY_DATA
		if (bEnableDebug && DebugMaterial)
		{
			LandscapeMesh->SetMaterial(SectionIndex, DebugMaterial);
			FIntVector2 SectionCoordinates;
			GetSectionCoordinates(SectionIndex, SectionCoordinates);
			const bool bIsEvenRow = SectionCoordinates.Y % 2 == 0;
			const bool bIsEvenColumn = SectionCoordinates.X % 2 == 0;
			const FColor SectionColor = bIsEvenColumn && bIsEvenRow || !bIsEvenColumn && !bIsEvenRow
				                            ? DebugColor1
				                            : DebugColor2;
			VertexColors.Init(SectionColor, Vertices.Num());
		}
#endif

		LandscapeMesh->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UV0, VertexColors, Tangents,
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
