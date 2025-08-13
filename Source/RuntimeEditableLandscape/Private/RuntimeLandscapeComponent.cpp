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
#include "Threads/GenerateVertexRowDataThread.h"

void URuntimeLandscapeComponent::AddLandscapeLayer(const ULandscapeLayerComponent* Layer)
{
	AffectingLayers.Add(Layer);
	Rebuild();
}

void URuntimeLandscapeComponent::Initialize(int32 ComponentIndex, const TArray<float>& HeightValuesInitial)
{
	ParentLandscape = Cast<ARuntimeLandscape>(GetOwner());
	if (ensure(ParentLandscape))
	{
		InitialHeightValues.SetNumUninitialized(HeightValuesInitial.Num());
		for (int32 i = 0; i < HeightValuesInitial.Num(); i++)
		{
			InitialHeightValues[i] = HeightValuesInitial[i] + ParentLandscape->GetParentHeight();
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
	InstancedStaticMesh->SetCastShadow(Variety.bCastDynamicShadow);
	InstancedStaticMesh->SetCastContactShadow(Variety.bCastContactShadow);

	GrassMeshes.Add(InstancedStaticMesh);
	return InstancedStaticMesh;
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
	const float VertexDistance = ParentLandscape->GetQuadSideLength();
	const float UVIncrement = 1 / ComponentResolution.X;

	HeightValues = InitialHeightValues;

	// TODO: Clean up. Do I need vertex colors?
	TArray<FColor> VertexColors;
	ApplyDataFromLayers(HeightValues, VertexColors);

	int32 VertexIndex = 0;
	// Start data generation threads
	ActiveGenerationThreads = ComponentResolution.Y + 1;
	for (int32 Y = -1; Y < ComponentResolution.Y; Y++)
	{
		UE_LOG(RuntimeEditableLandscape, Log, TEXT("Starting generation thread %s %i"), *GetName(), Y);
		auto* Thread = new FGenerateVertexRowDataThread(
			this, ComponentResolution, UV1Scale, UV1Offset, VertexDistance, UVIncrement, VertexIndex, Y);
		VertexIndex += ComponentResolution.X + 1;
		GenerationThreads.Add(Thread);
	}

	FTimerHandle _;
	GetWorld()->GetTimerManager().SetTimer(_, this, &URuntimeLandscapeComponent::CheckThreadsFinished, 0.5f, true);
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

void URuntimeLandscapeComponent::CheckThreadsFinished()
{
	UE_LOG(RuntimeEditableLandscape, Display, TEXT("Remaining threads: %i"), ActiveGenerationThreads);
	if (ActiveGenerationThreads < 1)
	{
		FinishRebuild();
	}
}

void URuntimeLandscapeComponent::FinishRebuild()
{
	// TODO: Reuse stuff instead of recreating everything
	// clean up old stuff
	for (UHierarchicalInstancedStaticMeshComponent* GrassMesh : GrassMeshes)
	{
		if (ensure(GrassMesh))
		{
			GrassMesh->DestroyComponent();
		}
	}
	GrassMeshes.Empty();

	UE_LOG(RuntimeEditableLandscape, Display, TEXT("Finished all generation threads. Merging and applying data..."));

	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

	const int32 VertexAmount = ParentLandscape->GetTotalVertexAmountPerComponent();
	const FVector2D ComponentResolution = ParentLandscape->GetComponentResolution();

	TArray<FVector> Vertices;
	TArray<FVector2D> UV0Coords;
	TArray<FVector2D> UV1Coords;

	Vertices.Reserve(VertexAmount);
	UV0Coords.Reserve(VertexAmount);
	UV1Coords.Reserve(VertexAmount);

	TArray<int32> Triangles;
	Triangles.Reserve(ComponentResolution.X * ComponentResolution.Y * 2);

	// merge results from threads
	for (FGenerateVertexRowDataThread* Thread : GenerationThreads)
	{
		FLandscapeVertexData& VertexData = Thread->VertexData;
		Vertices.Append(MoveTemp(VertexData.Vertices));
		Triangles.Append(MoveTemp(VertexData.Triangles));

		UV0Coords.Append(MoveTemp(VertexData.UV0Coords));
		UV1Coords.Append(MoveTemp(VertexData.UV1Coords));

		for (const FLandscapeGrassVertexData& GrassData : VertexData.GrassData)
		{
			UHierarchicalInstancedStaticMeshComponent* GrassMesh = FindOrAddGrassMesh(GrassData.GrassVariety);
			GrassMesh->AddInstances(GrassData.InstanceTransforms, false);
		}
	}

	GenerationThreads.Empty();

	// if The vertex amount differs, something is wrong with the algorithm
	check(Vertices.Num() == VertexAmount);
	// if The triangle amount differs, something is wrong with the algorithm
	//check(Triangles.Num() == SectionResolution.X * SectionResolution.Y * 6);

#if WITH_EDITORONLY_DATA

	FIntVector2 SectionCoordinates;
	ParentLandscape->GetComponentCoordinates(Index, SectionCoordinates);

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
		}
	}
#endif

	TArray<FColor> VertexColors;
	VertexColors.Init(FColor::White, Vertices.Num());

	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UV0Coords, Normals, Tangents);

	CreateMeshSection(0, Vertices, Triangles, Normals, UV0Coords, UV1Coords, UV0Coords, UV0Coords, VertexColors,
	                  Tangents,
	                  ParentLandscape->bUpdateCollision);

	RemoveFoliageAffectedByLayer();
	UpdateNavigation();

	bIsStale = false;
}

void URuntimeLandscapeComponent::DestroyComponent(bool bPromoteChildren)
{
	for (UHierarchicalInstancedStaticMeshComponent* GrassMesh : GrassMeshes)
	{
		GrassMesh->DestroyComponent();
	}

	Super::DestroyComponent(bPromoteChildren);
}
