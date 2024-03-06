// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscapeComponent.h"

#include "LandscapeLayerComponent.h"
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
		InitialHeightValues = HeightValues;
		Index = ComponentIndex;
		Rebuild(true);
	}
}

void URuntimeLandscapeComponent::Rebuild(bool bUpdateCollision)
{
	const int32 VertexAmount = ParentLandscape->GetVertexAmountPerSection();
	// ensure the section data is valid
	if (!ensure(InitialHeightValues.Num() == VertexAmount))
	{
		UE_LOG(LogTemp, Warning, TEXT("Section %i could not generate valid data and will not be generated!"),
		       Index);
		return;
	}

	const FVector2D SectionResolution = ParentLandscape->GetMeshResolution() / ParentLandscape->
		GetComponentAmount();
	const TArray<FVector> Normals;
	const TArray<FVector2D> UV0;
	const TArray<FProcMeshTangent> Tangents;
	TArray<float> HeightValues = InitialHeightValues;
	TArray<FColor> VertexColors;

	GenerateDataFromLayers(HeightValues, VertexColors);

	TArray<FVector> Vertices;
	Vertices.Reserve(VertexAmount);
	TArray<int32> Triangles;
	Triangles.Reserve(SectionResolution.X * SectionResolution.Y * 2);

	const FVector2D VertexDistance = ParentLandscape->GetLandscapeSize() / ParentLandscape->GetMeshResolution();
	int32 VertexIndex = 0;
	// generate first row of points
	for (int32 X = 0; X <= SectionResolution.X; X++)
	{
		Vertices.Add(FVector(X * VertexDistance.X, 0, HeightValues[VertexIndex]));
		VertexIndex++;
	}

	for (int32 Y = 0; Y < SectionResolution.Y; Y++)
	{
		const float Y1 = Y + 1;
		Vertices.Add(FVector(0, Y1 * VertexDistance.Y, HeightValues[VertexIndex]));
		VertexIndex++;

		// generate triangle strip in X direction
		for (int32 X = 0; X < SectionResolution.X; X++)
		{
			Vertices.Add(FVector((X + 1) * VertexDistance.X, Y1 * VertexDistance.Y, HeightValues[VertexIndex]));
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
	check(Vertices.Num() == VertexAmount);
	// if The triangle amount is differs, something is wrong with the algorithm
	check(Triangles.Num() == SectionResolution.X * SectionResolution.Y * 6);

#if WITH_EDITORONLY_DATA
	if (ParentLandscape->bEnableDebug && ParentLandscape->DebugMaterial)
	{
		CleanUpOverrideMaterials();
		SetMaterial(0, ParentLandscape->DebugMaterial);

		if (ParentLandscape->bDrawDebugCheckerBoard)
		{
			FIntVector2 SectionCoordinates;
			ParentLandscape->GetComponentCoordinates(Index, SectionCoordinates);
			const bool bIsEvenRow = SectionCoordinates.Y % 2 == 0;
			const bool bIsEvenColumn = SectionCoordinates.X % 2 == 0;
			const FColor SectionColor = bIsEvenColumn && bIsEvenRow || !bIsEvenColumn && !bIsEvenRow
				                            ? ParentLandscape->DebugColor1
				                            : ParentLandscape->DebugColor2;
			VertexColors.Init(SectionColor, Vertices.Num());
		}
	}
#endif

	CreateMeshSection(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, bUpdateCollision);
	bIsStale = false;
}

void URuntimeLandscapeComponent::GenerateDataFromLayers(TArray<float>& OutHeightValues, TArray<FColor>& OutVertexColors)
{
	check(OutHeightValues.Num() == InitialHeightValues.Num());

	OutVertexColors.Init(FColor::White, InitialHeightValues.Num());
	for (const ULandscapeLayerComponent* Layer : AffectingLayers)
	{
		for (int32 i = 0; i < InitialHeightValues.Num(); i++)
		{
			// check if the vertex is inside layer bounds			
			if (Layer->GetAffectedArea().IsInside(ParentLandscape->GetVertexLocation(Index, i)))
			{
				if (Layer->LayerData.bApplyHeight)
				{
					OutHeightValues[i] = Layer->LayerData.HeightValue;
				}

				if (Layer->LayerData.bApplyVertexColor)
				{
					OutVertexColors[i] = Layer->LayerData.VertexColor;
				}
			}
		}
	}
}
