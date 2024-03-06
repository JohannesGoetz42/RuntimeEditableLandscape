// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandscapeLayerActor.generated.h"

class ULandscapeLayerComponent;
class URuntimeLandscapeComponent;
class ARuntimeLandscape;
class UCapsuleComponent;
class UBoxComponent;
class USphereComponent;

UCLASS()
/**
 * Creates a hole in a RuntimeLandscape
 */
class RUNTIMEEDITABLELANDSCAPE_API ALandscapeLayerActor : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeLayerActor();

protected:
	UPROPERTY(EditAnywhere)
	TObjectPtr<ULandscapeLayerComponent> LayerComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<USphereComponent> SpherePreview;
	UPROPERTY()
	TObjectPtr<UBoxComponent> BoxPreview;

	void UpdatePreviewComponents() const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void OnConstruction(const FTransform& Transform) override
	{
		Super::OnConstruction(Transform);
		UpdatePreviewComponents();
	}
	
#endif
};
