// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"

#include "ImageUtils.h"
#include "Landscape.h"
#include "LandscapeLayerComponent.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Chaos/HeightField.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LayerTypes/LandscapeGroundTypeLayerData.h"
#include "LayerTypes/LandscapeLayerDataBase.h"
#include "Threads/RuntimeLandscapeRebuildManager.h"

TArray<FName> FRuntimeLandscapeGroundTypeLayerSet::GetLayerNames() const
{
	TArray<FName> Result;
	for (const ULandscapeGroundTypeData* GroundLayer : GroundTypes)
	{
		if (GroundLayer)
		{
			Result.Add(GroundLayer->LandscapeLayerName);
		}
	}

	return Result;
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
	FRuntimeLandscapeGroundTypeLayerSet* LayerSet = nullptr;
	for (FRuntimeLandscapeGroundTypeLayerSet& CurrentLayerSet : GroundLayerSets)
	{
		if (CurrentLayerSet.GroundTypes.Contains(GroundType))
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
		FLinearColor ColorChannel = LayerSet->GetColorChannelForLayer(GroundType);
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
		GetWorld()->GetTimerManager().SetTimer(Handle, this, &ARuntimeLandscape::BakeLandscapeLayers, 1.0f, false);
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

	if (bBakeLayersOnBeginPlay)
	{
		BakeLandscapeLayers();
	}
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
				for (const ULandscapeGroundTypeData* Layer : LayerSet.GroundTypes)
				{
					uint8 Value = 0;
					switch (LayerIndex)
					{
					case 0:
						Value = ColorAtPixel.R;
						break;
					case 1:
						Value = ColorAtPixel.G;
						break;
					case 2:
						Value = ColorAtPixel.B;
						break;
					case 3:
						Value = ColorAtPixel.A;
						break;
					default:
						checkNoEntry();
					}
					float LayerWeight = Value / 255.0f;
					Result.Add(Layer, LayerWeight);
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

void ARuntimeLandscape::UpdateVertexLayerWeights(FRuntimeLandscapeGroundTypeLayerSet& LayerSet)
{
	FImage MaskImage;
	FImageUtils::GetRenderTargetImage(LayerSet.RenderTarget, MaskImage);
	TArrayView64<FColor> MaskValues = MaskImage.AsBGRA8();
	LayerSet.VertexLayerWeights = MaskValues;
}

void ARuntimeLandscape::BakeLandscapeLayers()
{
	if (ParentLandscape)
	{
		FBox2D Box2D = FBox2D();

		for (FRuntimeLandscapeGroundTypeLayerSet& LayerSet : GroundLayerSets)
		{
			const TArray<FName>& LayerNames = LayerSet.GetLayerNames();
			if (LayerSet.RenderTarget)
			{
				LayerSet.RenderTarget->SizeX = MeshResolution.X + 1;
				LayerSet.RenderTarget->SizeY = MeshResolution.Y + 1;
				ParentLandscape->RenderWeightmaps(GetActorTransform(), Box2D, LayerNames,
				                                  LayerSet.RenderTarget);

				UpdateVertexLayerWeights(LayerSet);
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
void ARuntimeLandscape::InitializeFromLandscape()
{
	if (!ParentLandscape)
	{
		return;
	}

	if (!LandscapeMaterial)
	{
		LandscapeMaterial = ParentLandscape->LandscapeMaterial;
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

		URuntimeLandscapeComponent* LandscapeComponent = NewObject<URuntimeLandscapeComponent>(this);
		LandscapeComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		LandscapeComponent->SetWorldLocation(LandscapeCollision->GetComponentLocation());
		LandscapeComponent->SetMaterial(0, LandscapeMaterial);
		LandscapeComponent->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		LandscapeComponent->SetCastShadow(bCastShadow);
		LandscapeComponent->SetAffectDistanceFieldLighting(bAffectDistanceFieldLighting);

		LandscapeComponent->BodyInstance = FBodyInstance();
		LandscapeComponent->BodyInstance.CopyBodyInstancePropertiesFrom(&ParentLandscape->BodyInstance);
		LandscapeComponent->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		LandscapeComponent->SetCanEverAffectNavigation(bCanEverAffectNavigation);

		// calculate index by position for more efficient access later
		const FVector StartLocation = ParentOrigin - ParentExtent;
		const FVector ComponentLocation = LandscapeComponent->GetComponentLocation() - StartLocation;
		const int32 ComponentIndex = ComponentLocation.X / ComponentSize + ComponentLocation.Y / ComponentSize *
			ComponentAmount.X;

		LandscapeComponent->Initialize(ComponentIndex, HeightValues);
		LandscapeComponent->RegisterComponent();
		LandscapeComponents[ComponentIndex] = LandscapeComponent;
	}

	// add remembered layers
	for (TObjectPtr<const ULandscapeLayerComponent> Layer : LandscapeLayers)
	{
		AddLandscapeLayer(Layer);
	}
}

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

	if (PropertyChangedEvent.MemberProperty->GetName() == FName("bGenerateOverlapEvents"))
	{
		for (URuntimeLandscapeComponent* Component : LandscapeComponents)
		{
			Component->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		}
	}
	if (PropertyChangedEvent.MemberProperty->GetName() == FName("LandscapeMaterial"))
	{
		for (URuntimeLandscapeComponent* Component : LandscapeComponents)
		{
			Component->SetMaterial(0, bEnableDebug && DebugMaterial ? DebugMaterial : LandscapeMaterial);
		}
	}

	if (PropertyChangedEvent.MemberProperty->GetName() == FName("BodyInstance") || PropertyChangedEvent.MemberProperty->
		GetName() == FName("bGenerateOverlapEvents"))
	{
		for (URuntimeLandscapeComponent* Component : LandscapeComponents)
		{
			Component->BodyInstance = FBodyInstance();
			Component->BodyInstance.CopyBodyInstancePropertiesFrom(&BodyInstance);
			Component->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		}
	}
}

#endif
