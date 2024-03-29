#pragma once

#include "Blueprint/UserWidget.h"

#include "GKWidgetDragVisual.h"
#include "GKWidgetDraggable.generated.h"

/*! Simple Draggable widget, set DragKey, DragVisualClass & implement IsDraggable()
 * 
 * \rst
 * .. warning::
 *
 *    If you use :member:`UPanelWidget::AddChild` on :class:`UGKWidgetDraggable` it will lose its previous parent.
 *    :cpp:class:`UGKWidgetDragVisual` will try to restore the parent of :cpp:class:`UGKWidgetDraggable` on drag failure.
 *    Nevertheless, failure to restore the parent will make the dragged widget disappear.
 *
 * .. note::
 * 
 *    In design time, it is possible to place :class:`UGKWidgetDraggable` outside of a :class:`UGKWidgetDropZone`.
 *    Nevertheless, this use is not well supported as on drag and drop failure the widget position & size will reset
 *    to something different from its design time position and size, making the user experience less than ideal.
 * 
 * \endrst
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class GAMEKIT_API UGKWidgetDraggable : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    //! Key press used to create Drag and Drop operation
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drag and Drop")
    FKey DragKey;

    //! Widget to use to visualize the Dragging operation
    //! Usually people use the same widget 
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drag and Drop")
    TSubclassOf<UGKWidgetDragVisual> DragVisualClass;

    //! Return true to enable Dragging the widget
    UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Drag and Drop")
    bool IsDraggable();

    // UFUNCTION(BlueprintImplementableEvent
    // UFUNCTION(BlueprintNativeEvent)

    bool IsDraggable_Native();

    //! Compute the drag offset and call NativeOnDragDetected
    FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    //! Create the Drag Widget visual & Create a new DragDrop operation
    void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
};
