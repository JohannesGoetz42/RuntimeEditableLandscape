// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"
#include "ProceduralMeshComponent.h"

// Sets default values
ARuntimeLandscape::ARuntimeLandscape()
{
	LandscapeMesh = CreateDefaultSubobject<UProceduralMeshComponent>("Landscape mesh");
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

void ARuntimeLandscape::GenerateMesh() const
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	Vertices.Reserve(2 * MeshResolution.X * MeshResolution.Y);

	const int32 CurrentRow = 0;
	const FVector2D VertexDistance = LandscapeSize / MeshResolution;
	int32 TriangleCount = 0;

	// generate first two points
	Vertices.Add(FVector(0, 0, 0.0f));
	Vertices.Add(FVector(0, VertexDistance.Y, 0.0f));

	// generate mesh columns
	for (uint32 X = 0; X < MeshResolution.X; X++)
	{
		Vertices.Add(FVector((X + 1) * VertexDistance.X, CurrentRow * VertexDistance.Y, 0.0f));
		// add triangles counter clockwise for correct normals
		Triangles.Add(TriangleCount);
		Triangles.Add(TriangleCount + 1);
		Triangles.Add(TriangleCount + 2);
		TriangleCount++;

		Vertices.Add(FVector((X + 1) * VertexDistance.X, (CurrentRow + 1) * VertexDistance.Y, 0.0f));
		// add triangles counter clockwise for correct normals
		Triangles.Add(TriangleCount);
		Triangles.Add(TriangleCount + 2);
		Triangles.Add(TriangleCount + 1);
		TriangleCount++;
	}

	// if The vertex amount is differs, something is wrong with the algorithm
	check(Vertices.Num() == 2 * MeshResolution.X + 2);
	
	const TArray<FVector> Normals;
	const TArray<FVector2D> UV0;
	const TArray<FColor> VertexColors;
	const TArray<FProcMeshTangent> Tangents;
	LandscapeMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);
}
