// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeLayerComponent.h"

#include "RuntimeLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "LayerTypes/LandscapeLayerDataBase.h"

void ULandscapeLayerComponent::ApplyToLandscape()
{
	if (AffectedLandscapes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("LandscapeLayerComponent on '%s' could not find a landscape and can not be applied."),
		       *GetOwner()->GetName());
	}

	// if there is an affected landscape that is not yet initialized, wait for it to finish
	for (ARuntimeLandscape* LandscapeActor : AffectedLandscapes)
	{
		if (LandscapeActor->IsInitialized() == false)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("LandscapeLayerComponent on '%s' is waiting for landscape '%s' to be initialized."),
			       *GetOwner()->GetName(), *LandscapeActor->GetName());
			LandscapeActor->OnLandscapeInitialized.AddUniqueDynamic(
				this, &ULandscapeLayerComponent::HandleLandscapeInitialized);
			return;
		}
	}

	for (ARuntimeLandscape* LandscapeActor : AffectedLandscapes)
	{
		LandscapeActor->AddLandscapeLayer(this);
	}

	if (BoundsComponent)
	{
		BoundsComponent->TransformUpdated.AddUObject(this, &ULandscapeLayerComponent::HandleBoundsChanged);
	}
	else
	{
		GetOwner()->GetRootComponent()->TransformUpdated.AddUObject(
			this, &ULandscapeLayerComponent::HandleBoundsChanged);
	}

	if (GetOwner())
	{
		GetOwner()->OnDestroyed.AddUniqueDynamic(this, &ULandscapeLayerComponent::HandleOwnerDestroyed);
	}
}

bool ULandscapeLayerComponent::IsAffectedByLayer(FVector2D Location) const
{
	return GetBoundingBox().IsInside(Location);
}

void ULandscapeLayerComponent::ApplyLayerData(int32 VertexIndex, URuntimeLandscapeComponent* LandscapeComponent,
                                              float& OutHeightValue, FColor& OutVertexColorValue) const
{
	const FVector2D VertexWorldLocation = LandscapeComponent->GetRelativeVertexLocation(VertexIndex) + FVector2D(
		LandscapeComponent->GetComponentLocation());
	if (!IsAffectedByLayer(VertexWorldLocation))
	{
		return;
	}

	FLandscapeLayerVertexInfo VertexInfo;
	VertexInfo.VertexIndex = VertexIndex;

	if (TryCalculateVertexInfo(VertexInfo, VertexWorldLocation))
	{
		for (int32 i = 0; i < Layers.Num(); ++i)
		{
			if (Layers[i])
			{
				Layers[i]->ApplyToVertex(LandscapeComponent, this, OutHeightValue, OutVertexColorValue, VertexInfo);
			}
		}
	}
}

void ULandscapeLayerComponent::SetBoundsComponent(UPrimitiveComponent* NewBoundsComponent)
{
	if (Shape == ELayerShape::HS_Default)
	{
		if (NewBoundsComponent->IsA<USphereComponent>())
		{
			Shape = ELayerShape::HS_Round;
		}
		else
		{
			Shape = ELayerShape::HS_Box;
		}
	}

	BoundsComponent = NewBoundsComponent;
	Extent = BoundsComponent->Bounds.BoxExtent;
	UpdateShape();
}

void ULandscapeLayerComponent::InitializeLayerMemories(const URuntimeLandscapeComponent* LandscapeComponent) const
{
	for (ULandscapeLayerDataBase* LayerData : Layers)
	{
		LayerData->InitializeLayerMemory(this, LandscapeComponent);
	}
}

void ULandscapeLayerComponent::UpdateShape()
{
	if (!BoundsComponent && !GetOwner())
	{
		return;
	}

	const FVector Origin = BoundsComponent ? BoundsComponent->GetComponentLocation() : GetOwner()->GetActorLocation();

	switch (SmoothingDirection)
	{
	case SD_Inwards:
		InnerSmoothingOffset = SmoothingDistance;
		BoundsSmoothingOffset = 0.0f;
		break;
	case SD_Outwards:
		InnerSmoothingOffset = 0.0f;
		BoundsSmoothingOffset = SmoothingDistance;
		break;
	case SD_Center:
		InnerSmoothingOffset = SmoothingDistance * 0.5f;
		BoundsSmoothingOffset = SmoothingDistance * 0.5f;
		break;
	default:
		checkNoEntry();
	}

	// ensure the inner offset is smaller than the inner bounds
	if (SmoothingDirection != SD_Outwards)
	{
		const float MaxOffset = Shape == ELayerShape::HS_Round
			                        ? Radius - 0.001f
			                        : FMath::Min(Extent.X, Extent.Y) - 0.001f;
		InnerSmoothingOffset = FMath::Clamp(InnerSmoothingOffset, 0.0f, MaxOffset);
	}

	if (Shape == ELayerShape::HS_Round)
	{
		BoundingBox = FBox2D(FVector2D(Origin - BoundsSmoothingOffset - Radius),
		                     FVector2D(Origin + BoundsSmoothingOffset + Radius));
		return;
	}

	FBoxSphereBounds BoxSphereBounds(Origin, Extent + BoundsSmoothingOffset, Radius);
	const FTransform Transform = BoundsComponent
		                             ? BoundsComponent->GetComponentTransform()
		                             : GetOwner()->GetActorTransform();
	BoxSphereBounds = BoxSphereBounds.TransformBy(Transform);

	BoundingBox = FBox2D(FVector2D(Origin - BoxSphereBounds.BoxExtent), FVector2D(Origin + BoxSphereBounds.BoxExtent));

	InnerBox.Min = FVector2D(Origin - Extent) + InnerSmoothingOffset;
	InnerBox.Max = FVector2D(Origin + Extent) - InnerSmoothingOffset;
}

bool ULandscapeLayerComponent::TryCalculateSmoothingFactor(FLandscapeLayerVertexInfo& OutVertexInfo,
                                                           const FVector2D& WorldLocation) const
{
	const FVector2D Origin = FVector2D(BoundsComponent
		                                   ? BoundsComponent->GetComponentLocation()
		                                   : GetOwner()->GetActorLocation());
	switch (Shape)
	{
	case ELayerShape::HS_Box:
		return TryCalculateBoxSmoothingFactor(OutVertexInfo, WorldLocation, Origin);

	case ELayerShape::HS_Round:
		return TryCalculateSphereSmoothingFactor(OutVertexInfo, WorldLocation, Origin);
	default:
		checkNoEntry();
	}

	return false;
}

bool ULandscapeLayerComponent::TryCalculateBoxSmoothingFactor(FLandscapeLayerVertexInfo& OutVertexInfo,
                                                              const FVector2D& WorldLocation,
                                                              FVector2D Origin) const
{
	const FVector RotatedLocation = UKismetMathLibrary::InverseTransformLocation(
		BoundsComponent ? BoundsComponent->GetComponentTransform() : GetOwner()->GetActorTransform(),
		FVector(WorldLocation, 0.0f));

	const float DistanceSqr = InnerBox.ComputeSquaredDistanceToPoint(FVector2D(RotatedLocation) + Origin);
	const float SmoothingDistanceSqr = FMath::Square(SmoothingDistance);
	if (DistanceSqr >= SmoothingDistanceSqr)
	{
		return false;
	}

	OutVertexInfo.SmoothingFactor = DistanceSqr == 0.0f ? 0.0f : DistanceSqr / SmoothingDistanceSqr;
	return true;
}

bool ULandscapeLayerComponent::TryCalculateSphereSmoothingFactor(FLandscapeLayerVertexInfo& OutVertexInfo,
                                                                 const FVector2D& WorldLocation,
                                                                 FVector2D Origin) const
{
	const float OuterRadiusSquared = FMath::Square(Radius + BoundsSmoothingOffset);
	const float DistanceSqr = (WorldLocation - Origin).SizeSquared();
	if (DistanceSqr >= OuterRadiusSquared)
	{
		return false;
	}

	const float InnerRadiusSqr = FMath::Square(Radius - InnerSmoothingOffset);
	if (DistanceSqr < InnerRadiusSqr)
	{
		OutVertexInfo.SmoothingFactor = 0.0f;
	}
	else
	{
		check(SmoothingDistance > 0.0f);
		const float Distance = FMath::Abs(FMath::Sqrt(DistanceSqr) - (Radius - InnerSmoothingOffset));
		OutVertexInfo.SmoothingFactor = Distance / SmoothingDistance;
		check(OutVertexInfo.SmoothingFactor >= 0.0f && OutVertexInfo.SmoothingFactor <= 1.0f);
	}

	return true;
}

bool ULandscapeLayerComponent::TryCalculateBoundsDistances(FLandscapeLayerVertexInfo& OutVertexInfo,
                                                           const FVector2D& WorldLocation) const
{
	if (ensureAlways(BoundsComponent))
	{
		FVector BoundsExtent = BoundsComponent->GetLocalBounds().BoxExtent;
		if (ensureAlways(BoundsExtent.X > 0.0f && BoundsExtent.Y > 0.0f))
		{
			FVector ComponentLocation = BoundsComponent->GetComponentLocation();
			FVector2D LocationRelativeToBounds = WorldLocation - FVector2D(ComponentLocation.X, ComponentLocation.Y);
			LocationRelativeToBounds = LocationRelativeToBounds.
				GetRotated(-BoundsComponent->GetComponentRotation().Yaw);

			FVector2D LocationRelativeToTopLeftCorner = FVector2D(BoundsExtent.X, BoundsExtent.Y) -
				LocationRelativeToBounds;

			FVector BoundsSize = BoundsExtent * 2.0f;
			OutVertexInfo.BoundsDistanceLeft = LocationRelativeToTopLeftCorner.X / BoundsSize.X;
			OutVertexInfo.BoundsDistanceTop = LocationRelativeToTopLeftCorner.Y / BoundsSize.Y;

			return true;
		}
	}

	return false;
}

void ULandscapeLayerComponent::HandleBoundsChanged(USceneComponent* SceneComponent,
                                                   EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	UpdateShape();
	for (ARuntimeLandscape* AffectedLandscape : AffectedLandscapes)
	{
		AffectedLandscape->RemoveLandscapeLayer(this);
		AffectedLandscape->AddLandscapeLayer(this);
	}
}

void ULandscapeLayerComponent::RemoveFromLandscapes()
{
	for (TObjectPtr<ARuntimeLandscape> Landscape : AffectedLandscapes)
	{
		if (Landscape)
		{
			for (URuntimeLandscapeComponent* LandscapeComponent : Landscape->GetComponentsInArea(
				     GetBoundingBox()))
			{
				LandscapeComponent->RemoveLandscapeLayer(this);
			}
		}
	}
}

void ULandscapeLayerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...	
	if (AffectedLandscapes.IsEmpty())
	{
		TArray<AActor*> LandscapeActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARuntimeLandscape::StaticClass(), LandscapeActors);
		for (AActor* LandscapeActor : LandscapeActors)
		{
			AffectedLandscapes.Add(Cast<ARuntimeLandscape>(LandscapeActor));
		}
	}

	if (!bWaitForActivation)
	{
		ApplyToLandscape();
	}
}

void ULandscapeLayerComponent::DestroyComponent(bool bPromoteChildren)
{
	RemoveFromLandscapes();
	Super::DestroyComponent(bPromoteChildren);
}

#if WITH_EDITORONLY_DATA
void ULandscapeLayerComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	RemoveFromLandscapes();
}

void ULandscapeLayerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateShape();
	for (TObjectPtr<ARuntimeLandscape> Landscape : AffectedLandscapes)
	{
		if (Landscape)
		{
			for (URuntimeLandscapeComponent* LandscapeComponent : Landscape->GetComponentsInArea(
				     GetBoundingBox()))
			{
				LandscapeComponent->AddLandscapeLayer(this);
			}
		}
	}

	if (!AffectedLandscapes.IsEmpty())
	{
		if (BoundsComponent)
		{
			BoundsComponent->TransformUpdated.AddUObject(this, &ULandscapeLayerComponent::HandleBoundsChanged);
		}
		else if (GetOwner() && GetOwner()->GetRootComponent())
		{
			GetOwner()->GetRootComponent()->TransformUpdated.AddUObject(
				this, &ULandscapeLayerComponent::HandleBoundsChanged);
		}
	}
}
#endif
