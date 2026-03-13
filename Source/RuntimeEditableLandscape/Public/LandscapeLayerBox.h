// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "LandscapeLayerActorBase.h"
#include "LandscapeLayerBox.generated.h"

UCLASS(Category = "Runtime Landscape")
class RUNTIMEEDITABLELANDSCAPE_API ALandscapeLayerBox : public ALandscapeLayerActorBase
{
	GENERATED_BODY()

public:
	ALandscapeLayerBox(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditAnywhere)
	TObjectPtr<UBoxComponent> BoxBounds;

	virtual UPrimitiveComponent* GetBoundsComponent() const override
	{
		return BoxBounds;
	}
};
