// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"

#include "Landscape.h"
#include "LandscapeLayerComponent.h"
#include "RuntimeEditableLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Chaos/HeightField.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LayerTypes/LandscapeLayerDataBase.h"

ARuntimeLandscape::ARuntimeLandscape() : Super()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root component");

#if WITH_EDITORONLY_DATA
	const ConstructorHelpers::FObjectFinder<UMaterial> DebugMaterialFinder(
		TEXT("Material'/RuntimeEditableLandscape/Materials/M_DebugMaterial.M_DebugMaterial'"));
	if (ensure(DebugMaterialFinder.Succeeded()))
	{
		DebugMaterial = DebugMaterialFinder.Object;
	}
#endif
}

void ARuntimeLandscape::AddLandscapeLayer(const ULandscapeLayerComponent* LayerToAdd, bool bForceRebuild)
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
			Component->AddLandscapeLayer(LayerToAdd, bForceRebuild);
		}
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

void ARuntimeLandscape::RemoveLandscapeLayer(const ULandscapeLayerComponent* Layer, bool bForceRebuild)
{
	for (URuntimeLandscapeComponent* LandscapeComponent : LandscapeComponents)
	{
		LandscapeComponent->RemoveLandscapeLayer(Layer, bForceRebuild);
	}
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

void ARuntimeLandscape::BakeLandscapeLayers()
{
	if (ParentLandscape)
	{
		for (const auto& Layer : ParentLandscape->GetTargetLayers())
		{
			ULandscapeGroundTypeData** GroundTypeForLayer = GroundTypes.FindByPredicate(
				[Layer](const ULandscapeGroundTypeData* Current)
				{
					return Current && Current->LandscapeLayerName == Layer.Key;
				});
			if (GroundTypeForLayer)
			{
				const ULandscapeGroundTypeData* GroundType = *GroundTypeForLayer;
				GroundType->MaskRenderTarget->SizeX = FMath::RoundToInt(
					GroundType->PaintLayerResolution * LandscapeSize.X);
				GroundType->MaskRenderTarget->SizeY = FMath::RoundToInt(
					GroundType->PaintLayerResolution * LandscapeSize.Y);

				FBox2D Box2D = FBox2D();
				Box2D.Min = FVector2D(0.0f, 0.0f);
				Box2D.Max = LandscapeSize;
				ParentLandscape->RenderWeightmap(GetActorTransform(), Box2D, Layer.Key, GroundType->MaskRenderTarget);
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
		AddLandscapeLayer(Layer, false);
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
