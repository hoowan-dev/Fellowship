// Copyright HoowanDev 2026 all rights reserved.

#include "FSAnimBlueprintSetupLibrary.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequenceBase.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_VariableGet.h"
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

TArray<FString> UFSAnimBlueprintSetupLibrary::DescribeGraph(UAnimBlueprint* AnimBlueprint, const FString& GraphName)
{
	TArray<FString> Result;
	if (!AnimBlueprint)
	{
		return Result;
	}

	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	for (const UEdGraph* Graph : Graphs)
	{
		if (Graph->GetName() != GraphName)
		{
			continue;
		}

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			Result.Add(FString::Printf(TEXT("%s [%s] in %s"), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Node->GetClass()->GetName(), *GetNameSafe(Graph->GetOuter())));
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				FString Links;
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					Links += FString::Printf(TEXT(" -> %s.%s"), *Linked->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Linked->PinName.ToString());
				}
				Result.Add(FString::Printf(TEXT("    %s %s (%s) default='%s'%s"), Pin->Direction == EGPD_Input ? TEXT("in ") : TEXT("out"), *Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString(),
					*Pin->GetDefaultAsString(), *Links));
			}
		}
	}

	return Result;
}

bool UFSAnimBlueprintSetupLibrary::BlendStateIntoSequence(UAnimBlueprint* AnimBlueprint, const FString& StateGraphName, UAnimSequenceBase* Sequence, FName AlphaVariable)
{
	if (!AnimBlueprint || !Sequence)
	{
		return false;
	}

	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	UEdGraph* const* Found = Graphs.FindByPredicate([&StateGraphName](const UEdGraph* Graph) { return Graph->GetName() == StateGraphName; });
	UEdGraph* Graph = Found ? *Found : nullptr;
	if (!Graph || Graph->Nodes.ContainsByPredicate([](const UEdGraphNode* Node) { return Node->IsA<UAnimGraphNode_TwoWayBlend>(); }))
	{
		return false;
	}

	UAnimGraphNode_StateResult* ResultNode = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		ResultNode = ResultNode ? ResultNode : Cast<UAnimGraphNode_StateResult>(Node);
	}

	UEdGraphPin* ResultPin = ResultNode ? ResultNode->FindPin(TEXT("Result"), EGPD_Input) : nullptr;
	UEdGraphPin* PlayedPin = ResultPin && ResultPin->LinkedTo.Num() == 1 ? ResultPin->LinkedTo[0] : nullptr;
	if (!PlayedPin)
	{
		return false;
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	Graph->Modify();
	const int32 X = ResultNode->NodePosX - 250;
	const int32 Y = ResultNode->NodePosY;

	FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> BlendCreator(*Graph);
	UAnimGraphNode_TwoWayBlend* Blend = BlendCreator.CreateNode();
	Blend->NodePosX = X;
	Blend->NodePosY = Y;
	BlendCreator.Finalize();

	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> PlayerCreator(*Graph);
	UAnimGraphNode_SequencePlayer* Player = PlayerCreator.CreateNode();
	Player->NodePosX = X - 350;
	Player->NodePosY = Y + 150;
	Player->Node.SetSequence(Sequence);
	Player->Node.SetLoopAnimation(true);
	PlayerCreator.Finalize();

	FGraphNodeCreator<UK2Node_VariableGet> AlphaCreator(*Graph);
	UK2Node_VariableGet* Alpha = AlphaCreator.CreateNode();
	Alpha->VariableReference.SetSelfMember(AlphaVariable);
	Alpha->NodePosX = X - 250;
	Alpha->NodePosY = Y + 300;
	AlphaCreator.Finalize();

	UEdGraphPin* AlphaOut = Alpha->FindPin(AlphaVariable, EGPD_Output);
	if (!AlphaOut)
	{
		return false;
	}

	ResultPin->BreakAllPinLinks();
	const bool bConnected = Schema->TryCreateConnection(PlayedPin, Blend->FindPin(TEXT("A"), EGPD_Input))
		&& Schema->TryCreateConnection(Player->FindPin(TEXT("Pose"), EGPD_Output), Blend->FindPin(TEXT("B"), EGPD_Input))
		&& Schema->TryCreateConnection(AlphaOut, Blend->FindPin(TEXT("Alpha"), EGPD_Input))
		&& Schema->TryCreateConnection(Blend->FindPin(TEXT("Pose"), EGPD_Output), ResultPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	return bConnected;
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
