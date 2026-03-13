// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeLayerActorBase.h"
#include "Components/SphereComponent.h"
#include "LandscapeLayerSphere.generated.h"

UCLASS(Category = "Runtime Landscape")
class RUNTIMEEDITABLELANDSCAPE_API ALandscapeLayerSphere : public ALandscapeLayerActorBase
{
	GENERATED_BODY()

public:
	ALandscapeLayerSphere(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditAnywhere)
	TObjectPtr<USphereComponent> SphereBounds;

	virtual UPrimitiveComponent* GetBoundsComponent() const override { return SphereBounds; }
};
