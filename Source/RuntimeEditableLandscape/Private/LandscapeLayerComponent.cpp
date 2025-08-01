// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeLayerComponent.h"

#include "RuntimeLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "LayerTypes/LandscapeLayerDataBase.h"

void ULandscapeLayerComponent::ApplyToLandscape()
{
	if (AffectedLandscapes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("LandscapeLayerComponent on %s could not find a landscape and can not be applied."),
		       *GetOwner()->GetName());
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
                                              float& OutHeightValue,
                                              FColor& OutVertexColorValue) const
{
	const FVector2D VertexLocation = LandscapeComponent->GetRelativeVertexLocation(VertexIndex) + FVector2D(
		LandscapeComponent->GetComponentLocation());
	if (!IsAffectedByLayer(VertexLocation))
	{
		return;
	}

	float SmoothingFactor;
	if (TryCalculateSmoothingFactor(SmoothingFactor, VertexLocation))
	{
		for (const ULandscapeLayerDataBase* Layer : Layers)
		{
			Layer->ApplyToVertices(LandscapeComponent, this, VertexIndex, OutHeightValue, OutVertexColorValue,
			                       SmoothingFactor);
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

bool ULandscapeLayerComponent::TryCalculateSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location) const
{
	const FVector2D Origin = FVector2D(BoundsComponent
		                                   ? BoundsComponent->GetComponentLocation()
		                                   : GetOwner()->GetActorLocation());
	switch (Shape)
	{
	case ELayerShape::HS_Box:
		return TryCalculateBoxSmoothingFactor(OutSmoothingFactor, Location, Origin);

	case ELayerShape::HS_Round:
		return TryCalculateSphereSmoothingFactor(OutSmoothingFactor, Location, Origin);
	default:
		checkNoEntry();
	}

	return false;
}

bool ULandscapeLayerComponent::TryCalculateBoxSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location,
                                                              FVector2D Origin) const
{
	const FVector RotatedLocation = UKismetMathLibrary::InverseTransformLocation(
		BoundsComponent ? BoundsComponent->GetComponentTransform() : GetOwner()->GetActorTransform(),
		FVector(Location, 0.0f));

	const float DistanceSqr = InnerBox.ComputeSquaredDistanceToPoint(FVector2D(RotatedLocation) + Origin);
	const float SmoothingDistanceSqr = FMath::Square(SmoothingDistance);
	if (DistanceSqr >= SmoothingDistanceSqr)
	{
		return false;
	}

	OutSmoothingFactor = DistanceSqr == 0.0f ? 0.0f : DistanceSqr / SmoothingDistanceSqr;
	return true;
}

bool ULandscapeLayerComponent::TryCalculateSphereSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location,
                                                                 FVector2D Origin) const
{
	const float OuterRadiusSquared = FMath::Square(Radius + BoundsSmoothingOffset);
	const float DistanceSqr = (Location - Origin).SizeSquared();
	if (DistanceSqr >= OuterRadiusSquared)
	{
		return false;
	}

	const float InnerRadiusSqr = FMath::Square(Radius - InnerSmoothingOffset);
	if (DistanceSqr < InnerRadiusSqr)
	{
		OutSmoothingFactor = 0.0f;
	}
	else
	{
		check(SmoothingDistance > 0.0f);
		const float Distance = FMath::Abs(FMath::Sqrt(DistanceSqr) - (Radius - InnerSmoothingOffset));
		OutSmoothingFactor = Distance / SmoothingDistance;
		check(OutSmoothingFactor >= 0.0f && OutSmoothingFactor <= 1.0f);
	}

	return true;
}

void ULandscapeLayerComponent::HandleBoundsChanged(USceneComponent* SceneComponent,
                                                   EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	UpdateShape();
	for (ARuntimeLandscape* AffectedLandscape : AffectedLandscapes)
	{
		AffectedLandscape->RemoveLandscapeLayer(this, false);
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
				LandscapeComponent->RemoveLandscapeLayer(this, true);
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
				LandscapeComponent->AddLandscapeLayer(this, true);
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
