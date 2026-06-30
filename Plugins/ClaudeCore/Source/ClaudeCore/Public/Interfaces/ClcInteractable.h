// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ClcInteractable.generated.h"

UINTERFACE()
class CLAUDECORE_API UClcInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * C++层统一交互接口
 */
class CLAUDECORE_API IClcInteractable
{
	GENERATED_BODY()

public:
	virtual FText GetInteractionPrompt() const = 0;
	virtual bool OnInteract(AActor* Interactor) = 0;
};
