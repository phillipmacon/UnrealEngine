// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationBlueprintEditorModule.h"
#include "AnimationGraphFactory.h"
#include "Animation/AnimInstance.h"
#include "AnimationBlueprintEditor.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Animation/AnimBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AnimBlueprintCompiler.h"

IMPLEMENT_MODULE( FAnimationBlueprintEditorModule, AnimationBlueprintEditor);

#define LOCTEXT_NAMESPACE "AnimationBlueprintEditorModule"

void FAnimationBlueprintEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	AnimGraphNodeFactory = MakeShareable(new FAnimationGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(AnimGraphNodeFactory);

	AnimGraphPinFactory = MakeShareable(new FAnimationGraphPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(AnimGraphPinFactory);

	AnimGraphPinConnectionFactory = MakeShareable(new FAnimationGraphPinConnectionFactory());
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(AnimGraphPinConnectionFactory);

	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(this, UAnimInstance::StaticClass(), FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &FAnimationBlueprintEditorModule::OnNewBlueprintCreated));

	// Register widget blueprint compiler we do this no matter what.
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Add(&AnimBlueprintCompiler);

	FBlueprintEditorModule::RegisterCompilerGetter(UAnimBlueprint::StaticClass(), FOnGetBlueprintCompiler::CreateStatic(&FAnimationBlueprintEditorModule::GetCompiler));
}

void FAnimationBlueprintEditorModule::ShutdownModule()
{
	IKismetCompilerInterface* KismetCompilerModule = FModuleManager::GetModulePtr<IKismetCompilerInterface>("KismetCompiler");
	if (KismetCompilerModule)
	{
		KismetCompilerModule->GetCompilers().Remove(&AnimBlueprintCompiler);
	}

	FBlueprintEditorModule::UnregisterCompilerGetter(UAnimBlueprint::StaticClass());
	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	FEdGraphUtilities::UnregisterVisualNodeFactory(AnimGraphNodeFactory);
	FEdGraphUtilities::UnregisterVisualPinFactory(AnimGraphPinFactory);
	FEdGraphUtilities::UnregisterVisualPinConnectionFactory(AnimGraphPinConnectionFactory);

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();
}

TSharedRef<IAnimationBlueprintEditor> FAnimationBlueprintEditorModule::CreateAnimationBlueprintEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, class UAnimBlueprint* InAnimBlueprint)
{
	TSharedRef< FAnimationBlueprintEditor > NewAnimationBlueprintEditor( new FAnimationBlueprintEditor() );
	NewAnimationBlueprintEditor->InitAnimationBlueprintEditor( Mode, InitToolkitHost, InAnimBlueprint);
	return NewAnimationBlueprintEditor;
}

void FAnimationBlueprintEditorModule::OnNewBlueprintCreated(UBlueprint* InBlueprint)
{
	if (ensure(InBlueprint->UbergraphPages.Num() > 0))
	{
		UEdGraph* EventGraph = InBlueprint->UbergraphPages[0];

		int32 SafeXPosition = 0;
		int32 SafeYPosition = 0;

		if (EventGraph->Nodes.Num() != 0)
		{
			SafeXPosition = EventGraph->Nodes[0]->NodePosX;
			SafeYPosition = EventGraph->Nodes[EventGraph->Nodes.Num() - 1]->NodePosY + EventGraph->Nodes[EventGraph->Nodes.Num() - 1]->NodeHeight + 100;
		}

		// add try get owner node
		UK2Node_CallFunction* GetOwnerNode = NewObject<UK2Node_CallFunction>(EventGraph);
		UFunction* MakeNodeFunction = UAnimInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAnimInstance, TryGetPawnOwner));
		GetOwnerNode->CreateNewGuid();
		GetOwnerNode->PostPlacedNewNode();
		GetOwnerNode->SetFromFunction(MakeNodeFunction);
		GetOwnerNode->SetFlags(RF_Transactional);
		GetOwnerNode->AllocateDefaultPins();
		GetOwnerNode->NodePosX = SafeXPosition;
		GetOwnerNode->NodePosY = SafeYPosition;
		UEdGraphSchema_K2::SetNodeMetaData(GetOwnerNode, FNodeMetadata::DefaultGraphNode);
		GetOwnerNode->MakeAutomaticallyPlacedGhostNode();

		EventGraph->AddNode(GetOwnerNode);
	}
}

TSharedPtr<FKismetCompilerContext> FAnimationBlueprintEditorModule::GetCompiler(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new FAnimBlueprintCompilerContext(CastChecked<UAnimBlueprint>(BP), InMessageLog, InCompileOptions, nullptr));
}


#undef LOCTEXT_NAMESPACE
