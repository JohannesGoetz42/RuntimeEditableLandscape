// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscapeComponent.h"

#include "KismetProceduralMeshLibrary.h"
#include "LandscapeLayerComponent.h"
#include "NavigationSystem.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscape.h"

void URuntimeLandscapeComponent::AddLandscapeLayer(const ULandscapeLayerComponent* Layer, bool bForceRebuild)
{
	AffectingLayers.Add(Layer);
	if (bForceRebuild)
	{
		Rebuild();
	}
	else
	{
		bIsStale = true;
	}
}

void URuntimeLandscapeComponent::Initialize(int32 ComponentIndex, const TArray<float>& HeightValues)
{
	ParentLandscape = Cast<ARuntimeLandscape>(GetOwner());
	if (ensure(ParentLandscape))
	{
		InitialHeightValues.SetNumUninitialized(HeightValues.Num());
		for (int32 i = 0; i < HeightValues.Num(); i++)
		{
			InitialHeightValues[i] = HeightValues[i] + ParentLandscape->GetParentHeight();
		}

		Index = ComponentIndex;
		Rebuild();
	}
}

FVector2D URuntimeLandscapeComponent::GetRelativeVertexLocation(int32 VertexIndex) const
{
	FIntVector2 Coordinates;
	ParentLandscape->GetVertexCoordinatesWithinComponent(VertexIndex, Coordinates);
	return FVector2D(Coordinates.X * ParentLandscape->GetQuadSideLength(),
	                 Coordinates.Y * ParentLandscape->GetQuadSideLength());
}

void URuntimeLandscapeComponent::AddHole(TObjectPtr<const ALandscapeHole> Hole)
{
	if (Holes.Contains(Hole))
	{
		return;
	}

	Holes.Add(Hole);
	int32 VertexIndex = 0;

	for (const FProcMeshVertex& Vertex : GetProcMeshSection(0)->ProcVertexBuffer)
	{
		if (Hole->IsLocationInside(Vertex.Position + GetComponentLocation()))
		{
			VerticesInHole.Add(VertexIndex);
		}
		VertexIndex++;
	}

	Rebuild();
}

void URuntimeLandscapeComponent::RemoveHole(TObjectPtr<const ALandscapeHole> Hole)
{
	if (Holes.Contains(Hole))
	{
		Holes.Remove(Hole);
		UpdateHoles();
	}
}

void URuntimeLandscapeComponent::UpdateHoles()
{
	if (VerticesInHole.IsEmpty() && Holes.IsEmpty())
	{
		return;
	}

	VerticesInHole.Empty();
	if (Holes.IsEmpty())
	{
		Rebuild();
		return;
	}

	int32 VertexIndex = 0;
	for (const FProcMeshVertex& Vertex : GetProcMeshSection(0)->ProcVertexBuffer)
	{
		for (const ALandscapeHole* Hole : Holes)
		{
			if (Hole->IsLocationInside(Vertex.Position + GetComponentLocation()))
			{
				VerticesInHole.Add(VertexIndex);
				break;
			}
		}

		VertexIndex++;
	}

	Rebuild();
}

void URuntimeLandscapeComponent::Rebuild()
{
	UE_LOG(RuntimeEditableLandscape, Display, TEXT("Rebuilding Landscape component %s %i..."), *GetOwner()->GetName(),
	       Index);

	const int32 VertexAmount = ParentLandscape->GetTotalVertexAmountPerComponent();
	// ensure the section data is valid
	if (!ensure(InitialHeightValues.Num() == VertexAmount))
	{
		UE_LOG(RuntimeEditableLandscape, Warning,
		       TEXT("Component %i could not generate valid data and will not be generated!"),
		       Index);
		return;
	}

	const FVector2D ComponentResolution = ParentLandscape->GetComponentResolution();
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	UV0.Reserve(VertexAmount);
	TArray<FProcMeshTangent> Tangents;
	TArray<float> HeightValues = InitialHeightValues;
	TArray<FColor> VertexColors;

	ApplyDataFromLayers(HeightValues, VertexColors);

	TArray<FVector> Vertices;
	Vertices.Reserve(VertexAmount);
	TArray<int32> Triangles;
	Triangles.Reserve(ComponentResolution.X * ComponentResolution.Y * 2);

	const float VertexDistance = ParentLandscape->GetQuadSideLength();
	int32 VertexIndex = 0;
	const float UVIncrement = 1 / ComponentResolution.X;
	// generate first row of points
	for (int32 X = 0; X <= ComponentResolution.X; X++)
	{
		Vertices.Add(FVector(X * VertexDistance, 0, HeightValues[VertexIndex] - ParentLandscape->GetParentHeight()));
		UV0.Add(FVector2D(X * UVIncrement, 0));
		VertexIndex++;
	}

	for (int32 Y = 0; Y < ComponentResolution.Y; Y++)
	{
		const float Y1 = Y + 1;
		Vertices.Add(FVector(0, Y1 * VertexDistance, HeightValues[VertexIndex] - ParentLandscape->GetParentHeight()));
		UV0.Add(FVector2D(0, Y1 * UVIncrement));
		VertexIndex++;

		// generate triangle strip in X direction
		for (int32 X = 0; X < ComponentResolution.X; X++)
		{
			Vertices.Add(FVector((X + 1) * VertexDistance, Y1 * VertexDistance,
			                     HeightValues[VertexIndex] - ParentLandscape->GetParentHeight()));
			UV0.Add(FVector2D((X + 1) * UVIncrement, Y1 * UVIncrement));
			VertexIndex++;

			int32 T1 = Y * (ComponentResolution.X + 1) + X;
			int32 T2 = T1 + ComponentResolution.X + 1;
			int32 T3 = T1 + 1;

			if (VerticesInHole.Contains(T1) || VerticesInHole.Contains(T2) || VerticesInHole.Contains(T3) ||
				VerticesInHole.
				Contains(T2 + 1))
			{
				continue;
			}

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
	check(Vertices.Num() == VertexAmount);
	// if The triangle amount is differs, something is wrong with the algorithm
	//check(Triangles.Num() == SectionResolution.X * SectionResolution.Y * 6);

#if WITH_EDITORONLY_DATA
	if (ParentLandscape->bEnableDebug && ParentLandscape->DebugMaterial)
	{
		CleanUpOverrideMaterials();
		SetMaterial(0, ParentLandscape->DebugMaterial);

		if (ParentLandscape->bDrawDebugCheckerBoard || ParentLandscape->bDrawIndexGreyscales)
		{
			FIntVector2 SectionCoordinates;
			ParentLandscape->GetComponentCoordinates(Index, SectionCoordinates);
			const bool bIsEvenRow = SectionCoordinates.Y % 2 == 0;
			const bool bIsEvenColumn = SectionCoordinates.X % 2 == 0;
			FColor SectionColor = FColor::White;
			if (ParentLandscape->bDrawDebugCheckerBoard)
			{
				SectionColor = (bIsEvenColumn && bIsEvenRow) || (!bIsEvenColumn && !bIsEvenRow)
					               ? ParentLandscape->DebugColor1
					               : ParentLandscape->DebugColor2;
				if (ParentLandscape->bShowComponentsWithHole && !Holes.IsEmpty())
				{
					SectionColor = FColor::Red;
				}
			}
			else if (ParentLandscape->bDrawIndexGreyscales)
			{
				const float Factor = Index / (ParentLandscape->GetComponentAmount().X * ParentLandscape->
					GetComponentAmount().Y);
				SectionColor = FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor::Black, Factor).
					ToFColor(false);
			}
			VertexColors.Init(SectionColor, Vertices.Num());
		}
	}
#endif

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UV0, Normals, Tangents);
	CreateMeshSection(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, ParentLandscape->bUpdateCollision);
	UpdateNavigation();

	bIsStale = false;
}

void URuntimeLandscapeComponent::ApplyDataFromLayers(TArray<float>& OutHeightValues, TArray<FColor>& OutVertexColors)
{
	check(OutHeightValues.Num() == InitialHeightValues.Num());

	OutVertexColors.Init(FColor::White, InitialHeightValues.Num());
	for (const ULandscapeLayerComponent* Layer : AffectingLayers)
	{
		for (int32 i = 0; i < InitialHeightValues.Num(); i++)
		{
			Layer->ApplyLayerData(GetRelativeVertexLocation(i) + FVector2D(GetComponentLocation()), OutHeightValues[i],
			                      OutVertexColors[i]);
		}
	}
}

void URuntimeLandscapeComponent::UpdateNavigation()
{
	if (ParentLandscape->bUpdateNavigation)
	{
		if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			NavSys->UpdateComponentInNavOctree(*this);
		}
	}
}
