// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"

#include "ImageUtils.h"
#include "Landscape.h"
#include "LandscapeLayerComponent.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Chaos/HeightField.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LayerTypes/LandscapeGroundTypeLayerData.h"
#include "LayerTypes/LandscapeLayerDataBase.h"
#include "Threads/RuntimeLandscapeRebuildManager.h"

FVector FLandscapeVertex::GetWorldLocation() const
{
	if (ContainingComponent && VertexIndex != INDEX_NONE)
	{
		FVector2D RelativeVertexLocation = ContainingComponent->GetRelativeVertexLocation(VertexIndex);
		FVector ComponentLocation = ContainingComponent->GetComponentLocation();

		return FVector(RelativeVertexLocation.X + ComponentLocation.X, RelativeVertexLocation.Y + ComponentLocation.Y,
		               ContainingComponent->GetInitialHeightValues()[VertexIndex]);
	}

	return FVector::Zero();
}

int32 FRuntimeLandscapeGroundTypeLayerSet::GetPixelIndexForCoordinates(FIntVector2 VertexCoords) const
{
	int32 Result = VertexCoords.X;
	Result += VertexCoords.Y * RenderTarget->SizeX;
	return Result;
}

ARuntimeLandscape::ARuntimeLandscape() : Super()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root component");
	RebuildManager = CreateDefaultSubobject<URuntimeLandscapeRebuildManager>("Rebuild manager");

#if WITH_EDITORONLY_DATA
	const ConstructorHelpers::FObjectFinder<UMaterial> DebugMaterialFinder(
		TEXT("Material'/RuntimeEditableLandscape/Materials/M_DebugMaterial.M_DebugMaterial'"));
	if (ensure(DebugMaterialFinder.Succeeded()))
	{
		DebugMaterial = DebugMaterialFinder.Object;
	}
#endif
}

void ARuntimeLandscape::AddLandscapeLayer(const ULandscapeLayerComponent* LayerToAdd)
{
	SCOPE_CYCLE_COUNTER(STAT_AddLandscapeLayer);
	if (ensure(LayerToAdd))
	{
		// apply layer effects to whole landscape
		for (const ULandscapeLayerDataBase* Layer : LayerToAdd->GetLayerData())
		{
			Layer->ApplyToLandscape(this, LayerToAdd);
		}

		// apply layer effects to components
		for (URuntimeLandscapeComponent* Component : GetComponentsInArea(LayerToAdd->GetBoundingBox()))
		{
			Component->AddLandscapeLayer(LayerToAdd);
		}
	}
}

void ARuntimeLandscape::DrawGroundType(const ULandscapeGroundTypeData* GroundType, ELayerShape Shape,
                                       const FTransform& WorldTransform, const FVector& BrushExtent)
{
	if (GroundLayerSets.IsEmpty())
	{
		return;
	}

	FRuntimeLandscapeGroundTypeLayerSet* LayerSet = nullptr;
	for (FRuntimeLandscapeGroundTypeLayerSet& CurrentLayerSet : GroundLayerSets)
	{
		bool bIsMatchingMapping = CurrentLayerSet.GroundTypeMappings.ContainsByPredicate(
			[GroundType](const FGroundTypeMapping& Current)
			{
				return Current.GroundTypeData == GroundType;
			});

		if (bIsMatchingMapping)
		{
			LayerSet = &CurrentLayerSet;
			break;
		}
	}

	FGroundTypeBrushData BrushData = GroundTypeBrushes.FindRef(Shape);
	UMaterialInstanceDynamic* MaskBrushMaterial = BrushData.BrushMaterialInstance;

	if (ensure(MaskBrushMaterial && LayerSet))
	{
		UCanvas* Canvas;
		FDrawToRenderTargetContext RenderTargetContext;
		FVector2D RenderTargetSize;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(GetWorld(), LayerSet->RenderTarget, Canvas,
		                                                       RenderTargetSize,
		                                                       RenderTargetContext);

		// calculate Position
		const FVector RelativePosition = WorldTransform.GetLocation() - GetOriginLocation() - BrushExtent;
		const FVector2D Position = FVector2D(RelativePosition.X / LandscapeSize.X,
		                                     RelativePosition.Y / LandscapeSize.Y);
		const FVector2D ScreenPosition = FVector2D(Position.X * Canvas->SizeX, Position.Y * Canvas->SizeY);

		// calculate brush size
		const float AspectRatio = Canvas->SizeY / Canvas->SizeX;
		const float ScaleFactor = RenderTargetSize.X / LandscapeSize.X;
		const FVector BoxSize = BrushExtent * 2.0f;
		const FVector2D BrushSize = FVector2D(BoxSize.X, BoxSize.Y * AspectRatio) * ScaleFactor;

		const float Yaw = WorldTransform.GetRotation().Rotator().Yaw;

		FLinearColor ColorChannel = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		for (const FGroundTypeMapping& Mapping : LayerSet->GroundTypeMappings)
		{
			if (Mapping.GroundTypeData == GroundType)
			{
				ColorChannel = Mapping.GetRenderTargetColorChannel();
				break;
			}
		}

		MaskBrushMaterial->SetVectorParameterValue(MATERIAL_PARAMETER_GROUND_TYPE_LAYER_COLOR, ColorChannel);

		Canvas->K2_DrawMaterial(MaskBrushMaterial, ScreenPosition, BrushSize, FVector2D::Zero(),
		                        FVector2D::UnitVector, Yaw);

		UpdateVertexLayerWeights(*LayerSet);
	}
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef - bound to delegate
void ARuntimeLandscape::HandleLandscapeLayerOwnerDestroyed(AActor* DestroyedActor)
{
	TArray<ULandscapeLayerComponent*> LandscapeLayers;
	DestroyedActor->GetComponents(LandscapeLayers);

	for (ULandscapeLayerComponent* Layer : LandscapeLayers)
	{
		RemoveLandscapeLayer(Layer);
	}
}

void ARuntimeLandscape::PostLoad()
{
	Super::PostLoad();

	// Bake layers after editor load
	if (ParentLandscape)
	{
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, this, &ARuntimeLandscape::HandleEditorLoaded, 1.0f, false);
	}
}

void ARuntimeLandscape::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();

	// initialize brushes
	for (auto& GroundTypeBrush : GroundTypeBrushes)
	{
		GroundTypeBrush.Value.BrushMaterialInstance = UKismetMaterialLibrary::CreateDynamicMaterialInstance(
			World, GroundTypeBrush.Value.BrushMaterial);
	}

	GetWorldTimerManager().SetTimerForNextTick(this, &ARuntimeLandscape::BakeLandscapeLayersAndDestroyLandscape);
}

void ARuntimeLandscape::RemoveLandscapeLayer(const ULandscapeLayerComponent* Layer)
{
	for (URuntimeLandscapeComponent* LandscapeComponent : LandscapeComponents)
	{
		LandscapeComponent->RemoveLandscapeLayer(Layer);
	}
}

TMap<const ULandscapeGroundTypeData*, float> ARuntimeLandscape::GetGroundTypeLayerWeightsAtVertexCoordinates(
	int32 SectionIndex, int32 X, int32 Y) const
{
	FIntVector2 VertexCoordinates;
	GetVertexCoordinatesWithinLandscape(SectionIndex, X, Y, VertexCoordinates);

	TMap<const ULandscapeGroundTypeData*, float> Result;

	for (const FRuntimeLandscapeGroundTypeLayerSet& LayerSet : GroundLayerSets)
	{
		if (ensure(LayerSet.RenderTarget))
		{
			int32 PixelIndex = LayerSet.GetPixelIndexForCoordinates(VertexCoordinates);
			if (ensure(LayerSet.VertexLayerWeights.IsValidIndex(PixelIndex)))
			{
				const FColor& ColorAtPixel = LayerSet.VertexLayerWeights[PixelIndex];
				int32 LayerIndex = 0;
				for (const FGroundTypeMapping& Layer : LayerSet.GroundTypeMappings)
				{
					uint8 Value = Layer.GetColorValue(ColorAtPixel);

					float LayerWeight = Value / 255.0f;
					Result.Add(Layer.GroundTypeData, LayerWeight);
					++LayerIndex;
				}
			}
		}
	}

	return Result;
}

TArray<URuntimeLandscapeComponent*> ARuntimeLandscape::GetComponentsInArea(const FBox2D& Area) const
{
	const FVector2D StartLocation = FVector2D(LandscapeComponents[0]->GetComponentLocation());
	// check if the area is inside the landscape
	if (Area.Min.X > StartLocation.X + LandscapeSize.X || Area.Min.Y > StartLocation.Y +
		LandscapeSize.Y ||
		Area.Max.X < StartLocation.X || Area.Max.Y < StartLocation.Y)
	{
		return TArray<URuntimeLandscapeComponent*>();
	}

	FBox2D RelativeArea = Area;
	RelativeArea.Min -= StartLocation;
	RelativeArea.Max -= StartLocation;

	const int32 MinColumn = FMath::Max(FMath::FloorToInt(RelativeArea.Min.X / ComponentSize), 0);
	const int32 MaxColumn = FMath::Min(FMath::FloorToInt(RelativeArea.Max.X / ComponentSize), ComponentAmount.X - 1);

	const int32 MinRow = FMath::Max(FMath::FloorToInt(RelativeArea.Min.Y / ComponentSize), 0);
	const int32 MaxRow = FMath::Min(FMath::FloorToInt(RelativeArea.Max.Y / ComponentSize), ComponentAmount.Y - 1);

	TArray<URuntimeLandscapeComponent*> Result;
	const int32 ExpectedAmount = (MaxColumn - MinColumn + 1) * (MaxRow - MinRow + 1);

	if (!ensure(ExpectedAmount > 0))
	{
		return TArray<URuntimeLandscapeComponent*>();
	}

	Result.Reserve(ExpectedAmount);

	for (int32 Y = MinRow; Y <= MaxRow; Y++)
	{
		const int32 RowOffset = Y * ComponentAmount.X;
		for (int32 X = MinColumn; X <= MaxColumn; X++)
		{
			Result.Add(LandscapeComponents[RowOffset + X]);
		}
	}

	check(Result.Num() == ExpectedAmount);
	return Result;
}

void ARuntimeLandscape::GetComponentCoordinates(int32 SectionIndex, FIntVector2& OutCoordinateResult) const
{
	OutCoordinateResult.X = SectionIndex % FMath::RoundToInt(ComponentAmount.X);
	OutCoordinateResult.Y = FMath::FloorToInt(SectionIndex / ComponentAmount.X);
}

void ARuntimeLandscape::GetVertexCoordinatesWithinComponent(int32 VertexIndex, FIntVector2& OutCoordinateResult) const
{
	OutCoordinateResult.X = VertexIndex % VertexAmountPerComponent.X;
	OutCoordinateResult.Y = FMath::FloorToInt(
		static_cast<float>(VertexIndex) / static_cast<float>(VertexAmountPerComponent.Y));
}

void ARuntimeLandscape::GetVertexCoordinatesWithinLandscape(int32 SectionIndex, int32 SectionVertexX,
                                                            int32 SectionVertexY,
                                                            FIntVector2& OutCoordinateResult) const
{
	FIntVector2 SectionCoordinates;
	GetComponentCoordinates(SectionIndex, SectionCoordinates);

	OutCoordinateResult.X = ComponentResolution.X * SectionCoordinates.X + SectionVertexX;
	OutCoordinateResult.Y = ComponentResolution.Y * SectionCoordinates.Y + SectionVertexY;
}

int32 ARuntimeLandscape::GetComponentIndexAtCoordinate(const int32 CoordX, const int32 CoordY) const
{
	const int32 Result = CoordX + ComponentAmount.X * CoordY;
	return LandscapeComponents.IsValidIndex(Result) ? Result : INDEX_NONE;
}

FVector ARuntimeLandscape::GetOriginLocation() const
{
	if (LandscapeComponents.IsEmpty() == false && LandscapeComponents[0])
	{
		return LandscapeComponents[0]->GetComponentLocation();
	}

	return GetActorLocation();
}

FBox2D ARuntimeLandscape::GetComponentBounds(int32 SectionIndex) const
{
	const FVector2D SectionSize = LandscapeSize / ComponentAmount;
	FIntVector2 SectionCoordinates;
	GetComponentCoordinates(SectionIndex, SectionCoordinates);

	return FBox2D(
		FVector2D(SectionCoordinates.X * SectionSize.X, SectionCoordinates.Y * SectionSize.Y),
		FVector2D((SectionCoordinates.X + 1) * SectionSize.X, (SectionCoordinates.Y + 1) * SectionSize.Y));
}

bool ARuntimeLandscape::TryGetCornerVerticesOfBox(const FBox2D& Box, float BoxYaw,
                                                  TArray<FLandscapeVertex>& OutVertices) const
{
	check(OutVertices.IsEmpty());
	OutVertices.InsertDefaulted(0, 4);

	// grow the box in diagonal so it contains the original box regardless of it's yaw
	float Diagonal = FMath::Sqrt(FMath::Square(Box.GetExtent().X) + FMath::Square(Box.GetExtent().Y));
	FVector2D DiagonalVector(Diagonal);
	FBox2D GrownBox(Box.GetCenter() - DiagonalVector, Box.GetCenter() + DiagonalVector);

	for (const URuntimeLandscapeComponent* LandscapeComponent : GetComponentsInArea(GrownBox))
	{
		int32 i = 0;
		for (int32 VertexIndex : LandscapeComponent->GetCornerVerticesOfBox(Box, BoxYaw))
		{
			if (VertexIndex != INDEX_NONE)
			{
				OutVertices[i].VertexIndex = VertexIndex;
				OutVertices[i].ContainingComponent = LandscapeComponent;
			}

			++i;
		}
	}

	return !OutVertices.ContainsByPredicate([](const FLandscapeVertex& Current)
	{
		return Current.VertexIndex == INDEX_NONE;
	});
}

bool ARuntimeLandscape::TryCalculatePitchAndRollToMatchLandscapeNormal(const FBox2D& Box, float BoxYaw, float& OutPitch,
                                                                       float& OutRoll) const
{
	auto PythagorasAngleAlpha = [](float Length, float Height)
	{
		float DirectionOffset = Height >= 0.0f ? -90.0f : 90.0f;
		return FMath::RadiansToDegrees(FMath::Atan(Length / Height)) + DirectionOffset;
	};

	float ElevationX = 0.0f;
	float ElevationY = 0.0f;
	if (TryCalculateElevationInBoxDirections(Box, BoxYaw, ElevationX, ElevationY))
	{
		FVector2D BoxSize = Box.GetExtent() * 2.0f;
		OutPitch = PythagorasAngleAlpha(BoxSize.X, ElevationX);
		OutRoll = PythagorasAngleAlpha(BoxSize.Y, ElevationY);
		return true;
	}

	return false;
}

bool ARuntimeLandscape::TryCalculateElevationInBoxDirections(const FBox2D& Box, float BoxYaw,
                                                             float& OutElevationXDirection,
                                                             float& OutElevationYDirection) const
{
	TArray<FLandscapeVertex> CornerVertices;
	if (TryGetCornerVerticesOfBox(Box, BoxYaw, CornerVertices))
	{
		// get initial height values
		float TopLeftCornerHeight =
			CornerVertices[0].ContainingComponent->GetInitialHeightValues()[CornerVertices[0].VertexIndex];

		float TopRightCornerHeight =
			CornerVertices[1].ContainingComponent->GetInitialHeightValues()[CornerVertices[1].VertexIndex];

		float BottomRightCornerHeight =
			CornerVertices[2].ContainingComponent->GetInitialHeightValues()[CornerVertices[2].VertexIndex];

		float BottomLeftCornerHeight =
			CornerVertices[3].ContainingComponent->GetInitialHeightValues()[CornerVertices[3].VertexIndex];

		// calculate average heights for box edges
		float AvgHeightTopEdge = (TopLeftCornerHeight + TopRightCornerHeight) * 0.5f;
		float AvgHeightBottomEdge = (BottomLeftCornerHeight + BottomRightCornerHeight) * 0.5f;
		float AvgHeightLeftEdge = (TopLeftCornerHeight + BottomLeftCornerHeight) * 0.5f;
		float AvgHeightRightEdge = (TopRightCornerHeight + BottomRightCornerHeight) * 0.5f;

		OutElevationYDirection = AvgHeightTopEdge - AvgHeightBottomEdge;
		OutElevationXDirection = AvgHeightLeftEdge - AvgHeightRightEdge;
		return true;
	}

	return false;
}

bool ARuntimeLandscape::TryGetNormalDirectionInBox(const FBox2D& Box, float BoxYaw,
                                                   FVector& OutNormalDirection) const
{
	TArray<FLandscapeVertex> CornerVertices;
	if (TryGetCornerVerticesOfBox(Box, BoxYaw, CornerVertices))
	{
		FPlane Plane = FPlane(CornerVertices[2].GetWorldLocation(), CornerVertices[1].GetWorldLocation(),
		                      CornerVertices[0].GetWorldLocation());
		OutNormalDirection = Plane.GetNormal();
		return true;
	}

	return false;
}

void ARuntimeLandscape::UpdateVertexLayerWeights(FRuntimeLandscapeGroundTypeLayerSet& LayerSet)
{
	if (ensure(LayerSet.RenderTarget))
	{
		if (FTextureResource* Resource = LayerSet.RenderTarget->GetResource())
		{
			bool bHasMatchingResolution = LayerSet.RenderTarget->SizeX == Resource->GetSizeX()
				&& LayerSet.RenderTarget->SizeY == Resource->GetSizeY();
			if (ensureAlwaysMsgf(bHasMatchingResolution,
			                     TEXT("Render target hat non matching resolution. Save the asset %s and try again."
			                     ),
			                     *LayerSet.RenderTarget->GetName()))
			{
				FImage MaskImage;
				FImageUtils::GetRenderTargetImage(LayerSet.RenderTarget, MaskImage);
				TArrayView64<FColor> MaskValues = MaskImage.AsBGRA8();
				LayerSet.VertexLayerWeights = MaskValues;
			}
		}
	}
}

void ARuntimeLandscape::InitializeSubcomponents()
{
	for (URuntimeLandscapeComponent* LandscapeComponent : LandscapeComponents)
	{
		if (ensure(IsValid(LandscapeComponent)))
		{
			LandscapeComponent->InitializeSubcomponents();
		}
	}
}

void ARuntimeLandscape::InitializeDynamicMaterial()
{
	if (!LandscapeMaterialInstance && LandscapeMaterialParent)
	{
		LandscapeMaterialInstance =
			UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, LandscapeMaterialParent);
		for (URuntimeLandscapeComponent* LandscapeComponent : LandscapeComponents)
		{
			LandscapeComponent->SetMaterial(0, LandscapeMaterialInstance);
		}
	}
}

void ARuntimeLandscape::BakeLandscapeLayers()
{
	if (ParentLandscape)
	{
		InitializeDynamicMaterial();
		InitializeRenderTargets(false);

#if WITH_EDITORONLY_DATA
		SetUpLayerColorChannelMappings();
#endif

		constexpr FBox2D Box2D = FBox2D();

		for (FRuntimeLandscapeGroundTypeLayerSet& LayerSet : GroundLayerSets)
		{
			if (LayerSet.RenderTarget)
			{
				LayerSet.RenderTarget->SizeX = MeshResolution.X + 1;
				LayerSet.RenderTarget->SizeY = MeshResolution.Y + 1;

#if WITH_EDITORONLY_DATA

				TArray<ULandscapeLayerInfoObject*> PaintLayers;
				ParentLandscape->GetUsedPaintLayers(0, PaintLayers);

				// match ground type order with parent landscape paint layer order
				TArray<FGroundTypeMapping> OrderedGroundTypeMappings;
				OrderedGroundTypeMappings.AddZeroed(LayerSet.GroundTypeMappings.Num());

				for (int32 Index = PaintLayers.Num() - 1; Index >= 0; --Index)
				{
					for (const FGroundTypeMapping& GroundType : LayerSet.GroundTypeMappings)
					{
						if (GroundType.LayerInfoObject == PaintLayers[Index])
						{
							OrderedGroundTypeMappings[Index] = GroundType;
							PaintLayers.RemoveAt(Index);
							break;;
						}
					}
				}

				LayerSet.GroundTypeMappings = OrderedGroundTypeMappings;

				for (ULandscapeLayerInfoObject* PaintLayer : PaintLayers)
				{
					UE_LOG(RuntimeEditableLandscape, Warning,
					       TEXT("Missing ground type for paint layer on Parent Landscape: %s"),
					       *PaintLayer->GetLayerName().ToString());
				}

#endif

				TArray<FName> BakedLayerNames;
				BakedLayerNames.Reserve(LayerSet.GroundTypeMappings.Num());

				for (const FGroundTypeMapping& GroundType : LayerSet.GroundTypeMappings)
				{
					if (GroundType.LayerInfoObject)
					{
						BakedLayerNames.Add(GroundType.LayerInfoObject->GetLayerName());
					}
				}
				
				if (!BakedLayerNames.IsEmpty()
					&& ensureAlways(ParentLandscape->RenderWeightmaps(GetActorTransform(), Box2D, BakedLayerNames,
						LayerSet.RenderTarget)))
				{
					UpdateVertexLayerWeights(LayerSet);
				}
			}
		}
	}
}

void ARuntimeLandscape::Rebuild()
{
	TArray<UHierarchicalInstancedStaticMeshComponent*> InstancedMeshes;
	GetComponents(InstancedMeshes);
	for (UHierarchicalInstancedStaticMeshComponent* InstancedMesh : InstancedMeshes)
	{
		InstancedMesh->DestroyComponent();
	}

	BakeLandscapeLayers();

	// clean up old components but remember existing layers
	TSet<TObjectPtr<const ULandscapeLayerComponent>> LandscapeLayers;
	for (URuntimeLandscapeComponent* LandscapeComponent : LandscapeComponents)
	{
		if (LandscapeComponent)
		{
			LandscapeLayers.Append(LandscapeComponent->GetAffectingLayers().Array());
			LandscapeComponent->DestroyComponent();
		}
	}

	BodyInstance = FBodyInstance();
	BodyInstance.CopyBodyInstancePropertiesFrom(&ParentLandscape->BodyInstance);
	bGenerateOverlapEvents = ParentLandscape->bGenerateOverlapEvents;

	// create landscape components
	LandscapeComponents.SetNumUninitialized(ParentLandscape->CollisionComponents.Num());
	const int32 VertexAmountPerSection = GetTotalVertexAmountPerComponent();

	for (const ULandscapeHeightfieldCollisionComponent* LandscapeCollision : ParentLandscape->CollisionComponents)
	{
		Chaos::FHeightFieldPtr HeightField = LandscapeCollision->HeightfieldRef->HeightfieldGeometry;
		TArray<float> HeightValues = TArray<float>();
		HeightValues.Reserve(VertexAmountPerSection);
		for (int32 i = 0; i < VertexAmountPerSection; i++)
		{
			HeightValues.Add(HeightField->GetHeight(i) * HeightScale);
		}

		TSubclassOf<URuntimeLandscapeComponent> ComponentType = ComponentTypeOverride;
		if (!ComponentType)
		{
			ComponentType = URuntimeLandscapeComponent::StaticClass();
		}

		URuntimeLandscapeComponent* LandscapeComponent = NewObject<URuntimeLandscapeComponent>(this, ComponentType);
		LandscapeComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

		LandscapeComponent->SetWorldLocation(LandscapeCollision->GetComponentLocation());
		LandscapeComponent->SetMaterial(0, LandscapeMaterialInstance);
		LandscapeComponent->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		LandscapeComponent->SetCastShadow(bCastShadow);
		LandscapeComponent->SetAffectDistanceFieldLighting(bAffectDistanceFieldLighting);

		LandscapeComponent->BodyInstance = FBodyInstance();
		LandscapeComponent->BodyInstance.CopyBodyInstancePropertiesFrom(&ParentLandscape->BodyInstance);
		LandscapeComponent->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		LandscapeComponent->SetCanEverAffectNavigation(bCanEverAffectNavigation);

		FVector ParentOrigin;
		FVector ParentExtent;
		ParentLandscape->GetActorBounds(false, ParentOrigin, ParentExtent);

		// calculate index by position for more efficient access later
		const FVector StartLocation = ParentOrigin - ParentExtent;
		const FVector ComponentLocation = LandscapeComponent->GetComponentLocation() - StartLocation;
		int32 ComponentIndex = ComponentLocation.X / ComponentSize + ComponentLocation.Y / ComponentSize *
			ComponentAmount.X;

		LandscapeComponent->Initialize(ComponentIndex, HeightValues);
		LandscapeComponent->RegisterComponent();
		LandscapeComponents[ComponentIndex] = LandscapeComponent;
	}

	// add remembered layers
	for (const ULandscapeLayerComponent* Layer : LandscapeLayers)
	{
		AddLandscapeLayer(Layer);
	}
}

void ARuntimeLandscape::InitializeFromLandscape()
{
#if WITH_EDITORONLY_DATA // do nothing in packaged build (is still there to make editor widget work)
	if (!ParentLandscape)
	{
		return;
	}

	SetActorLocation(ParentLandscape->GetActorLocation());

	if (!LandscapeMaterialParent)
	{
		LandscapeMaterialParent = ParentLandscape->LandscapeMaterial;
	}

	HeightScale = ParentLandscape->GetActorScale().Z / FMath::Pow(2.0f, HeightValueBits);
	ParentHeight = ParentLandscape->GetActorLocation().Z;

	const FIntRect Rect = ParentLandscape->GetBoundingRect();
	MeshResolution.X = Rect.Max.X - Rect.Min.X;
	MeshResolution.Y = Rect.Max.Y - Rect.Min.Y;
	FVector ParentOrigin;
	FVector ParentExtent;
	ParentLandscape->GetActorBounds(false, ParentOrigin, ParentExtent);
	LandscapeSize = FVector2D(ParentExtent * FVector(2.0f));
	const int32 ComponentSizeQuads = ParentLandscape->ComponentSizeQuads;
	QuadSideLength = ParentExtent.X * 2 / MeshResolution.X;
	ComponentSize = ComponentSizeQuads * QuadSideLength;
	AreaPerSquare = FMath::Square(QuadSideLength);
	ComponentAmount = FVector2D(MeshResolution.X / ComponentSizeQuads, MeshResolution.Y / ComponentSizeQuads);
	ComponentResolution = MeshResolution / ComponentAmount;

	VertexAmountPerComponent.X = MeshResolution.X / ComponentAmount.X + 1;
	VertexAmountPerComponent.Y = MeshResolution.Y / ComponentAmount.Y + 1;

	Rebuild();
#endif
}

void ARuntimeLandscape::SetComponentSelected(int32 ComponentIndex, bool bNewSelected) const
{
#if WITH_EDITORONLY_DATA
	if (LandscapeComponents.IsValidIndex(ComponentIndex) && LandscapeComponents[ComponentIndex])
	{
		if (bNewSelected)
		{
			LandscapeComponents[ComponentIndex]->SetMaterial(0, SelectionHighlightMaterial);
		}
		else
		{
			LandscapeComponents[ComponentIndex]->SetMaterial(0, LandscapeMaterialInstance);
		}
	}
#endif
}

void ARuntimeLandscape::BakeLandscapeLayersAndDestroyLandscape()
{
	if (ParentLandscape)
	{
		InitializeRenderTargets(true);
		BakeLandscapeLayers();

		ParentLandscape->SetActorHiddenInGame(true);
		ParentLandscape = nullptr;
	}

	OnLandscapeInitialized.Broadcast(this);
	OnLandscapeInitialized.Clear(); // won't be needed anymore, free up the memory
}


void ARuntimeLandscape::InitializeRenderTargets(bool bOverrideExisting)
{
	if (!IsValid(LandscapeMaterialInstance))
	{
		return;
	}

	for (int32 i = 0; i < GroundLayerSets.Num(); ++i)
	{
		if (bOverrideExisting || !IsValid(GroundLayerSets[i].RenderTarget))
		{
			GroundLayerSets[i].RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
				GetWorld(), MeshResolution.X + 1, MeshResolution.Y + 1,
				ETextureRenderTargetFormat::RTF_RGBA8,
				FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

			TMap<FMaterialParameterInfo, FMaterialParameterMetadata> MaterialParameters;
			LandscapeMaterialInstance->GetAllParametersOfType(EMaterialParameterType::Texture, MaterialParameters);
			FName ParameterName = FName(MATERIAL_PARAMETER_GROUND_TYPE_LAYER_MASK, i);

			bool bDoesParameterExist = false;
			for (const auto& Parameter : MaterialParameters)
			{
				if (Parameter.Key.Name.IsEqual(MATERIAL_PARAMETER_GROUND_TYPE_LAYER_MASK))
				{
					LandscapeMaterialInstance->SetTextureParameterValue(
						ParameterName, GroundLayerSets[i].RenderTarget);
					bDoesParameterExist = true;
					break;
				}
			}

			if (!bDoesParameterExist)
			{
				UE_LOG(RuntimeEditableLandscape, Warning,
				       TEXT("The landscape material %s has no parameter named %s"),
				       *LandscapeMaterialInstance->GetName(), *ParameterName.ToString())
			}
		}
	}
}

TArray<URuntimeLandscapeComponent*> ARuntimeLandscape::GetComponentsInArea(int32 ColumnMin, int32 ColumnMax,
                                                                           int32 RowMin, int32 RowMax) const
{
	TArray<URuntimeLandscapeComponent*> Result;
	for (int32 Column = ColumnMin; Column <= ColumnMax; ++Column)
	{
		for (int32 Row = RowMin; Row <= RowMax; ++Row)
		{
			int32 ComponentIndex = GetComponentIndexAtCoordinate(Column, Row);
			Result.Add(LandscapeComponents[ComponentIndex].Get());
		}
	}

	return Result;
}

#if WITH_EDITORONLY_DATA

void ARuntimeLandscape::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	if (IsValid(ParentLandscape))
	{
		if (bAffectDistanceFieldLighting)
		{
			ParentLandscape->SetActorEnableCollision(false);
			ParentLandscape->bUsedForNavigation = false;
			ParentLandscape->SetActorHiddenInGame(true);
		}
		else
		{
			ParentLandscape->Destroy();
		}
	}
}

void ARuntimeLandscape::SetUpLayerColorChannelMappings()
{
	if (!ParentLandscape)
	{
		return;
	}

	// map layers to render target color channels
	// TODO: implement multiple edit layers
	// int32 EditLayerCount = ParentLandscape->GetEditLayersConst().Num();
	TArrayView<const FLandscapeLayer> ParentLayers = ParentLandscape->GetLayersConst();
	for (const FLandscapeLayer& Layer : ParentLayers)
	{
		UE_LOG(RuntimeEditableLandscape, Warning, TEXT("TEST %s"), *Layer.Name_DEPRECATED.ToString());
	}

	TArray<ULandscapeLayerInfoObject*> LayerInfos;
	ParentLandscape->GetUsedPaintLayers(0, LayerInfos);
	for (FRuntimeLandscapeGroundTypeLayerSet& LayerSet : GroundLayerSets)
	{
		for (FGroundTypeMapping& Mapping : LayerSet.GroundTypeMappings)
		{
			if (Mapping.LayerInfoObject)
			{
				switch (LayerInfos.IndexOfByKey(Mapping.LayerInfoObject))
				{
				case 0:
					Mapping.RenderTargetColorChannel = FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
					break;
				case 1:
					Mapping.RenderTargetColorChannel = FLinearColor(0.0f, 1.0f, 0.0f, 0.0f);
					break;
				case 2:
					Mapping.RenderTargetColorChannel = FLinearColor(0.0f, 0.0f, 1.0f, 0.0f);
					break;
				case 3:
					Mapping.RenderTargetColorChannel = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
					break;
				default:
					UE_LOG(RuntimeEditableLandscape, Warning,
					       TEXT("Cound not find layer info at the parent Landscape for layer-info object '%s'"),
					       *Mapping.LayerInfoObject->GetName());
				}
			}
		}
	}
}

void ARuntimeLandscape::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const TSet<FString> InitLandscapeProperties = TSet<FString>({
		"ParentLandscape", "bEnableDebug", "bDrawDebugCheckerBoard", "bDrawIndexGreyScales", "DebugColor1",
		"DebugColor2", "DebugMaterial", "HoleActors", "bCastShadow", "bAffectDistanceFieldLighting"
	});
	if (InitLandscapeProperties.Contains(PropertyChangedEvent.MemberProperty->GetName()))
	{
		InitializeFromLandscape();
	}

	LandscapeComponents.Remove(nullptr);

	if (PropertyChangedEvent.MemberProperty->GetName() == FName("bGenerateOverlapEvents"))
	{
		for (URuntimeLandscapeComponent* Component : LandscapeComponents)
		{
			Component->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		}
	}

	if (PropertyChangedEvent.MemberProperty->GetName() == FName("LandscapeMaterial"))
	{
		InitializeDynamicMaterial();
		UMaterialInterface* Material = LandscapeMaterialInstance;
		if (bEnableDebug && DebugMaterial)
		{
			Material = DebugMaterial;
		}

		for (URuntimeLandscapeComponent* Component : LandscapeComponents)
		{
			Component->SetMaterial(0, Material);
		}
	}

	if (PropertyChangedEvent.MemberProperty->GetName() == FName("BodyInstance") || PropertyChangedEvent.
		MemberProperty->
		GetName() == FName("bGenerateOverlapEvents"))
	{
		for (URuntimeLandscapeComponent* Component : LandscapeComponents)
		{
			Component->BodyInstance = FBodyInstance();
			Component->BodyInstance.CopyBodyInstancePropertiesFrom(&BodyInstance);
			Component->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		}
	}

	SetUpLayerColorChannelMappings();
}

#endif
