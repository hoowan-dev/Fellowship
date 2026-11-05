// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSInventoryRig.generated.h"

class ACharacter;
class UCameraComponent;
class UFSInventoryComponent;
class UFSItemDefinition;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UPoseableMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FFSInventoryItem;
enum class EFSHand : uint8;
enum class EFSQuickSlot : uint8;

/**
 * What the player sees with the inventory open: a first person camera looking at the held backpack, with the grid floating between them,
 * the player's forearms raised either side of it with a large cell over each hand, a cell for each quick slot above it, and a button below it that drops the backpack.
 * Without a backpack only the forearms and the hands' cells are there.
 * Exists only on the owning client. The grid's cells run from (0, 0) at the top left.
 */
UCLASS()
class FELLOWSHIP_API AFSInventoryRig : public AActor
{
	GENERATED_BODY()

public:
	// The instance custom data each grid cell has, which M_InventoryGridCell reads. Each is 1 or 0.
	static constexpr int32 CellHoveredData = 0;
	static constexpr int32 CellTakenData = 1;
	static constexpr int32 CellCanPlaceData = 2;
	static constexpr int32 CellCannotPlaceData = 3;
	static constexpr int32 CellCanSwapData = 4;
	static constexpr int32 CellDataCount = 5;

	// The instance of the hand slots each hand's cell is
	static constexpr int32 LeftHandSlot = 0;
	static constexpr int32 RightHandSlot = 1;
	static constexpr int32 HandSlotCount = 2;

	// Which hand a hand slot is for
	static EFSHand GetHandOfSlot(int32 HandSlot);

	// The instance of the quick slots each quick slot's cell is
	static constexpr int32 LeftQuickSlot = 0;
	static constexpr int32 RightQuickSlot = 1;
	static constexpr int32 QuickSlotCount = 2;

	// Which of the inventory's quick slots an instance of the quick slots is for
	static EFSQuickSlot GetQuickSlotOfIndex(int32 QuickSlotIndex);

	AFSInventoryRig();

	virtual void Tick(float DeltaTime) override;

	// Moves the rig to the character's head, facing the way the character faces, and keeps it there. The arms take on the character's mesh.
	void AttachToCharacter(ACharacter* Character);

	void SetPackVisible(bool bVisible);

	// The grid scales in and out rather than appearing instantly
	void SetGridVisible(bool bVisible);

	// Finds the cell a world space ray passes through. Returns false if it misses the grid.
	bool GetCellFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutCell) const;

	// Highlights a cell, or every cell an item covers if there's one in it. While an item is carried, previews the cells it would
	// cover with its top left in this cell instead, showing whether it can go there. Any cell outside the grid clears the highlight.
	void SetHoveredCell(const FIntPoint& Cell);

	// Finds the hand slot a world space ray passes through. Returns false if it misses both.
	bool GetHandSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutHandSlot) const;

	// Highlights a hand slot, showing whether the carried item would go in it or swap with what's there. INDEX_NONE clears the highlight.
	void SetHoveredHandSlot(int32 HandSlot);

	// Finds the quick slot a world space ray passes through. Returns false if it misses both.
	bool GetQuickSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutQuickSlot) const;

	// Highlights a quick slot, showing whether the carried item would go in it or swap with what's there. INDEX_NONE clears the highlight.
	void SetHoveredQuickSlot(int32 QuickSlot);

	// Whether a world space ray passes through the button that drops the backpack. There's no button without a backpack.
	bool IsDropBackpackButtonOnWorldRay(const FVector& RayOrigin, const FVector& RayDirection) const;

	// Highlights the button that drops the backpack, or clears the highlight
	void SetDropBackpackButtonHovered(bool bHovered);

	FORCEINLINE bool IsDropBackpackButtonHovered() const { return bDropBackpackButtonHovered; }

	// Moves the light and the carried item to where the mouse points, given as a world space ray. The carried item leans the way the point has been moving on tick.
	void SetCursorRay(const FVector& RayOrigin, const FVector& RayDirection);

	// Matches the pack, the grid, the item meshes, their borders and the carried item to what the inventory holds
	void ShowInventory(const UFSInventoryComponent& Inventory);

	// Where a held item's mesh sits relative to the grid, upright and fitted to the hand's cell
	FTransform GetHandItemTransform(const FFSInventoryItem& Item, int32 HandSlot) const;

	// Where a quick slot's item's mesh sits relative to the grid, upright and fitted to the slot's cell
	FTransform GetQuickSlotItemTransform(const FFSInventoryItem& Item, int32 QuickSlot) const;

	// Where an item's mesh sits relative to the grid, fitted to the cells it covers
	FTransform GetItemTransform(const FFSInventoryItem& Item) const;

	// Where a carried item's mesh sits relative to the grid, with the top left of its cells at a point on the grid. It's closer to the camera
	// than every item in the grid, and shrunk by just enough to look the same size and in the same place as if it were on the grid.
	FTransform GetCarriedItemTransform(const FFSInventoryItem& Item, const FVector2D& GridPoint) const;

	FORCEINLINE const TArray<TObjectPtr<UStaticMeshComponent>>& GetItemMeshes() const { return ItemMeshes; }
	FORCEINLINE UStaticMeshComponent* GetCarriedItemMesh() const { return CarriedItemMesh; }
	FORCEINLINE UStaticMeshComponent* GetHandItemMesh(int32 HandSlot) const { return HandSlot == LeftHandSlot ? LeftHandItemMesh : RightHandItemMesh; }
	FORCEINLINE int32 GetHoveredHandSlot() const { return HoveredHandSlot; }
	FORCEINLINE UPointLightComponent* GetCursorLight() const { return CursorLight; }
	FORCEINLINE const FIntPoint& GetHoveredCell() const { return HoveredCell; }
	FORCEINLINE UInstancedStaticMeshComponent* GetGrid() const { return Grid; }
	FORCEINLINE UInstancedStaticMeshComponent* GetItemBorders() const { return ItemBorders; }
	FORCEINLINE int32 GetCellIndex(const FIntPoint& Cell) const { return Cell.Y * GridWidth + Cell.X; }
	FORCEINLINE UInstancedStaticMeshComponent* GetHandSlots() const { return HandSlots; }
	FORCEINLINE UInstancedStaticMeshComponent* GetQuickSlots() const { return QuickSlots; }
	FORCEINLINE UStaticMeshComponent* GetQuickSlotItemMesh(int32 QuickSlot) const { return QuickSlot == LeftQuickSlot ? LeftQuickSlotItemMesh : RightQuickSlotItemMesh; }
	FORCEINLINE int32 GetHoveredQuickSlot() const { return HoveredQuickSlot; }
	FORCEINLINE UPoseableMeshComponent* GetArms() const { return Arms; }
	FORCEINLINE UStaticMeshComponent* GetPack() const { return Pack; }
	FORCEINLINE UStaticMeshComponent* GetBackgroundDarkener() const { return BackgroundDarkener; }
	FORCEINLINE UCameraComponent* GetViewCamera() const { return ViewCamera; }

	// How far the carried item is leaning, in degrees: X to the right and Y up
	FORCEINLINE const FVector2D& GetCarriedItemSway() const { return CarriedItemSway; }
	FORCEINLINE UInstancedStaticMeshComponent* GetDropBackpackButton() const { return DropBackpackButton; }

	// Whether the last inventory shown had a backpack, which is what the grid, quick slots and button belong to. True until one is shown.
	FORCEINLINE bool HasBackpack() const { return bHasBackpack; }

	// The centre of a hand's cell on the plane of the grid, in cm from the grid's centre: X to the right and Y up
	FVector2D GetHandSlotCentre(int32 HandSlot) const;

	// The centre of a quick slot's cell on the plane of the grid, in cm from the grid's centre: X to the right and Y up
	FVector2D GetQuickSlotCentre(int32 QuickSlot) const;

	// The point in the world to centre a hand slot's name on, just above its cell
	FVector GetHandSlotLabelLocation(int32 HandSlot) const;

	// The point in the world to centre a quick slot's name on, just above its cell
	FVector GetQuickSlotLabelLocation(int32 QuickSlot) const;

	// The centre of the button that drops the backpack on the plane of the grid, in cm from the grid's centre: X to the right and Y up
	FVector2D GetDropBackpackButtonCentre() const;

	// The point in the world to centre the button's label on
	FVector GetDropBackpackButtonLabelLocation() const;

protected:
	virtual void BeginPlay() override;

	void BuildGrid();
	void BuildHandSlots();
	void BuildQuickSlots();
	void BuildDropBackpackButton();
	void PoseArms();
	void PoseArm(bool bLeft);

	// Shows the backpack's mesh in the background, turned by its Inventory Mesh Rotation. Null shows nothing.
	void FitPackMesh(const UFSItemDefinition* Backpack);

	// Shows the grid and everything on it if the display is showing and it's scaled in, except for what needs a backpack when there isn't one
	void UpdateGridVisibility();

	void AddItemBorder(const FFSInventoryItem& Item);
	bool IsCellInGrid(const FIntPoint& Cell) const;

	// Finds where a world space ray meets the plane of the grid, in cm from the grid's centre: X to the right and Y up. Returns false if it never does.
	bool GetGridPointFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FVector2D& OutGridPoint) const;
	FIntPoint GetCellFromGridPoint(const FVector2D& GridPoint) const;
	FTransform GetItemTransformAt(const FFSInventoryItem& Item, const FVector2D& TopLeft) const;
	void HighlightCells(TArray<FIntPoint>&& Cells, int32 CellData);
	FTransform GetSlotItemTransform(const FFSInventoryItem& Item, const FVector2D& SlotCentre, double SlotSize) const;
	FVector GetSlotLabelLocation(const FVector2D& SlotCentre, double SlotSize) const;
	FVector GetLocationOnGrid(const FVector2D& GridPoint) const;

	// Starts the carried item's mesh turning from how it looked at its old rotation to how it should look at its current one
	void StartCarriedItemTurn(const FFSInventoryItem& Item, uint8 OldRotation);

	// Moves the sway on by a frame, or settles it if nothing's carried
	void UpdateCarriedItemSway(float DeltaTime);

	// Places the carried item's mesh under the cursor, leaning and part way through any turn
	void UpdateCarriedItemMesh();

	// Places the meshes of the items in the hands and quick slots, swaying about their cells
	void UpdateSlotItemSway();

	// The turn of a slot's item at this moment. Phase is how far ahead of the others it is, as a fraction of a sway.
	FQuat GetSlotItemSwayRotation(double Phase) const;

	// Moves a highlight between the cells of one kind of slot. Hovered and HoveredData are that kind's record of what it has set.
	void SetHoveredSlot(UInstancedStaticMeshComponent& Slots, int32& Hovered, int32& HoveredData, int32 NewHovered, bool bNewHoveredTaken);

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UCameraComponent> ViewCamera;

	// Frame the pack and grid are placed in: X runs away from the camera, Y to the right, Z up
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<USceneComponent> Display;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> Pack;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> Grid;

	// Four bars around each item in the inventory. Attached to the grid so they scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> ItemBorders;

	// Attached to the grid so it scales in and out along with it. Has no mesh while nothing is carried.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> CarriedItemMesh;

	// A large cell over each hand, for what that hand holds. Attached to the grid so they scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> HandSlots;

	// Attached to the grid so it scales in and out along with it. Has no mesh while the hand is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> LeftHandItemMesh;

	// Attached to the grid so it scales in and out along with it. Has no mesh while the hand is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> RightHandItemMesh;

	// A cell above the grid for each quick slot. Attached to the grid so they scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> QuickSlots;

	// Attached to the grid so it scales in and out along with it. Has no mesh while the slot is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> LeftQuickSlotItemMesh;

	// Attached to the grid so it scales in and out along with it. Has no mesh while the slot is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> RightQuickSlotItemMesh;

	// A wide cell below the grid that drops the backpack when selected. Attached to the grid so it scales in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> DropBackpackButton;

	// Fills the screen just behind the grid and everything on it, shading the arms, the pack and the world beyond them
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> BackgroundDarkener;

	// A copy of the character's mesh with its arms posed in the display's frame and the rest of its body shrunk away, since it's all one mesh
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UPoseableMeshComponent> Arms;

	// Follows the mouse over the grid, shining on the items and the pack. The grid's own cells are unlit, so it doesn't brighten them.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UPointLightComponent> CursorLight;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.1", Units = cm))
	float CellSize = 3.0f;

	// From the camera to the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "1.0", Units = cm))
	float GridDistance = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid")
	TObjectPtr<UMaterialInterface> GridCellMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid")
	TObjectPtr<UMaterialInterface> ItemBorderMaterial;

	// How wide the border around an item is. It's drawn inside the cells the item covers.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = cm))
	float ItemBorderThickness = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = s))
	float GridScaleTime = 0.08f;

	// How much of the cells it covers an item fills, leaving a gap so it doesn't touch the lines
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ItemFillFraction = 0.9f;

	// The gap between a carried item and the tallest item in the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = cm))
	float CarriedItemClearance = 1.0f;

	// How many degrees a carried item leans for each cm per second it's dragged at on the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.0"))
	float CarriedItemSwayPerSpeed = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = deg))
	float CarriedItemMaxSway = 18.0f;

	// How hard a carried item is pulled towards the lean its speed calls for. Higher swings faster.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
	float CarriedItemSwayStiffness = 150.0f;

	// How quickly a carried item's swinging dies down. Lower swings back and forth more times when it stops.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float CarriedItemSwayDamping = 9.0f;

	// How long a carried item takes to turn when it's rotated
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.01", Units = s))
	float CarriedItemTurnTime = 0.15f;

	// How far the items in the hands and quick slots turn each way as they sway
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Slot Items", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = deg))
	float SlotItemSwayAngle = 3.0f;

	// How long the items in the hands and quick slots take to sway there and back
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Slot Items", meta = (ClampMin = "0.1", Units = s))
	float SlotItemSwayPeriod = 3.5f;

	// Unlit and translucent, with its opacity set through a scalar parameter named Darkness
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Background")
	TObjectPtr<UMaterialInterface> BackgroundDarkenMaterial;

	// How much of what's behind the grid is blocked out, once the grid has scaled in
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Background", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackgroundDarkness = 0.35f;

	// How far in front of the grid the light under the mouse sits
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.0", Units = cm))
	float CursorLightHeight = 8.0f;

	// How far the light reaches, as it's rendered: the meshes it shines on are drawn at FirstPersonScale of their size
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.1", Units = cm))
	float CursorLightRadius = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.0"))
	float CursorLightIntensity = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light")
	FLinearColor CursorLightColor = FLinearColor(1.0f, 0.85f, 0.65f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "0.1", Units = cm))
	float HandSlotSize = 12.0f;

	// Between the side of the grid and a hand's cell. The hands follow the cells, so they stay clear of the grid whatever its size.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "0.0", Units = cm))
	float HandSlotGap = 3.0f;

	// How far the centre of a hand's cell is above the centre of the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (Units = cm))
	float HandSlotHeight = -2.0f;

	// From the camera to the palms, which sit behind the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "1.0", Units = cm))
	float HandDistance = 65.0f;

	// From the wrist bone to the part of the hand centred in its cell
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "0.0", Units = cm))
	float WristToPalm = 10.0f;

	// The directions below pose the right hand, in the display's frame: X away from the camera, Y to the right, Z up. The left hand mirrors it.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector FingerDirection = FVector(0.15, -0.2, 1.0);

	// The way the palm faces, which only needs to be roughly across the fingers
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector PalmDirection = FVector(-1.0, -0.25, 0.0);

	// From the elbow to the wrist
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector ForearmDirection = FVector(0.4, -0.2, 0.9);

	// From the shoulder to the elbow. The upper arm is kept so that a narrow screen, which shows more below the elbow, doesn't show the arm's cut off end.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector UpperArmDirection = FVector(0.55, -0.3, 0.75);

	// Small enough that the cells stay on a 16:9 screen above a grid of the default height
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Quick Slots", meta = (ClampMin = "0.1", Units = cm))
	float QuickSlotSize = 7.5f;

	// Between the top of the grid and the quick slots' cells, and between the two cells
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Quick Slots", meta = (ClampMin = "0.0", Units = cm))
	float QuickSlotGap = 1.5f;

	// Wide enough for its label
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Drop Backpack Button", meta = (ClampMin = "0.1", Units = cm))
	float DropBackpackButtonWidth = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Drop Backpack Button", meta = (ClampMin = "0.1", Units = cm))
	float DropBackpackButtonHeight = 2.5f;

	// Between the bottom of the grid and the button
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Drop Backpack Button", meta = (ClampMin = "0.0", Units = cm))
	float DropBackpackButtonGap = 1.0f;

	// From the camera to the centre of the pack
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack", meta = (ClampMin = "1.0", Units = cm))
	float PackDistance = 70.0f;

	// The pack is scaled so its longest side is this long, whatever size the mesh is
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack", meta = (ClampMin = "1.0", Units = cm))
	float PackDisplaySize = 55.0f;

	// How far the camera looks down
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|View", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = deg))
	float ViewPitchDown = 20.0f;

	// Added to the head bone's height, which sits low in the head
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|View", meta = (Units = cm))
	float ViewHeightOffset = 10.0f;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ItemMeshes;

	// Replaced by the size of the backpack's grid when it's shown
	int32 GridWidth = 8;
	int32 GridHeight = 8;

	// What ShowInventory last showed, used to find the cells a hovered item covers
	TWeakObjectPtr<const UFSInventoryComponent> ShownInventory;

	FIntPoint HoveredCell = FIntPoint(INDEX_NONE, INDEX_NONE);
	int32 HoveredHandSlot = INDEX_NONE;

	// Which of the cell custom data values the hovered hand slot has set
	int32 HoveredHandSlotData = CellHoveredData;

	int32 HoveredQuickSlot = INDEX_NONE;

	// Which of the cell custom data values the hovered quick slot has set
	int32 HoveredQuickSlotData = CellHoveredData;

	bool bDropBackpackButtonHovered = false;

	bool bHasBackpack = true;

	TArray<FIntPoint> HighlightedCells;

	// Which of the cell custom data values HighlightedCells have set
	int32 HighlightedCellData = CellHoveredData;

	// How far in front of the grid a carried item sits, which ShowInventory finds from the items in the grid
	double CarriedItemLift = 1.0;

	float GridScale = 0.0f;
	float GridScaleTarget = 0.0f;

	// Where the mouse last pointed on the plane of the grid, and where it was when the sway last looked
	FVector2D CursorGridPoint = FVector2D::ZeroVector;
	FVector2D SwayCursorGridPoint = FVector2D::ZeroVector;

	FVector2D CarriedItemSway = FVector2D::ZeroVector;
	FVector2D CarriedItemSwayVelocity = FVector2D::ZeroVector;

	// The carried item ShowInventory last showed, used to tell when it's been rotated
	TWeakObjectPtr<const UFSItemDefinition> ShownCarriedDefinition;
	int32 ShownCarriedId = INDEX_NONE;
	uint8 ShownCarriedRotation = 0;

	// How far the carried item's mesh is from its proper place as a turn starts: an angle about the view and a move on the grid. It closes as the time runs out.
	float CarriedItemTurnTimeLeft = 0.0f;
	double CarriedItemTurnAngle = 0.0;
	FVector CarriedItemTurnShift = FVector::ZeroVector;

	// Where the items in the hands and quick slots sit, which they sway about
	FTransform HandItemRests[HandSlotCount];
	FTransform QuickSlotItemRests[QuickSlotCount];
	double SlotItemSwayTime = 0.0;

	// Made from BackgroundDarkenMaterial, so the rig can fade it
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackgroundDarkenInstance;
};
