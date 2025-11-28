// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RuntimeLandscapeSubcomponent.generated.h"

// This class does not need to be modified.
UINTERFACE()
class URuntimeLandscapeSubcomponent : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for components that can be attached to runtime landscape components for additional functionality
 */
class RUNTIMEEDITABLELANDSCAPE_API IRuntimeLandscapeSubcomponent
{
	GENERATED_BODY()

	friend class URuntimeLandscapeComponent;
	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
protected:
	virtual void SetRuntimeLandscapeComponent(URuntimeLandscapeComponent* ParentLandscapeComponent) = 0;
};
