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
	Vertices.Reserve((MeshResolution.X + 1) * (MeshResolution.Y + 1));
	TArray<int32> Triangles;
	Triangles.Reserve(MeshResolution.X * MeshResolution.Y * 2);

	const FVector2D VertexDistance = LandscapeSize / MeshResolution;

	// generate first row of points
	for (uint32 X = 0; X <= MeshResolution.X; X++)
	{
		Vertices.Add(FVector(X * VertexDistance.X, 0, 0.0f));
	}

	for (uint32 Y = 0; Y < MeshResolution.Y; Y++)
	{
		const float Y1 = Y + 1;
		Vertices.Add(FVector(0, Y1 * VertexDistance.Y, 0.0f));

		// generate triangle strip in X direction
		for (uint32 X = 0; X < MeshResolution.X; X++)
		{
			Vertices.Add(FVector((X + 1) * VertexDistance.X, Y1 * VertexDistance.Y, 0.0f));

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
