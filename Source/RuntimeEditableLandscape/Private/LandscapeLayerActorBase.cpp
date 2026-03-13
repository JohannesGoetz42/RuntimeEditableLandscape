// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeLayerActorBase.h"

#include "LandscapeLayerComponent.h"
#include "Components/BillboardComponent.h"

ALandscapeLayerActorBase::ALandscapeLayerActorBase(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	LayerComponent = CreateDefaultSubobject<ULandscapeLayerComponent>(FName("Layer component"));
}

void ALandscapeLayerActorBase::InitializeBounds()
{
	if (UPrimitiveComponent* Bounds = GetBoundsComponent())
	{
		RootComponent = Bounds;
		Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LayerComponent->SetBoundsComponent(Bounds);
		
#if WITH_EDITORONLY_DATA
		UBillboardComponent* BillboardComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>("Editor billboard");
		BillboardComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
#endif
	}
}
