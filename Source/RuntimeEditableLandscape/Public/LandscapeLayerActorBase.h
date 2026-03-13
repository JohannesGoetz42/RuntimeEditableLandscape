// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandscapeLayerActorBase.generated.h"

class ULandscapeLayerComponent;
class URuntimeLandscapeComponent;
class ARuntimeLandscape;
class UCapsuleComponent;
class UBoxComponent;
class USphereComponent;

UCLASS(Category = "Runtime Landscape")
/**
 * Actor that holds a landscape layer
 */
class RUNTIMEEDITABLELANDSCAPE_API ALandscapeLayerActorBase : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeLayerActorBase(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditAnywhere)
	TObjectPtr<ULandscapeLayerComponent> LayerComponent;

	/** MUST Be called in constructor! */
	void InitializeBounds();

	virtual UPrimitiveComponent* GetBoundsComponent() const
	{
		checkNoEntry();
		return nullptr;
	}
};
