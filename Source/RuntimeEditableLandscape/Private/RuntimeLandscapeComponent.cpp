// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscapeComponent.h"

#include "KismetProceduralMeshLibrary.h"
#include "LandscapeGrassType.h"
#include "LandscapeLayerComponent.h"
#include "NavigationSystem.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscape.h"
#include "LayerTypes/LandscapeHoleLayerData.h"
#include "LayerTypes/LandscapeLayerDataBase.h"
#include "Runtime/Foliage/Public/InstancedFoliageActor.h"

void URuntimeLandscapeComponent::AddLandscapeLayer(const ULandscapeLayerComponent* Layer)
{
	AffectingLayers.Add(Layer);
	Rebuild();
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

void URuntimeLandscapeComponent::AddVertex(TArray<FVector>& OutVertices, const FVector& VertexLocation, int32 X,
                                           int32 Y)
{
	OutVertices.Add(VertexLocation);
	SetGrassForVertex(VertexLocation, X, Y);
}

void URuntimeLandscapeComponent::GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale)
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

UHierarchicalInstancedStaticMeshComponent* URuntimeLandscapeComponent::FindOrAddGrassMesh(const FGrassVariety& Variety)
{
	UHierarchicalInstancedStaticMeshComponent** Mesh = GrassMeshes.FindByPredicate(
		[Variety](const UHierarchicalInstancedStaticMeshComponent* Current)
		{
			return Current->GetStaticMesh() == Variety.GrassMesh;
		});

	if (Mesh)
	{
		return *Mesh;
	}

	UHierarchicalInstancedStaticMeshComponent* InstancedStaticMesh = NewObject<
		UHierarchicalInstancedStaticMeshComponent>(GetOwner());
	InstancedStaticMesh->SetStaticMesh(Variety.GrassMesh);
	InstancedStaticMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
	InstancedStaticMesh->RegisterComponent();
	InstancedStaticMesh->SetCullDistances(Variety.GetStartCullDistance(), Variety.GetEndCullDistance());
	GrassMeshes.Add(InstancedStaticMesh);
	return InstancedStaticMesh;
}

void URuntimeLandscapeComponent::GetRandomGrassLocation(const FVector& VertexRelativeLocation,
                                                        FVector& OutGrassLocation) const
{
	float PosX = FMath::RandRange(-0.5f, 0.5f);
	float PosY = FMath::RandRange(-0.5f, 0.5f);

	float SideLength = ParentLandscape->GetQuadSideLength();
	OutGrassLocation = VertexRelativeLocation + FVector(PosX * SideLength, PosY * SideLength, 0.0f);
}

void URuntimeLandscapeComponent::GetRandomGrassRotation(const FGrassVariety& Variety, FRotator& OutRotation)
{
	if (Variety.RandomRotation)
	{
		float RandomRotation = FMath::RandRange(-180.0f, 180.0f);
		OutRotation = FRotator(0.0f, RandomRotation, 0.0f);
	}
}

void URuntimeLandscapeComponent::UpdateGrassAtVertex(const ULandscapeGrassType* SelectedGrass,
                                                     const FVector& VertexRelativeLocation,
                                                     float Weight)
{
	for (const FGrassVariety& Variety : SelectedGrass->GrassVarieties)
	{
		UHierarchicalInstancedStaticMeshComponent* InstancedStaticMesh = FindOrAddGrassMesh(Variety);

		int32 RemainingInstanceCount = FMath::RoundToInt32(
			ParentLandscape->GetAreaPerSquare() * Variety.GetDensity() * 0.000001f * Weight);
		while (RemainingInstanceCount > 0)
		{
			FVector GrassLocation;
			GetRandomGrassLocation(VertexRelativeLocation, GrassLocation);

			FRotator Rotation;
			GetRandomGrassRotation(Variety, Rotation);

			FVector Scale;
			GetRandomGrassScale(Variety, Scale);

			FTransform InstanceTransform(Rotation, GrassLocation, Scale);
			InstancedStaticMesh->AddInstance(InstanceTransform);

			--RemainingInstanceCount;
		}
	}
}

void URuntimeLandscapeComponent::SetGrassForVertex(const FVector& VertexLocation, int32 X, int32 Y)
{
	const ULandscapeGrassType* SelectedGrass = nullptr;
	float HighestWeight = 0;

	bool bIsLayerApplied = false;
	for (const auto& LayerWeightData : ParentLandscape->GetGroundTypeLayerWeightsAtVertexCoordinates(Index, X, Y))
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
		const float VertexHeight = GetComponentLocation().Z + VertexLocation.Z;
		for (const FHeightBasedLandscapeData& HeightBasedData : ParentLandscape->GetHeightBasedData())
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
		UpdateGrassAtVertex(SelectedGrass, VertexLocation, HighestWeight);
	}
}

void URuntimeLandscapeComponent::Rebuild()
{
	if (bIsStale)
	{
		return;
	}

	bIsStale = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]
	{
		ExecuteRebuild();
	});
}

void URuntimeLandscapeComponent::ExecuteRebuild()
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

	FIntVector2 SectionCoordinates;
	ParentLandscape->GetComponentCoordinates(Index, SectionCoordinates);

	const FVector2D ComponentResolution = ParentLandscape->GetComponentResolution();

	const FVector2D UV1Scale = FVector2D::One() / ParentLandscape->GetComponentAmount();
	const FVector2D UV1Offset = UV1Scale * FVector2D(SectionCoordinates.X, SectionCoordinates.Y);

	TArray<FVector> Normals;
	TArray<FVector2D> UV0Coords;
	TArray<FVector2D> UV1Coords;
	UV0Coords.Reserve(VertexAmount);
	UV1Coords.Reserve(VertexAmount);
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
		AddVertex(Vertices, FVector(X * VertexDistance, 0,
		                            HeightValues[VertexIndex] - ParentLandscape->GetParentHeight()), X, 0);
		const FVector2D UV0 = FVector2D(X * UVIncrement, 0);
		UV0Coords.Add(UV0);
		UV1Coords.Add(UV0 * UV1Scale + UV1Offset);
		VertexIndex++;
	}

	for (int32 Y = 0; Y < ComponentResolution.Y; Y++)
	{
		UE_LOG(RuntimeEditableLandscape, Display, TEXT("	Y: %i/%i..."), Y, static_cast<int32>(ComponentResolution.Y));
		const float Y1 = Y + 1;
		AddVertex(Vertices, FVector(0, Y1 * VertexDistance,
		                            HeightValues[VertexIndex] - ParentLandscape->GetParentHeight()), 0, Y);

		FVector2D UV0 = FVector2D(0, Y1 * UVIncrement);
		UV0Coords.Add(UV0);
		UV1Coords.Add(UV0 * UV1Scale + UV1Offset);
		VertexIndex++;

		// generate triangle strip in X direction
		for (int32 X = 0; X < ComponentResolution.X; X++)
		{
			FVector VertexLocation = FVector((X + 1) * VertexDistance, Y1 * VertexDistance,
			                                 HeightValues[VertexIndex] - ParentLandscape->GetParentHeight());
			AddVertex(Vertices, VertexLocation, X, Y);

			UV0 = FVector2D((X + 1) * UVIncrement, Y1 * UVIncrement);
			UV0Coords.Add(UV0);
			UV1Coords.Add(UV0 * UV1Scale + UV1Offset);

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

	// if The vertex amount differs, something is wrong with the algorithm
	check(Vertices.Num() == VertexAmount);
	// if The triangle amount differs, something is wrong with the algorithm
	//check(Triangles.Num() == SectionResolution.X * SectionResolution.Y * 6);

#if WITH_EDITORONLY_DATA
	if (ParentLandscape->bEnableDebug && ParentLandscape->DebugMaterial)
	{
		CleanUpOverrideMaterials();
		SetMaterial(0, ParentLandscape->DebugMaterial);

		if (ParentLandscape->bDrawDebugCheckerBoard || ParentLandscape->bDrawIndexGreyScales)
		{
			const bool bIsEvenRow = SectionCoordinates.Y % 2 == 0;
			const bool bIsEvenColumn = SectionCoordinates.X % 2 == 0;
			FColor SectionColor = FColor::White;
			if (ParentLandscape->bDrawDebugCheckerBoard)
			{
				SectionColor = (bIsEvenColumn && bIsEvenRow) || (!bIsEvenColumn && !bIsEvenRow)
					               ? ParentLandscape->DebugColor1
					               : ParentLandscape->DebugColor2;
				if (ParentLandscape->bShowComponentsWithHole)
				{
					for (const ULandscapeLayerComponent* Layer : AffectingLayers)
					{
						for (const ULandscapeLayerDataBase* LayerData : Layer->GetLayerData())
						{
							if (LayerData->IsA<ULandscapeHoleLayerData>())
							{
								SectionColor = FColor::Red;
								break;
							}
						}
					}
				}
			}
			else if (ParentLandscape->bDrawIndexGreyScales)
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

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UV0Coords, Normals, Tangents);
	CreateMeshSection(0, Vertices, Triangles, Normals, UV0Coords, UV1Coords, UV0Coords, UV0Coords, VertexColors,
	                  Tangents,
	                  ParentLandscape->bUpdateCollision);
	RemoveFoliageAffectedByLayer();
	UpdateNavigation();

	bIsStale = false;
}

void URuntimeLandscapeComponent::ApplyDataFromLayers(TArray<float>& OutHeightValues, TArray<FColor>& OutVertexColors)
{
	check(OutHeightValues.Num() == InitialHeightValues.Num());

	VerticesInHole.Empty();
	OutVertexColors.Init(FColor::White, InitialHeightValues.Num());
	for (const ULandscapeLayerComponent* Layer : AffectingLayers)
	{
		for (int32 i = 0; i < InitialHeightValues.Num(); i++)
		{
			Layer->ApplyLayerData(i, this, OutHeightValues[i],
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

void URuntimeLandscapeComponent::RemoveFoliageAffectedByLayer() const
{
	const AInstancedFoliageActor* Foliage = ParentLandscape->GetFoliageActor();
	if (!Foliage)
	{
		return;
	}

	FBox MyBounds = GetLocalBounds().GetBox();
	MyBounds = MyBounds.MoveTo(GetComponentLocation() + MyBounds.GetExtent());
	MyBounds = MyBounds.ExpandBy(FVector(0.0f, 0.0f, 10000.0f));

	for (const auto& FoliageInfo : Foliage->GetFoliageInfos())
	{
		TArray<int32> Instances;
		UHierarchicalInstancedStaticMeshComponent* FoliageComp = FoliageInfo.Value->GetComponent();

		TArray<int32> FoliageToRemove;
		for (int32 Instance : FoliageComp->GetInstancesOverlappingBox(MyBounds))
		{
			FTransform InstanceTransform;
			FoliageComp->GetInstanceTransform(Instance, InstanceTransform, true);
			FVector2D InstanceLocation = FVector2D(InstanceTransform.GetLocation());

			for (const ULandscapeLayerComponent* Layer : AffectingLayers)
			{
				if (Layer->IsAffectedByLayer(InstanceLocation))
				{
					FoliageToRemove.Add(Instance);
					break;
				}
			}
		}

		FoliageComp->RemoveInstances(FoliageToRemove);
	}
}

void URuntimeLandscapeComponent::DestroyComponent(bool bPromoteChildren)
{
	for (UHierarchicalInstancedStaticMeshComponent* GrassMesh : GrassMeshes)
	{
		GrassMesh->DestroyComponent();
	}

	Super::DestroyComponent(bPromoteChildren);
}
