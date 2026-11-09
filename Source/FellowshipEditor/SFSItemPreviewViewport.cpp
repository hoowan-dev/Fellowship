// Copyright HoowanDev 2026 all rights reserved.

#include "SFSItemPreviewViewport.h"
#include "AdvancedPreviewScene.h"
#include "AnimationRuntime.h"
#include "Character/FSCharacter.h"
#include "Character/FSEquipmentComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemDefinition.h"
#include "Inventory/FSItemPickup.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SFSItemPreviewViewport"

namespace
{
	// What the game's own Blueprints and data are, for the parts of the preview that aren't the item
	const TCHAR* CharacterClassPath = TEXT("/Game/Blueprints/Character/BP_FSCharacter.BP_FSCharacter_C");
	const TCHAR* InventoryRigClassPath = TEXT("/Game/Blueprints/Inventory/BP_FSInventoryRig.BP_FSInventoryRig_C");
	const TCHAR* DefaultBackpackPath = TEXT("/Game/Data/Items/Backpack/DA_Item_Backpack_01.DA_Item_Backpack_01");
	const TCHAR* PlaneMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");

	// The engine's plane mesh is 100cm square and faces +Z
	constexpr double PlaneMeshSize = 100.0;

	// How much wider than the item the floor under a dropped one is
	constexpr double DiscardedFloorScale = 4.0;

	// The floor the character stands on
	constexpr double CharacterFloorSize = 300.0;

	// The character's arms and what they hold reach this far out from the middle of its capsule, as a fraction of its half height
	constexpr double CharacterReachFraction = 0.6;

	// Items on the inventory grid are only a few centimetres across, which the editor's usual near plane would cut through
	constexpr float NearClipPlane = 0.5f;

	// Where each view looks from at first. The inventory always looks along +X at the grid's face, with +Y to the right and +Z up, and the backpack from behind its wearer's right, since backpack meshes face +Y.
	FRotator GetInitialViewRotation(EFSItemPreviewView View)
	{
		switch (View)
		{
		case EFSItemPreviewView::Inventory: return FRotator(0.0, 0.0, 0.0);
		case EFSItemPreviewView::Backpack: return FRotator(-15.0, 60.0, 0.0);
		case EFSItemPreviewView::Character: return FRotator(-10.0, -150.0, 0.0);
		default: return FRotator(-30.0, -135.0, 0.0);
		}
	}

	TSubclassOf<AFSCharacter> GetCharacterClass()
	{
		UClass* Class = LoadClass<AFSCharacter>(nullptr, CharacterClassPath);
		return Class ? Class : AFSCharacter::StaticClass();
	}

	const AFSInventoryRig* GetInventoryRig()
	{
		const UClass* Class = LoadClass<AFSInventoryRig>(nullptr, InventoryRigClassPath);
		return GetDefault<AFSInventoryRig>(Class ? Class : AFSInventoryRig::StaticClass());
	}

	// Where a socket or bone is relative to the mesh, in the pose the mesh was modeled in
	FTransform GetRefPoseTransform(const USkeletalMesh& Mesh, FName SocketOrBone)
	{
		FTransform FromBone = FTransform::Identity;
		FName Bone = SocketOrBone;

		if (const USkeletalMeshSocket* Socket = Mesh.FindSocket(SocketOrBone))
		{
			FromBone = Socket->GetSocketLocalTransform();
			Bone = Socket->BoneName;
		}

		const FReferenceSkeleton& Skeleton = Mesh.GetRefSkeleton();
		const int32 BoneIndex = Skeleton.FindBoneIndex(Bone);
		return BoneIndex == INDEX_NONE ? FromBone : FromBone * FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, BoneIndex);
	}

	// Ticks the preview world, which nothing else does. Only what ticks in the editor moves, which keeps the character from falling with no floor to stand on, but still animates it.
	class FFSItemPreviewViewportClient : public FEditorViewportClient
	{
	public:
		using FEditorViewportClient::FEditorViewportClient;

		virtual void Tick(float DeltaSeconds) override
		{
			FEditorViewportClient::Tick(DeltaSeconds);

			if (PreviewScene && !GIntraFrameDebuggingGameThread)
			{
				PreviewScene->GetWorld()->Tick(LEVELTICK_ViewportsOnly, DeltaSeconds);
			}
		}
	};
}

SFSItemPreviewViewport::SFSItemPreviewViewport() = default;

void SFSItemPreviewViewport::Construct(const FArguments& InArgs, UFSItemDefinition* InItem, EFSItemPreviewView InView)
{
	Item = InItem;
	View = InView;

	// Before the viewport is constructed, because that's what makes the client that shows this scene
	PreviewScene = MakeUnique<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());

	// Meshes have their pivots wherever suited the character holding them, so a floor at the pivot would cut through them. The views that need one have their own.
	PreviewScene->SetFloorVisibility(false, true);

	Refresh();

	SEditorViewport::Construct(SEditorViewport::FArguments());

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SFSItemPreviewViewport::HandleObjectPropertyChanged);
}

SFSItemPreviewViewport::~SFSItemPreviewViewport()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);

	if (ViewportClient)
	{
		ViewportClient->Viewport = nullptr;
	}
}

FText SFSItemPreviewViewport::GetViewName(EFSItemPreviewView View)
{
	switch (View)
	{
	case EFSItemPreviewView::Inventory: return LOCTEXT("InventoryView", "Inventory");
	case EFSItemPreviewView::Backpack: return LOCTEXT("BackpackView", "Stored");
	case EFSItemPreviewView::Character: return LOCTEXT("CharacterView", "Equipped");
	default: return LOCTEXT("DiscardedView", "Discarded");
	}
}

TSharedRef<FEditorViewportClient> SFSItemPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FFSItemPreviewViewportClient(nullptr, PreviewScene.Get(), SharedThis(this)));
	ViewportClient->SetViewportType(LVT_Perspective);
	ViewportClient->SetViewMode(VMI_Lit);
	ViewportClient->SetRealtime(true);
	// The inventory is only ever seen head on, so its view can't be turned, only moved and zoomed
	ViewportClient->ToggleOrbitCamera(View != EFSItemPreviewView::Inventory);
	ViewportClient->OverrideNearClipPlane(NearClipPlane);
	ViewportClient->SetViewRotation(GetInitialViewRotation(View));
	return ViewportClient.ToSharedRef();
}

void SFSItemPreviewViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	Overlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(8.0f)
		[
			SNew(STextBlock)
			.Text(GetViewName(View))
			.ShadowOffset(FVector2D(1.0, 1.0))
		];
}

void SFSItemPreviewViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SEditorViewport::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Looking along +X at the grid's face, as the inventory screen's camera does, whatever dragging in the viewport tried to turn it
	if (View == EFSItemPreviewView::Inventory && ViewportClient)
	{
		ViewportClient->SetViewRotation(GetInitialViewRotation(View));
	}

	if (!bFramePending || !ViewportClient || !ViewportClient->Viewport || ViewportClient->Viewport->GetSizeXY().X <= 0)
	{
		return;
	}

	const FBox Box = GetFramingBox();
	if (!Box.IsValid)
	{
		return;
	}

	bFramePending = false;

	// Focusing fits a box to the viewport's width, which would cut the ends off a long item in a wide viewport
	const FIntPoint Size = ViewportClient->Viewport->GetSizeXY();
	const double Aspect = FMath::Max(static_cast<double>(Size.X) / FMath::Max(Size.Y, 1), 1.0);
	ViewportClient->FocusViewportOnBox(FBox::BuildAABB(Box.GetCenter(), Box.GetExtent() * Aspect), true);
}

void SFSItemPreviewViewport::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (Object != Item.Get())
	{
		return;
	}

	// Only what changes the size or the kind of what's shown moves the camera, so that it holds still while a transform is being tweaked
	const FName Property = Event.GetMemberPropertyName();
	if (Property == GET_MEMBER_NAME_CHECKED(UFSItemDefinition, Mesh) || Property == GET_MEMBER_NAME_CHECKED(UFSItemDefinition, InventoryDimensions)
		|| Property == GET_MEMBER_NAME_CHECKED(UFSItemDefinition, ItemType))
	{
		bFramePending = true;
	}

	Refresh();
}

void SFSItemPreviewViewport::Refresh()
{
	switch (View)
	{
	case EFSItemPreviewView::Discarded: RefreshDiscarded(); break;
	case EFSItemPreviewView::Inventory: RefreshInventory(); break;
	case EFSItemPreviewView::Backpack: RefreshBackpack(); break;
	case EFSItemPreviewView::Character: RefreshCharacter(); break;
	}
}

void SFSItemPreviewViewport::RefreshDiscarded()
{
	const UFSItemDefinition* PreviewedItem = Item.Get();
	UStaticMesh* ItemMesh = PreviewedItem ? PreviewedItem->Mesh.Get() : nullptr;

	// The floor is at zero, and the pickup sits above it, with its mesh placed relative to it, as when it's dropped
	const FTransform Resting = ItemMesh
		? AFSItemPickup::GetDiscardedMeshTransform(*PreviewedItem) * FTransform(FVector(0.0, 0.0, AFSItemPickup::GetFloorClearance(*PreviewedItem)))
		: FTransform::Identity;
	ShowMesh(*GetMesh(0), ItemMesh, Resting);

	// Under the middle of the item, wherever its offset has moved it
	const FBox ItemBox = GetMesh(0)->Bounds.GetBox();
	const double FloorSize = FMath::Max(ItemBox.GetSize().GetMax() * DiscardedFloorScale, 10.0);
	const FVector FloorCentre(ItemBox.GetCenter().X, ItemBox.GetCenter().Y, 0.0);
	ShowMesh(*GetMesh(1), LoadObject<UStaticMesh>(nullptr, PlaneMeshPath), FTransform(FQuat::Identity, FloorCentre, FVector(FloorSize / PlaneMeshSize)));

	HideMeshesFrom(2);
}

void SFSItemPreviewViewport::RefreshInventory()
{
	const UFSItemDefinition* PreviewedItem = Item.Get();
	const AFSInventoryRig* Rig = GetInventoryRig();
	const double CellSize = Rig->GetCellSize();
	const FIntPoint GridSize = PreviewedItem ? PreviewedItem->InventoryDimensions.ComponentMax(FIntPoint(1, 1)) : FIntPoint(1, 1);

	if (!GridCells)
	{
		GridCells.Reset(NewObject<UInstancedStaticMeshComponent>(GetTransientPackage()));
		GridCells->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, PlaneMeshPath));
		GridCells->SetNumCustomDataFloats(AFSInventoryRig::CellDataCount);
		PreviewScene->AddComponent(GridCells.Get(), FTransform::Identity);
	}

	if (UMaterialInterface* CellMaterial = Rig->GetGridCellMaterial())
	{
		GridCells->SetMaterial(0, CellMaterial);
	}

	// Exactly the item's cells, all of them covered by it, as the grid shows an item at its top left
	GridCells->ClearInstances();
	for (int32 Row = 0; Row < GridSize.Y; ++Row)
	{
		for (int32 Column = 0; Column < GridSize.X; ++Column)
		{
			const int32 Cell = GridCells->AddInstance(AFSInventoryRig::GetCellTransform(FIntPoint(Column, Row), GridSize, CellSize));
			if (PreviewedItem && PreviewedItem->Mesh)
			{
				AFSInventoryRig::SetCellTakenBy(*GridCells, Cell, *PreviewedItem);
			}
		}
	}

	UStaticMesh* ItemMesh = PreviewedItem ? PreviewedItem->Mesh.Get() : nullptr;
	const FVector2D TopLeft(-GridSize.X * 0.5 * CellSize, GridSize.Y * 0.5 * CellSize);
	ShowMesh(*GetMesh(0), ItemMesh, ItemMesh ? AFSInventoryRig::GetItemTransformOnGrid(*PreviewedItem, 0, TopLeft, CellSize, Rig->GetItemFillFraction()) : FTransform::Identity);

	HideMeshesFrom(1);
}

void SFSItemPreviewViewport::RefreshBackpack()
{
	UFSItemDefinition* PreviewedItem = Item.Get();
	const bool bIsBackpack = PreviewedItem && PreviewedItem->ItemType == EFSItemType::Backpack;
	const UFSItemDefinition* Backpack = bIsBackpack ? PreviewedItem : LoadObject<UFSItemDefinition>(nullptr, DefaultBackpackPath);
	UStaticMesh* BackpackMesh = Backpack ? Backpack->Mesh.Get() : nullptr;

	// Backpack meshes are made with their pivot at the bone they're worn on
	ShowMesh(*GetMesh(0), BackpackMesh, FTransform::Identity);

	UStaticMesh* ItemMesh = !bIsBackpack && BackpackMesh && PreviewedItem ? PreviewedItem->Mesh.Get() : nullptr;
	int32 NextMesh = 1;

	if (ItemMesh)
	{
		const AFSCharacter* CharacterDefaults = GetDefault<AFSCharacter>(GetCharacterClass());
		const UFSEquipmentComponent* Equipment = CharacterDefaults->GetEquipment();
		const USkeletalMesh* CharacterMesh = CharacterDefaults->GetMesh() ? CharacterDefaults->GetMesh()->GetSkeletalMeshAsset() : nullptr;

		// Sheathed in each hand, where the equipment puts it on the character: relative to the sheath socket, which is found relative to the backpack's bone
		if (PreviewedItem->bShowHandStored && Equipment && CharacterMesh)
		{
			const FTransform BackpackBone = GetRefPoseTransform(*CharacterMesh, Equipment->BackpackBone);

			for (const EFSHand Hand : { EFSHand::Right, EFSHand::Left })
			{
				const bool bRight = Hand == EFSHand::Right;
				const FTransform Socket = GetRefPoseTransform(*CharacterMesh, bRight ? Equipment->RightSheathSocket : Equipment->LeftSheathSocket);
				const FTransform& SheathOffset = bRight ? Equipment->RightSheathOffset : Equipment->LeftSheathOffset;
				const FTransform OnCharacter = PreviewedItem->HandStoredTransform * SheathOffset * Socket;

				ShowMesh(*GetMesh(NextMesh++), ItemMesh, OnCharacter.GetRelativeTransform(BackpackBone));
			}
		}

		// Hanging off each side, as far down as it's allowed on the character
		if (PreviewedItem->bShowQuickStored)
		{
			const double MaxDrop = Equipment ? Equipment->HungItemMaxDrop : 0.0;

			for (const EFSQuickSlot QuickSlot : { EFSQuickSlot::Left, EFSQuickSlot::Right })
			{
				ShowMesh(*GetMesh(NextMesh++), ItemMesh, FFSHungItemMeshes::GetTransform(*Backpack, *PreviewedItem, QuickSlot, MaxDrop));
			}
		}
	}

	HideMeshesFrom(NextMesh);
}

void SFSItemPreviewViewport::RefreshCharacter()
{
	UWorld* World = PreviewScene->GetWorld();

	if (!Character.IsValid())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;

		AFSCharacter* NewCharacter = World->SpawnActor<AFSCharacter>(GetCharacterClass(), FTransform::Identity, SpawnParameters);
		if (!NewCharacter)
		{
			return;
		}

		Character = NewCharacter;

		// Standing on the floor at zero
		NewCharacter->SetActorLocation(FVector(0.0, 0.0, NewCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

		// Walking rather than falling, which decides the animation. Its movement doesn't tick in the editor, so it stays put.
		NewCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// Only the character and what's put on it show, not its capsule, camera and so on
		USkeletalMeshComponent* CharacterMesh = NewCharacter->GetMesh();
		for (UPrimitiveComponent* Component : TInlineComponentArray<UPrimitiveComponent*>(NewCharacter))
		{
			if (Component != CharacterMesh && !Component->IsAttachedTo(CharacterMesh))
			{
				Component->SetVisibility(false);
			}
		}

		// Its tick is what keeps skeletal armor in the character's pose
		NewCharacter->GetEquipment()->bTickInEditor = true;

		ShowMesh(*GetMesh(0), LoadObject<UStaticMesh>(nullptr, PlaneMeshPath), FTransform(FQuat::Identity, FVector::ZeroVector, FVector(CharacterFloorSize / PlaneMeshSize)));
	}

	// A fresh inventory each time, since the item may now go somewhere else entirely
	UFSItemDefinition* PreviewedItem = Item.Get();
	Inventory.Reset(NewObject<UFSInventoryComponent>(GetTransientPackage()));

	if (PreviewedItem)
	{
		if (PreviewedItem->ItemType == EFSItemType::Backpack)
		{
			Inventory->EquipBackpack(PreviewedItem);
		}
		else
		{
			// Armor is worn as well as held, so that both can be seen
			if (PreviewedItem->ItemType == EFSItemType::Armor && Inventory->CarryNewItem(PreviewedItem))
			{
				Inventory->SwapCarriedItemWithArmorSlot(PreviewedItem->ArmorType);
				Inventory->ClearCarriedItem();
			}

			for (const EFSHand Hand : { EFSHand::Right, EFSHand::Left })
			{
				if (Inventory->CarryNewItem(PreviewedItem))
				{
					Inventory->SwapCarriedItemWithHand(Hand);
				}
			}
		}

		Inventory->ClearCarriedItem();
	}

	UFSEquipmentComponent* Equipment = Character->GetEquipment();
	Equipment->SetInventory(Inventory.Get());

	// Does nothing with empty hands, which leaves the character unarmed
	Equipment->Draw();
}

UStaticMeshComponent* SFSItemPreviewViewport::GetMesh(int32 Index)
{
	while (Meshes.Num() <= Index)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetTransientPackage());
		Component->SetMobility(EComponentMobility::Movable);
		PreviewScene->AddComponent(Component, FTransform::Identity);
		Meshes.Emplace(Component);
	}

	return Meshes[Index].Get();
}

void SFSItemPreviewViewport::HideMeshesFrom(int32 Index)
{
	for (int32 MeshIndex = Index; MeshIndex < Meshes.Num(); ++MeshIndex)
	{
		ShowMesh(*Meshes[MeshIndex], nullptr, FTransform::Identity);
	}
}

void SFSItemPreviewViewport::ShowMesh(UStaticMeshComponent& Component, UStaticMesh* Mesh, const FTransform& Transform)
{
	Component.SetStaticMesh(Mesh);
	Component.SetRelativeTransform(Transform);
	Component.SetVisibility(Mesh != nullptr);
	Component.MarkRenderStateDirty();
}

FBox SFSItemPreviewViewport::GetFramingBox() const
{
	FBox Box(ForceInit);

	if (View == EFSItemPreviewView::Character)
	{
		const AFSCharacter* PreviewCharacter = Character.Get();
		if (!PreviewCharacter)
		{
			return Box;
		}

		// The capsule, wide enough for the arms, rather than the mesh's bounds, which are wider for the pose it starts in than the one it's animated into
		const UCapsuleComponent* Capsule = PreviewCharacter->GetCapsuleComponent();
		const double HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const double Reach = FMath::Max(static_cast<double>(Capsule->GetScaledCapsuleRadius()), HalfHeight * CharacterReachFraction);
		Box += FBox::BuildAABB(Capsule->GetComponentLocation(), FVector(Reach, Reach, HalfHeight));

		for (const EFSHand Hand : { EFSHand::Left, EFSHand::Right })
		{
			const UStaticMeshComponent* HandMesh = PreviewCharacter->GetEquipment()->GetHandMesh(Hand);
			if (HandMesh && HandMesh->IsVisible() && HandMesh->GetStaticMesh())
			{
				Box += HandMesh->Bounds.GetBox();
			}
		}

		return Box;
	}

	// The floor under a dropped item is only there to rest it on
	const int32 FramedMeshes = View == EFSItemPreviewView::Discarded ? 1 : Meshes.Num();

	for (int32 Index = 0; Index < FMath::Min(FramedMeshes, Meshes.Num()); ++Index)
	{
		const UStaticMeshComponent* Component = Meshes[Index].Get();
		if (Component->IsVisible() && Component->GetStaticMesh())
		{
			Box += Component->Bounds.GetBox();
		}
	}

	if (View == EFSItemPreviewView::Inventory && GridCells && GridCells->GetInstanceCount() > 0)
	{
		Box += GridCells->Bounds.GetBox();
	}

	return Box;
}

#undef LOCTEXT_NAMESPACE
