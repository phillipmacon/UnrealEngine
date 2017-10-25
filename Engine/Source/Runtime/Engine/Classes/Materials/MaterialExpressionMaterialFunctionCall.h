// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * MaterialExpressionMaterialFunctionCall - an expression which allows a material to use a material function
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionMaterialFunctionCall.generated.h"

class UMaterialFunction;
struct FPropertyChangedEvent;

/** Struct that stores information about a function input which is needed to maintain connections and implement the function call. */
USTRUCT()
struct FFunctionExpressionInput
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * Reference to the FunctionInput in the material function.  
	 * This is a reference to a private object so it can't be saved, and must be generated by UpdateFromFunctionResource or SetMaterialFunction. 
	 */
	UPROPERTY(transient)
	class UMaterialExpressionFunctionInput* ExpressionInput;

	/** Id of the FunctionInput, used to link ExpressionInput. */
	UPROPERTY()
	FGuid ExpressionInputId;

	/** Actual input struct which stores information about how this input is connected in the material. */
	UPROPERTY()
	FExpressionInput Input;


	FFunctionExpressionInput()
		: ExpressionInput(NULL)
	{
	}

};

/** Struct that stores information about a function output which is needed to maintain connections and implement the function call. */
USTRUCT()
struct FFunctionExpressionOutput
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * Reference to the FunctionOutput in the material function.  
	 * This is a reference to a private object so it can't be saved, and must be generated by UpdateFromFunctionResource or SetMaterialFunction. 
	 */
	UPROPERTY(transient)
	class UMaterialExpressionFunctionOutput* ExpressionOutput;

	/** Id of the FunctionOutput, used to link ExpressionOutput. */
	UPROPERTY()
	FGuid ExpressionOutputId;

	/** Actual output struct which stores information about how this output is connected in the material. */
	UPROPERTY()
	FExpressionOutput Output;


	FFunctionExpressionOutput()
		: ExpressionOutput(NULL)
	{
	}

};

UCLASS(hidecategories=object, MinimalAPI)
class UMaterialExpressionMaterialFunctionCall : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** The function to call. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionMaterialFunctionCall)
	class UMaterialFunctionInterface* MaterialFunction;

	/** Array of all the function inputs that this function exposes. */
	UPROPERTY()
	TArray<struct FFunctionExpressionInput> FunctionInputs;

	/** Array of all the function outputs that this function exposes. */
	UPROPERTY()
	TArray<struct FFunctionExpressionOutput> FunctionOutputs;

	/** Used by material parameters to split references to separate instances. */
	UPROPERTY(Transient)
	struct FMaterialParameterInfo FunctionParameterInfo;

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostLoad() override;
	virtual bool NeedsLoadForClient() const override;
	//~ End UObject Interface.

	ENGINE_API void GetDependentFunctions(TArray<UMaterialFunctionInterface*>& DependentFunctions) const;

#if WITH_EDITOR
	void UnlinkFunctionFromCaller(FMaterialCompiler* Compiler);
	void LinkFunctionIntoCaller(FMaterialCompiler* Compiler);
#endif

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const override;
#if WITH_EDITOR
	virtual FString GetDescription() const override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
#endif // WITH_EDITOR
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
#if WITH_EDITOR
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface

	/** Util to get name of a particular type, optionally with type appended in parenthesis */
	ENGINE_API FName GetInputNameWithType(int32 InputIndex, bool bWithType) const;

#if WITH_EDITOR
	/** 
	 * Set a new material function, given an old function so that links can be passed over if the name matches. 
	 *
	 *	@param OldFunctionResource				The function it was set to.
	 *	@param NewResource						The function to be set to.
	 *
	 *	@return									true if setting the function was a success, false if it failed.
	 */
	ENGINE_API bool SetMaterialFunctionEx(UMaterialFunctionInterface* OldFunctionResource, UMaterialFunctionInterface* NewResource);

	/** */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	ENGINE_API bool SetMaterialFunction(UMaterialFunctionInterface* NewMaterialFunction);
#endif // WITH_EDITOR

	/** 
	 * Update FunctionInputs and FunctionOutputs from the MaterialFunction.  
	 * This must be called to keep the inputs and outputs up to date with the function being used. 
	 */
	ENGINE_API void UpdateFromFunctionResource(bool bRecreateAndLinkNode = true);

private:
	
#if WITH_EDITOR
	/** Helper that fixes up expression links where possible. */
	void FixupReferencingExpressions(
		const TArray<FFunctionExpressionOutput>& NewOutputs,
		const TArray<FFunctionExpressionOutput>& OriginalOutputs,
		TArray<UMaterialExpression*>& Expressions, 
		TArray<FExpressionInput*>& MaterialInputs,
		bool bMatchByName);
#endif // WITH_EDITOR
};



