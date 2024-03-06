// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"

#include "Landscape.h"
#include "LandscapeLayerComponent.h"
#include "Chaos/HeightField.h"

ARuntimeLandscape::ARuntimeLandscape()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root component");
}

void ARuntimeLandscape::AddLandscapeLayer(const ULandscapeLayerComponent* LayerToAdd, bool bForceRebuild)
{
	SCOPE_CYCLE_COUNTER(STAT_AddLandscapeLayer);
	if (ensure(LayerToAdd))
	{
		for (const int32 AffectedSectionIndex : GetComponentsInArea(LayerToAdd->GetAffectedArea()))
		{
			LandscapeComponents[AffectedSectionIndex]->AddLandscapeLayer(LayerToAdd, bForceRebuild);
		}
	}
}

void ARuntimeLandscape::BeginPlay()
{
	Super::BeginPlay();

	if (LandscapeToCopyFrom)
	{
		LandscapeToCopyFrom->Destroy();
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

TArray<int32> ARuntimeLandscape::GetComponentsInArea(const FBox2D& Area) const
{
	const FVector2D ActorHorizontalLocation = FVector2D(GetActorLocation());
	// check if the area is inside the landscape
	if (Area.Min.X > ActorHorizontalLocation.X + LandscapeSize.X || Area.Min.Y > ActorHorizontalLocation.Y +
		LandscapeSize.Y ||
		Area.Max.X < ActorHorizontalLocation.X || Area.Max.Y < ActorHorizontalLocation.Y)
	{
		return TArray<int32>();
	}

	const FVector2D SectionSize = GetSizePerComponent();

	FBox2D RelativeArea = Area;
	RelativeArea.Min -= ActorHorizontalLocation;
	RelativeArea.Max -= ActorHorizontalLocation;

	const int32 MinColumn = FMath::Max(FMath::FloorToInt(RelativeArea.Min.X / SectionSize.X), 0);
	const int32 MaxColumn = FMath::Min(FMath::FloorToInt(RelativeArea.Max.X / SectionSize.X), ComponentAmount.X - 1);

	const int32 MinRow = FMath::Max(FMath::FloorToInt(RelativeArea.Min.Y / SectionSize.Y), 0);
	const int32 MaxRow = FMath::Min(FMath::FloorToInt(RelativeArea.Max.Y / SectionSize.Y), ComponentAmount.Y - 1);

	TArray<int32> Result;
	const int32 ExpectedAmount = (MaxColumn - MinColumn + 1) * (MaxRow - MinRow + 1);

	if (!ensure(ExpectedAmount > 0))
	{
		return TArray<int32>();
	}

	Result.Reserve(ExpectedAmount);

	for (int32 Y = MinRow; Y <= MaxRow; Y++)
	{
		const int32 RowOffset = Y * ComponentAmount.X;
		for (int32 X = MinColumn; X <= MaxColumn; X++)
		{
			Result.Add(RowOffset + X);
		}
	}

	check(Result.Num() == ExpectedAmount);
	return Result;
}

void ARuntimeLandscape::GetComponentCoordinates(int32 SectionId, FIntVector2& OutCoordinateResult) const
{
	OutCoordinateResult.X = SectionId % FMath::RoundToInt(ComponentAmount.X);
	OutCoordinateResult.Y = FMath::FloorToInt(SectionId / ComponentAmount.X);
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

#if WITH_EDITOR
void ARuntimeLandscape::InitializeFromLandscape()
{
	if (!LandscapeToCopyFrom)
	{
		return;
	}

	const FIntRect Rect = LandscapeToCopyFrom->GetBoundingRect();
	MeshResolution.X = Rect.Max.X - Rect.Min.X;
	MeshResolution.Y = Rect.Max.Y - Rect.Min.Y;

	FVector ParentOrigin;
	FVector ParentExtent;
	LandscapeToCopyFrom->GetActorBounds(false, ParentOrigin, ParentExtent);

	LandscapeSize = FVector2D(ParentExtent * FVector(2.0f));

	const float ComponentCount = LandscapeToCopyFrom->LandscapeComponents.Num();
	const float ComponentsX = FMath::Sqrt(ComponentCount) * (LandscapeSize.X / LandscapeSize.Y);
	const float ComponentsY = ComponentCount / ComponentsX;
	ComponentAmount = FVector2D(ComponentsX, ComponentsY);

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

	// create landscape components
	LandscapeComponents.SetNumUninitialized(LandscapeToCopyFrom->CollisionComponents.Num());
	const int32 VertexAmountPerSection = GetVertexAmountPerSection();
	int32 ComponentIndex = 0;
	for (const ULandscapeHeightfieldCollisionComponent* LandscapeCollision : LandscapeToCopyFrom->CollisionComponents)
	{
		const TUniquePtr<Chaos::FHeightField>& HeightField = LandscapeCollision->HeightfieldRef->Heightfield;
		TArray<float> HeightValues = TArray<float>();
		HeightValues.Reserve(VertexAmountPerSection);
		for (int32 i = 0; i < VertexAmountPerSection; i++)
		{
			HeightValues.Add(HeightField->GetHeight(i) * HeightScale);
		}

		URuntimeLandscapeComponent* LandscapeComponent = NewObject<URuntimeLandscapeComponent>(this);
		LandscapeComponent->RegisterComponent();
		LandscapeComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		LandscapeComponent->SetWorldLocation(LandscapeCollision->GetComponentLocation());
		LandscapeComponent->Initialize(ComponentIndex, HeightValues);
		LandscapeComponents[ComponentIndex] = LandscapeComponent;
		ComponentIndex++;
	}

	// add remembered layers
	for (TObjectPtr<const ULandscapeLayerComponent> Layer : LandscapeLayers)
	{
		AddLandscapeLayer(Layer, false);
	}
}

void ARuntimeLandscape::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	InitializeFromLandscape();
}
#endif
