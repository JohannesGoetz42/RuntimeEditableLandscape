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

	if (MeshGenerationAffectingProperties.Contains(PropertyChangedEvent.MemberProperty->GetName()))
	{
		GenerateMesh();
	}
}
#endif

TArray<int32> ARuntimeLandscape::GetSectionIdsInArea(const FBox2D& Area) const
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

void ARuntimeLandscape::GenerateMesh() const
{
	TArray<FVector> Vertices;
	Vertices.Reserve((MeshResolution.X + 1) * (MeshResolution.Y + 1));
	TArray<int32> Triangles;
	Triangles.Reserve(MeshResolution.X * MeshResolution.Y * 2);

	const FVector2D VertexDistance = LandscapeSize / MeshResolution;

	TArray<float> HeightValues;
	PrepareHeightValues(HeightValues);

	int32 VertexIndex = 0;
	// generate first row of points
	for (uint32 X = 0; X <= MeshResolution.X; X++)
	{
		Vertices.Add(FVector(X * VertexDistance.X, 0, HeightValues[VertexIndex]));
		VertexIndex++;
	}

	for (uint32 Y = 0; Y < MeshResolution.Y; Y++)
	{
		const float Y1 = Y + 1;
		Vertices.Add(FVector(0, Y1 * VertexDistance.Y, HeightValues[VertexIndex]));
		VertexIndex++;

		// generate triangle strip in X direction
		for (uint32 X = 0; X < MeshResolution.X; X++)
		{
			Vertices.Add(FVector((X + 1) * VertexDistance.X, Y1 * VertexDistance.Y, HeightValues[VertexIndex]));
			VertexIndex++;

			int32 T1 = Y * (MeshResolution.X + 1) + X;
			int32 T2 = T1 + MeshResolution.X + 1;
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
	check(Vertices.Num() == (MeshResolution.X + 1) * (MeshResolution.Y + 1));
	// if The triangle amount is differs, something is wrong with the algorithm
	check(Triangles.Num() == MeshResolution.X * MeshResolution.Y * 6);

	const TArray<FVector> Normals;
	const TArray<FVector2D> UV0;
	const TArray<FColor> VertexColors;
	const TArray<FProcMeshTangent> Tangents;
	LandscapeMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);
}
