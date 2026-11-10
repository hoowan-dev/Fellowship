// Copyright HoowanDev 2026 all rights reserved.

#include "FSAnimBlueprintSetupLibrary.h"
#include "AnimGraphNode_Base.h"
#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
	// Calls Visit with each matching node's runtime struct and the property inside it
	template<typename FunctionType>
	int32 ForEachAnimNodeProperty(UAnimBlueprint* AnimBlueprint, FName NodeStructName, FName PropertyName, FunctionType Visit)
	{
		if (!AnimBlueprint)
		{
			return 0;
		}

		TArray<UAnimGraphNode_Base*> GraphNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UAnimGraphNode_Base>(AnimBlueprint, GraphNodes);

		int32 Count = 0;
		for (UAnimGraphNode_Base* GraphNode : GraphNodes)
		{
			const FStructProperty* NodeProperty = GraphNode->GetFNodeProperty();
			if (!NodeProperty || NodeProperty->Struct->GetFName() != NodeStructName)
			{
				continue;
			}

			const FProperty* Property = NodeProperty->Struct->FindPropertyByName(PropertyName);
			if (!Property)
			{
				continue;
			}

			void* NodeData = NodeProperty->ContainerPtrToValuePtr<void>(GraphNode);
			if (Visit(*GraphNode, *Property, Property->ContainerPtrToValuePtr<void>(NodeData)))
			{
				++Count;
			}
		}

		return Count;
	}
}

int32 UFSAnimBlueprintSetupLibrary::SetAnimNodeProperty(UAnimBlueprint* AnimBlueprint, FName NodeStructName, FName PropertyName, const FString& Value)
{
	const int32 Count = ForEachAnimNodeProperty(AnimBlueprint, NodeStructName, PropertyName, [&Value](UAnimGraphNode_Base& GraphNode, const FProperty& Property, void* Data)
	{
		GraphNode.Modify();
		return Property.ImportText_Direct(*Value, Data, &GraphNode, PPF_None) != nullptr;
	});

	if (Count > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBlueprint);
	}

	return Count;
}

TArray<FString> UFSAnimBlueprintSetupLibrary::ListAnimNodes(UAnimBlueprint* AnimBlueprint)
{
	TArray<FString> Result;
	if (!AnimBlueprint)
	{
		return Result;
	}

	TArray<UAnimGraphNode_Base*> GraphNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass<UAnimGraphNode_Base>(AnimBlueprint, GraphNodes);
	for (const UAnimGraphNode_Base* GraphNode : GraphNodes)
	{
		const FStructProperty* NodeProperty = GraphNode->GetFNodeProperty();
		Result.Add(FString::Printf(TEXT("%s (%s)"), NodeProperty ? *NodeProperty->Struct->GetName() : TEXT("?"), *GetNameSafe(GraphNode->GetGraph())));
	}

	return Result;
}

FString UFSAnimBlueprintSetupLibrary::GetAnimNodeProperty(UAnimBlueprint* AnimBlueprint, FName NodeStructName, FName PropertyName)
{
	FString Result;
	ForEachAnimNodeProperty(AnimBlueprint, NodeStructName, PropertyName, [&Result](UAnimGraphNode_Base& GraphNode, const FProperty& Property, void* Data)
	{
		if (Result.IsEmpty())
		{
			Property.ExportTextItem_Direct(Result, Data, nullptr, &GraphNode, PPF_None);
		}
		return true;
	});

	return Result;
}
