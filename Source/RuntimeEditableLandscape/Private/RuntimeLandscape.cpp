// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"
#include "ProceduralMeshComponent.h"

// Sets default values
ARuntimeLandscape::ARuntimeLandscape()
{
	LandscapeMesh = CreateDefaultSubobject<UProceduralMeshComponent>("Landscape mesh");
}
}

void ARuntimeLandscape::PrepareHeightValues(TArray<float>& OutHeightValues) const
{
	const int32 ExpectedValueCount = (MeshResolution.X + 1) * (MeshResolution.Y + 1);
	OutHeightValues.Reserve(ExpectedValueCount);
	for (int32 i = 0; i < ExpectedValueCount; i++)
	{
		float HeightValue = FMath::RandRange(0.0f, 300.0f);
		OutHeightValues.Add(HeightValue);
	}

	check(OutHeightValues.Num() == ExpectedValueCount);
}

// Called when the game starts or when spawned
void ARuntimeLandscape::BeginPlay()
{
	Super::BeginPlay();
	GenerateMesh();
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

void ARuntimeLandscape::GenerateMesh() const
{
	const int32 RequiredSectionAmount = SectionAmount.X * SectionAmount.Y;
	for (int32 i = 0; i < RequiredSectionAmount; i++)
	{
		GenerateSection(i);
	}
}

void ARuntimeLandscape::GenerateSection(int32 SectionIndex) const
{
	const FVector2D SectionResolution = MeshResolution / SectionAmount;
	const FVector2D SectionOffset = GetSectionBounds(SectionIndex).Min;
	
	TArray<FVector> Vertices;
	Vertices.Reserve((SectionResolution.X + 1) * (SectionResolution.Y + 1));
	TArray<int32> Triangles;
	Triangles.Reserve(SectionResolution.X * SectionResolution.Y * 2);

	const FVector2D VertexDistance = LandscapeSize / MeshResolution;

	TArray<float> HeightValues;
	PrepareHeightValues(HeightValues);

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
			Vertices.Add(FVector((X + 1) * VertexDistance.X + SectionOffset.X, Y1 * VertexDistance.Y + SectionOffset.Y, HeightValues[VertexIndex]));
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
	check(Vertices.Num() == (SectionResolution.X + 1) * (SectionResolution.Y + 1));
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

	LandscapeMesh->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);
}
