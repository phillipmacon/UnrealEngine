// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"

#include "CompilerResultsLog.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "UK2Node_BaseAsyncTask"

UK2Node_BaseAsyncTask::UK2Node_BaseAsyncTask(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	, ProxyFactoryFunctionName(NAME_None)
	, ProxyFactoryClass(NULL)
	, ProxyClass(NULL)
{
}

FName UK2Node_BaseAsyncTask::GetProxyFactoryFunctionName()
{
	return ProxyFactoryFunctionName;
}

UClass*	UK2Node_BaseAsyncTask::GetProxyFactoryClass()
{
	return ProxyFactoryClass;
}

UClass* UK2Node_BaseAsyncTask::GetProxyClass()
{
	return ProxyClass;
}

FString UK2Node_BaseAsyncTask::GetCategoryName()
{
	return TEXT("Latent Execution");
}

void UK2Node_BaseAsyncTask::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Execute);
	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Then);

	UFunction* DelegateSignatureFunction = NULL;
	for (TFieldIterator<UProperty> PropertyIt(ProxyClass, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		UMulticastDelegateProperty* Property = Cast<UMulticastDelegateProperty>(*PropertyIt);
		if(!Property)
			continue;

		CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, *Property->GetName());
		if (!DelegateSignatureFunction)
		{
			DelegateSignatureFunction = Property->SignatureFunction;
		}
	}

	if (DelegateSignatureFunction)
	{
		for (TFieldIterator<UProperty> PropIt(DelegateSignatureFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* Param = *PropIt;
			const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (bIsFunctionInput)
			{
				UEdGraphPin* Pin = CreatePin(EGPD_Output, TEXT(""), TEXT(""), NULL, false, false, Param->GetName());
				K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType);
			}
		}
	}

	bool bAllPinsGood = true;
	UFunction* Function = ProxyFactoryClass->FindFunctionByName(ProxyFactoryFunctionName);
	if (Function)
	{
		TSet<FString> PinsToHide;
		FBlueprintEditorUtils::GetHiddenPinsForFunction(GetBlueprint(), Function, PinsToHide);
		for (TFieldIterator<UProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* Param = *PropIt;
			const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (!bIsFunctionInput)
			{
				// skip function output, it's internal node data 
				continue;
			}

			const bool bIsRefParam = Param->HasAnyPropertyFlags(CPF_ReferenceParm) && bIsFunctionInput;
			UEdGraphPin* Pin = CreatePin(EGPD_Input, TEXT(""), TEXT(""), NULL, false, bIsRefParam, Param->GetName());
			const bool bPinGood = (Pin != NULL) && K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType);

			if (bPinGood)
			{
				//Flag pin as read only for const reference property
				Pin->bDefaultValueIsIgnored = Param->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReferenceParm) && (!Function->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm) || Pin->PinType.bIsArray);

				const bool bAdvancedPin = Param->HasAllPropertyFlags(CPF_AdvancedDisplay);
				Pin->bAdvancedView = bAdvancedPin;
				if(bAdvancedPin && (ENodeAdvancedPins::NoPins == AdvancedPinDisplay))
				{
					AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
				}

				FString MetadataDefaultValue = Function->GetMetaData(*Param->GetName());
				if (!MetadataDefaultValue.IsEmpty())
				{
					// Specified default value in the metadata
					Pin->DefaultValue = Pin->AutogeneratedDefaultValue = MetadataDefaultValue;
				}
				else
				{
					const FName MetadataCppDefaultValueKey( *(FString(TEXT("CPP_Default_")) + Param->GetName()) );
					const FString MetadataCppDefaultValue = Function->GetMetaData(MetadataCppDefaultValueKey);
					if(!MetadataCppDefaultValue.IsEmpty())
					{
						Pin->DefaultValue = Pin->AutogeneratedDefaultValue = MetadataCppDefaultValue;
					}
					else
					{
						// Set the default value to (T)0
						K2Schema->SetPinDefaultValueBasedOnType(Pin);
					}
				}

				if (PinsToHide.Contains(Pin->PinName))
				{
					Pin->bHidden = true;
					Pin->DefaultValue = TEXT("0");
				}
			}

			bAllPinsGood = bAllPinsGood && bPinGood;
		}
	}


	Super::AllocateDefaultPins();
}

void UK2Node_BaseAsyncTask::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	if (!CompilerContext.bIsFullCompile)
	{
		return;
	}

	check(SourceGraph);
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	UK2Node_BaseAsyncTask* CurrentNode = this;

	TArray<UMulticastDelegateProperty*> DelegateProperties;
	for (TFieldIterator<UProperty> PropertyIt(CurrentNode->GetProxyClass(), EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		if (UMulticastDelegateProperty* Property = Cast<UMulticastDelegateProperty>(*PropertyIt))
		{
			DelegateProperties.AddUnique(Property);
		}
	}

	if (DelegateProperties.Num() == 0)
	{
		// we don't have any delegates to handle, skip this node
		return;
	}

	const FName CreateMoveToProxyObjectBlueprintFuncName = CurrentNode->GetProxyFactoryFunctionName();
	UK2Node_CallFunction* CallCreateMoveToProxyObjectNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(CurrentNode, SourceGraph);
	CallCreateMoveToProxyObjectNode->FunctionReference.SetExternalMember(CreateMoveToProxyObjectBlueprintFuncName, CurrentNode->GetProxyFactoryClass());
	CallCreateMoveToProxyObjectNode->AllocateDefaultPins();
	CompilerContext.MovePinLinksToIntermediate(*CurrentNode->FindPinChecked(Schema->PN_Execute), *CallCreateMoveToProxyObjectNode->FindPinChecked(Schema->PN_Execute));

	// match function inputs, to pass data to function from CallFunction node
	for (int32 Index=0; Index < CurrentNode->Pins.Num(); ++Index)
	{
		UEdGraphPin* CurrentPin = CurrentNode->Pins[Index];
		if (!CurrentPin || CurrentPin->Direction != EGPD_Input || CurrentPin->PinName == Schema->PN_Execute || CurrentPin->PinName == Schema->PN_Then)
		{
			continue;
		}

		if (UEdGraphPin* DestPin = CallCreateMoveToProxyObjectNode->FindPin(CurrentPin->PinName))
		{
			CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *DestPin);
		}
	}

	TArray<UEdGraphPin*> VariableOutputPins;
	for (int32 Index=0; Index < CurrentNode->Pins.Num(); ++Index)
	{
		UEdGraphPin* CurrentPin = CurrentNode->Pins[Index];
		if (!CurrentPin || CurrentPin->Direction != EGPD_Output || CurrentPin->PinType.PinCategory == Schema->PC_Exec || CurrentPin->PinName == Schema->PN_Execute || CurrentPin->PinName == Schema->PN_Then)
		{
			continue;
		}

		VariableOutputPins.AddUnique(CurrentPin);
	}

	// Create int selection index
	UK2Node_TemporaryVariable* TempIndexVar = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(CurrentNode, SourceGraph);
	TempIndexVar->VariableType.PinCategory = Schema->PC_Int;
	TempIndexVar->AllocateDefaultPins();

	TArray<UK2Node_Select*> OutputSelectNodes;
	for (int32 Index=0; Index < VariableOutputPins.Num(); ++Index)
	{
		UK2Node_Select* SelectNode = CompilerContext.SpawnIntermediateNode<UK2Node_Select>(CurrentNode, SourceGraph);
		SelectNode->NumOptionPins = DelegateProperties.Num();
		SelectNode->IndexPinType = TempIndexVar->VariableType;
		SelectNode->AllocateDefaultPins();
		OutputSelectNodes.AddUnique(SelectNode);
	}

	bool bMoveDataPins = true;
	UEdGraphPin* LastThenPin = NULL;
	for (int32 Index=0; Index < DelegateProperties.Num(); ++Index)
	{
		UProperty* CurrentProperty = DelegateProperties[Index];
		UEdGraphPin* PinForCurrentDelegateProperty = CurrentNode->FindPin(CurrentProperty->GetName());
		if (CurrentProperty == NULL || PinForCurrentDelegateProperty == NULL)
		{
			// we can't match current delegate property with output pin, skip it
			continue;
		}

		UK2Node_AddDelegate* AddDelegateNode = CompilerContext.SpawnIntermediateNode<UK2Node_AddDelegate>(CurrentNode, SourceGraph);
		check(AddDelegateNode);
		AddDelegateNode->SetFromProperty(CurrentProperty, false);
		AddDelegateNode->AllocateDefaultPins();
		Schema->TryCreateConnection(AddDelegateNode->FindPinChecked(Schema->PN_Self), CallCreateMoveToProxyObjectNode->GetReturnValuePin());
		if (LastThenPin == NULL)
		{
			Schema->TryCreateConnection(CallCreateMoveToProxyObjectNode->FindPinChecked(Schema->PN_Then), AddDelegateNode->FindPinChecked(Schema->PN_Execute));
		}
		else
		{
			Schema->TryCreateConnection(LastThenPin, AddDelegateNode->FindPinChecked(Schema->PN_Execute));
		}
		LastThenPin = AddDelegateNode->FindPinChecked(Schema->PN_Then);

		UK2Node_CustomEvent* CurrentCENode = CompilerContext.SpawnIntermediateNode<UK2Node_CustomEvent>(CurrentNode, SourceGraph);
		check(CurrentCENode);
		CurrentCENode->CustomFunctionName = *FString::Printf(TEXT("%s_%u"), *CurrentProperty->GetName(), FPlatformTime::Cycles());
		CurrentCENode->AllocateDefaultPins();

		{
			// WORKAROUND, so we can create delegate from nonexistent function by avoiding check at expanding step
			// instead simply: Schema->TryCreateConnection(AddDelegateNode->GetDelegatePin(), CurrentCENode->FindPinChecked(UK2Node_CustomEvent::DelegateOutputName));

			UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
			SelfNode->AllocateDefaultPins();

			UK2Node_CreateDelegate* CreateDelegateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CreateDelegate>(this, SourceGraph);
			CreateDelegateNode->AllocateDefaultPins();
			Schema->TryCreateConnection(AddDelegateNode->GetDelegatePin(), CreateDelegateNode->GetDelegateOutPin());
			Schema->TryCreateConnection(SelfNode->FindPinChecked(Schema->PN_Self), CreateDelegateNode->GetObjectInPin());
			CreateDelegateNode->SelectedFunctionName = CurrentCENode->GetFunctionName();
		}

		for (TFieldIterator<UProperty> PropIt(AddDelegateNode->GetDelegateSignature()); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const UProperty* Param = *PropIt;
			if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FEdGraphPinType PinType;
				Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);
				CurrentCENode->CreateUserDefinedPin(Param->GetName(), PinType);
			}
		}


		UEdGraphPin* LastActivatedNodeThen = CurrentCENode->FindPinChecked(Schema->PN_Then);
		for (int32 OutIndex = 0; OutIndex < VariableOutputPins.Num(); ++OutIndex)
		{
			UEdGraphPin* OutPin = VariableOutputPins[OutIndex];
			UK2Node_TemporaryVariable* TempVarOutput;
			{
				TempVarOutput = CompilerContext.SpawnInternalVariable(
					CurrentNode,
					OutPin->PinType.PinCategory,
					OutPin->PinType.PinSubCategory,
					OutPin->PinType.PinSubCategoryObject.Get(),
					OutPin->PinType.bIsArray);
			}
			UEdGraphPin* PinWithData = CurrentCENode->FindPin(OutPin->PinName);

			if (PinWithData == NULL)
			{
				FText ErrorMessage = FText::Format(LOCTEXT("MissingDataPin", "ICE: Pin @@ was expecting a data output pin named {0} on @@ (each delegate must have the same signature)"), FText::FromString(OutPin->PinName));
				CompilerContext.MessageLog.Error(*ErrorMessage.ToString(), OutPin, CurrentCENode);
				continue;
			}

			UK2Node_AssignmentStatement* AssignNode = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(CurrentNode, SourceGraph);
			AssignNode->AllocateDefaultPins();
			AssignNode->GetVariablePin()->DefaultValue = OutPin->DefaultValue;
			Schema->TryCreateConnection(LastActivatedNodeThen, AssignNode->GetExecPin());

			// Move connections from fake UK2Node_AddDelegate pint to this assignment node
			Schema->TryCreateConnection(TempVarOutput->GetVariablePin(), AssignNode->GetVariablePin());
			AssignNode->NotifyPinConnectionListChanged(AssignNode->GetVariablePin());

			Schema->TryCreateConnection(AssignNode->GetValuePin(), PinWithData);
			AssignNode->NotifyPinConnectionListChanged(AssignNode->GetValuePin());

			// Set correct index for K2Node_Select node to return correct value from delegate
			UK2Node_AssignmentStatement* TempIndexVarInitialize = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(CurrentNode, SourceGraph);
			TempIndexVarInitialize->AllocateDefaultPins();
			TempIndexVarInitialize->GetVariablePin()->PinType = TempIndexVar->GetVariablePin()->PinType;
			TempIndexVarInitialize->GetVariablePin()->MakeLinkTo(TempIndexVar->GetVariablePin());
			TempIndexVarInitialize->PinConnectionListChanged(TempIndexVarInitialize->GetVariablePin());
			TempIndexVarInitialize->GetValuePin()->PinType = TempIndexVar->GetVariablePin()->PinType;
			TempIndexVarInitialize->GetValuePin()->DefaultValue = FString::Printf(TEXT("%d"), Index);
			Schema->TryCreateConnection(AssignNode->GetThenPin(), TempIndexVarInitialize->GetExecPin());
			LastActivatedNodeThen = TempIndexVarInitialize->GetThenPin();

			TempIndexVar->GetVariablePin()->MakeLinkTo(OutputSelectNodes[OutIndex]->GetIndexPin());

			TArray<UEdGraphPin*> OptionPins;
			OutputSelectNodes[OutIndex]->GetOptionPins(OptionPins);
			Schema->TryCreateConnection(TempVarOutput->GetVariablePin(), OptionPins[Index]);
		}

		// connect CE output with OnSuccess output of AIMoveTo node
		CompilerContext.MovePinLinksToIntermediate(*PinForCurrentDelegateProperty, *LastActivatedNodeThen);
	}

	for (int32 OutIndex = 0; OutIndex < VariableOutputPins.Num(); ++OutIndex)
	{
		UEdGraphPin* OutPin = VariableOutputPins[OutIndex];
		TArray<UEdGraphPin*> OptionPins;
		OutputSelectNodes[OutIndex]->GetOptionPins(OptionPins);
		CompilerContext.MovePinLinksToIntermediate(*OutPin, *OutputSelectNodes[OutIndex]->GetReturnValuePin());
	}

	if (LastThenPin != NULL)
	{
		CompilerContext.MovePinLinksToIntermediate(*CurrentNode->FindPinChecked(Schema->PN_Then), *LastThenPin);
	}
	else
	{
		CompilerContext.MovePinLinksToIntermediate(*CurrentNode->FindPinChecked(Schema->PN_Then), *CallCreateMoveToProxyObjectNode->FindPinChecked(Schema->PN_Then));
	}

	CurrentNode->BreakAllNodeLinks();
}

bool UK2Node_BaseAsyncTask::HasExternalBlueprintDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	const UBlueprint* SourceBlueprint = GetBlueprint();

	const bool bProxyFactoryResult = (ProxyFactoryClass != NULL) && (ProxyFactoryClass->ClassGeneratedBy != NULL) && (ProxyFactoryClass->ClassGeneratedBy != SourceBlueprint);
	if (bProxyFactoryResult && OptionalOutput)
	{
		OptionalOutput->Add(ProxyFactoryClass);
	}

	const bool bProxyResult = (ProxyClass != NULL) && (ProxyClass->ClassGeneratedBy != NULL) && (ProxyClass->ClassGeneratedBy != SourceBlueprint);
	if (bProxyResult && OptionalOutput)
	{
		OptionalOutput->Add(ProxyClass);
	}

	return bProxyFactoryResult || bProxyResult;
}

#undef LOCTEXT_NAMESPACE
