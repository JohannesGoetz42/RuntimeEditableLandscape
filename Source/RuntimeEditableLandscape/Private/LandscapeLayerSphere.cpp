// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeLayerSphere.h"

ALandscapeLayerSphere::ALandscapeLayerSphere(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SphereBounds = CreateDefaultSubobject<USphereComponent>(FName("Sphere bounds"));
	InitializeBounds();
}
