// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeLayerBox.h"

ALandscapeLayerBox::ALandscapeLayerBox(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	BoxBounds = CreateDefaultSubobject<UBoxComponent>("Box bounds");
	InitializeBounds();
}
