// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeHole.h"

#include "RuntimeLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"

ALandscapeHole::ALandscapeHole() : Super()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("Root component"));

#if WITH_EDITORONLY_DATA
	BoxPreview = CreateDefaultSubobject<UBoxComponent>(FName("Box preview"));
	BoxPreview->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	BoxPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpherePreview = CreateDefaultSubobject<USphereComponent>(FName("Sphere preview"));
	SpherePreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpherePreview->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	UpdatePreviewComponents();

	UBillboardComponent* BillboardComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>("Editor billboard");
	BillboardComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
#endif
}

bool ALandscapeHole::IsLocationInside(const FVector& Location) const
{
	switch (Shape)
	{
	case EHoleShape::HS_Box:
		return FBox(GetActorLocation() - Extent, GetActorLocation() + Extent).IsInsideOrOn(Location);
	case EHoleShape::HS_Sphere:
		return (GetActorLocation() - Location).SizeSquared() <= FMath::Square(Radius);
	}

	checkNoEntry();
	return false;
}

FBox2D ALandscapeHole::GetHoleBounds() const
{
	const FVector2D Location2D = FVector2D(GetActorLocation());
	FVector2D Extent2D;
	switch (Shape)
	{
	case EHoleShape::HS_Box:
		Extent2D = FVector2D(Extent);
		break;
	case EHoleShape::HS_Sphere:
		Extent2D = FVector2D(Radius);
		break;
	default:
		checkNoEntry();
	}

	return FBox2D(Location2D - Extent2D, Location2D + Extent2D);
}

void ALandscapeHole::Destroyed()
{
	Super::Destroyed();

	for (TObjectPtr<ARuntimeLandscape> Landscape : AffectedLandscapes)
	{
		if (Landscape)
		{
			for (URuntimeLandscapeComponent* LandscapeComponent : Landscape->GetComponentsInArea(GetHoleBounds()))
			{
				LandscapeComponent->RemoveHole(this);
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
void ALandscapeHole::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange->GetName() == FName("AffectedLandscapes"))
	{
		for (TObjectPtr<ARuntimeLandscape> Landscape : AffectedLandscapes)
		{
			if (Landscape)
			{
				for (URuntimeLandscapeComponent* LandscapeComponent : Landscape->GetComponentsInArea(GetHoleBounds()))
				{
					LandscapeComponent->RemoveHole(this);
				}
			}
		}
	}
}

void ALandscapeHole::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const TSet<FString> PreviewedProperties = TSet<FString>({"Shape", "Radius", "Extent"});
	if (PreviewedProperties.Contains(PropertyChangedEvent.MemberProperty->GetName()))
	{
		UpdatePreviewComponents();
	}

	if (PropertyChangedEvent.MemberProperty->GetName() == FName("AffectedLandscapes"))
	{
		for (TObjectPtr<ARuntimeLandscape> Landscape : AffectedLandscapes)
		{
			if (Landscape)
			{
				for (URuntimeLandscapeComponent* LandscapeComponent : Landscape->GetComponentsInArea(GetHoleBounds()))
				{
					LandscapeComponent->AddHole(this);
				}
			}
		}
	}
}

void ALandscapeHole::UpdatePreviewComponents() const
{
	SpherePreview->SetSphereRadius(Radius);
	BoxPreview->SetBoxExtent(Extent);

	BoxPreview->SetVisibility(false);
	SpherePreview->SetVisibility(false);
	switch (Shape)
	{
	case EHoleShape::HS_Box:
		BoxPreview->SetVisibility(true);
		break;
	case EHoleShape::HS_Sphere:
		SpherePreview->SetVisibility(true);
		break;
	default:
		checkNoEntry();
	}
}
#endif
