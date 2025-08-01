// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeLayerActor.h"

#include "LandscapeLayerComponent.h"
#include "RuntimeLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"

ALandscapeLayerActor::ALandscapeLayerActor() : Super()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("Root component"));
	LayerComponent = CreateDefaultSubobject<ULandscapeLayerComponent>(FName("Layer component"));

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

#if WITH_EDITORONLY_DATA
void ALandscapeLayerActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty->GetName() == FString("LayerComponent"))
	{
		UpdatePreviewComponents();
	}
}

void ALandscapeLayerActor::UpdatePreviewComponents() const
{
	SpherePreview->SetSphereRadius(LayerComponent->GetRadius());
	BoxPreview->SetBoxExtent(LayerComponent->GetExtent());

	BoxPreview->SetVisibility(false);
	SpherePreview->SetVisibility(false);
	switch (LayerComponent->GetShape())
	{
	case ELayerShape::HS_Box:
		BoxPreview->SetVisibility(true);
		break;
	case ELayerShape::HS_Round:
		SpherePreview->SetVisibility(true);
		break;
	default:
		checkNoEntry();
	}
}
#endif
