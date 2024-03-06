// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LandscapeLayerComponent.generated.h"


class ARuntimeLandscape;

USTRUCT()
struct FLandscapeLayerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FVector2D LayerSize;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeLayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULandscapeLayerComponent();

	void ApplyToLandscape() const;

	FBox2D GetAffectedArea() const
	{
		FVector Origin;
		FVector Extent;
		GetOwner()->GetActorBounds(false, Origin, Extent);
		return FBox2D(FVector2D(Origin.X - Extent.X, Origin.Y - Extent.Y),
		              FVector2D(Origin.X + Extent.X, Origin.Y + Extent.Y));
	}

	UPROPERTY(EditDefaultsOnly)
	/** If true, the layer will be applied after apply to ApplyToLandscape(), otherwise it will be applied on construction */
	bool bWaitForActivation;
	UPROPERTY(EditDefaultsOnly)
	TSharedRef<FLandscapeLayerData> LayerData;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void DestroyComponent(bool bPromoteChildren) override;
};
