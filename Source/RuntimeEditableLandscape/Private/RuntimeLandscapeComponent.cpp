// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscapeComponent.h"

#include "LandscapeGrassType.h"
#include "LandscapeLayerComponent.h"
#include "NavigationSystem.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscape.h"
#include "RuntimeLandscapeSubcomponent.h"
#include "LayerTypes/LandscapeHoleLayerData.h"
#include "LayerTypes/LandscapeLayerDataBase.h"
#include "Runtime/Foliage/Public/InstancedFoliageActor.h"
#include "Threads/RuntimeLandscapeRebuildManager.h"

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
	InstancedStaticMesh->SetRenderCustomDepth(true);
	InstancedStaticMesh->SetCustomDepthStencilValue(42);
	InstancedStaticMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);

	GrassMeshes.Add(InstancedStaticMesh);
	return InstancedStaticMesh;
}

void URuntimeLandscapeComponent::Rebuild()
{
	ParentLandscape->GetRebuildManager()->QueueRebuild(this);
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

void URuntimeLandscapeComponent::InitializeSubcomponents()
{
	TArray<USceneComponent*> OldChildComponents;
	GetChildrenComponents(false, OldChildComponents);

	OldChildComponents = OldChildComponents.FilterByPredicate([](const USceneComponent* Current)
	{
		return Current->Implements<URuntimeLandscapeSubcomponent>();
	});

	for (TSubclassOf<UActorComponent> SubComponentType : ParentLandscape->SubComponentTypes)
	{
		int32 i = OldChildComponents.IndexOfByPredicate(
			[SubComponentType](const USceneComponent* Current)
			{
				return Current->GetClass() == SubComponentType;
			});

		if (i != INDEX_NONE)
		{
			Cast<IRuntimeLandscapeSubcomponent>(OldChildComponents[i])->SetRuntimeLandscapeComponent(this);
			OldChildComponents.RemoveAt(i);
		}
		// add a new component
		else
		{
			UActorComponent* AddedComponent = NewObject<UActorComponent>(ParentLandscape, SubComponentType);
			if (IRuntimeLandscapeSubcomponent* SubComponent = Cast<IRuntimeLandscapeSubcomponent>(AddedComponent);
				ensure(SubComponent))
			{
				AddedComponent->RegisterComponent();
				SubComponent->SetRuntimeLandscapeComponent(this);
			}
		}
	}

	// Remove components that are no longer required
	for (int32 i = OldChildComponents.Num() - 1; i >= 0; --i)
	{
		OldChildComponents[i]->DestroyComponent();
	}
}

void URuntimeLandscapeComponent::FinishRebuild(const FRuntimeLandscapeRebuildBuffer& RebuildBuffer)
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

	for (const FLandscapeAdditionalData& AdditionalData : RebuildBuffer.AdditionalData)
	{
		for (const auto& GrassData : AdditionalData.GrassData)
		{
			if (GrassData.Value.InstanceTransformsRelative.IsEmpty() == false)
			{
				UHierarchicalInstancedStaticMeshComponent* GrassMesh = FindOrAddGrassMesh(GrassData.Value.GrassVariety);
				GrassMesh->AddInstances(GrassData.Value.InstanceTransformsRelative, false);
			}
		}
	}

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
	VertexColors.Init(FColor::White, RebuildBuffer.VerticesRelative.Num());

	TArray<int32> Triangles;
	if (VerticesInHole.IsEmpty())
	{
		Triangles = RebuildBuffer.Triangles;
	}
	else
	{
		Triangles = ParentLandscape->GetRebuildManager()->GenerateTriangleArray(&VerticesInHole);
	}

	CreateMeshSection(0, RebuildBuffer.VerticesRelative, Triangles, RebuildBuffer.Normals, RebuildBuffer.UV0Coords,
	                  RebuildBuffer.UV1Coords, RebuildBuffer.UV0Coords, RebuildBuffer.UV0Coords, VertexColors,
	                  RebuildBuffer.Tangents, ParentLandscape->bUpdateCollision);

	RemoveFoliageAffectedByLayer();
	UpdateNavigation();
	InitializeSubcomponents();

	UE_LOG(RuntimeEditableLandscape, Display, TEXT("	Finished rebuilding Landscape component %s %i..."),
	       *GetOwner()->GetName(), Index);
}

void URuntimeLandscapeComponent::DestroyComponent(bool bPromoteChildren)
{
	const TArray<TObjectPtr<USceneComponent>>& Children = GetAttachChildren();
	for (int32 i = Children.Num() - 1; i >= 0; --i)
	{
		Children[i]->DestroyComponent();
	}

	for (UHierarchicalInstancedStaticMeshComponent* GrassMesh : GrassMeshes)
	{
		if (GrassMesh)
		{
			GrassMesh->DestroyComponent();
		}
	}

	Super::DestroyComponent(bPromoteChildren);
}
