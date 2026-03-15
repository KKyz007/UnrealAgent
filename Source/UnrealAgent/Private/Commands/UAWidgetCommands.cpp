// Copyright KuoYu. All Rights Reserved.

#include "Commands/UAWidgetCommands.h"
#include "UnrealAgent.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/GridPanel.h"
#include "Components/WrapBox.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/Spacer.h"
#include "Components/RichTextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogUAWidget, Log, All);

// ==================== Schema Helpers ====================

namespace UAWidgetHelper
{
	static TSharedPtr<FJsonObject> MakeProp(const FString& Type, const FString& Desc)
	{
		auto P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("type"), Type);
		P->SetStringField(TEXT("description"), Desc);
		return P;
	}

	static TSharedPtr<FJsonObject> MakeInputSchema(
		TArray<TPair<FString, TSharedPtr<FJsonObject>>> Props,
		TArray<FString> RequiredFields = {})
	{
		auto Schema = MakeShared<FJsonObject>();
		Schema->SetStringField(TEXT("type"), TEXT("object"));
		auto Properties = MakeShared<FJsonObject>();
		for (auto& P : Props) { Properties->SetObjectField(P.Key, P.Value); }
		Schema->SetObjectField(TEXT("properties"), Properties);
		if (RequiredFields.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Req;
			for (auto& R : RequiredFields) { Req.Add(MakeShared<FJsonValueString>(R)); }
			Schema->SetArrayField(TEXT("required"), Req);
		}
		return Schema;
	}
}

// ==================== Widget class mapping ====================

UClass* UAWidgetCommands::ResolveWidgetClass(const FString& WidgetType)
{
	static TMap<FString, UClass*> ClassMap;
	if (ClassMap.Num() == 0)
	{
		ClassMap.Add(TEXT("CanvasPanel"),      UCanvasPanel::StaticClass());
		ClassMap.Add(TEXT("VerticalBox"),      UVerticalBox::StaticClass());
		ClassMap.Add(TEXT("HorizontalBox"),    UHorizontalBox::StaticClass());
		ClassMap.Add(TEXT("Overlay"),          UOverlay::StaticClass());
		ClassMap.Add(TEXT("SizeBox"),          USizeBox::StaticClass());
		ClassMap.Add(TEXT("ScaleBox"),         UScaleBox::StaticClass());
		ClassMap.Add(TEXT("ScrollBox"),        UScrollBox::StaticClass());
		ClassMap.Add(TEXT("GridPanel"),        UGridPanel::StaticClass());
		ClassMap.Add(TEXT("WrapBox"),          UWrapBox::StaticClass());
		ClassMap.Add(TEXT("Border"),           UBorder::StaticClass());
		ClassMap.Add(TEXT("Button"),           UButton::StaticClass());
		ClassMap.Add(TEXT("TextBlock"),        UTextBlock::StaticClass());
		ClassMap.Add(TEXT("Image"),            UImage::StaticClass());
		ClassMap.Add(TEXT("EditableTextBox"),  UEditableTextBox::StaticClass());
		ClassMap.Add(TEXT("CheckBox"),         UCheckBox::StaticClass());
		ClassMap.Add(TEXT("ProgressBar"),      UProgressBar::StaticClass());
		ClassMap.Add(TEXT("Slider"),           USlider::StaticClass());
		ClassMap.Add(TEXT("ComboBoxString"),   UComboBoxString::StaticClass());
		ClassMap.Add(TEXT("Spacer"),           USpacer::StaticClass());
		ClassMap.Add(TEXT("RichTextBlock"),    URichTextBlock::StaticClass());
	}

	if (UClass** Found = ClassMap.Find(WidgetType))
	{
		return *Found;
	}

	UClass* FoundClass = FindFirstObject<UClass>(*WidgetType, EFindFirstObjectOptions::NativeFirst);
	if (FoundClass && FoundClass->IsChildOf(UWidget::StaticClass()))
	{
		return FoundClass;
	}

	FString PrefixedName = FString(TEXT("U")) + WidgetType;
	FoundClass = FindFirstObject<UClass>(*PrefixedName, EFindFirstObjectOptions::NativeFirst);
	if (FoundClass && FoundClass->IsChildOf(UWidget::StaticClass()))
	{
		return FoundClass;
	}

	return nullptr;
}

// ==================== GetSupportedMethods ====================

TArray<FString> UAWidgetCommands::GetSupportedMethods() const
{
	return {
		TEXT("get_widget_tree"),
		TEXT("add_widget"),
		TEXT("remove_widget"),
		TEXT("set_widget_property"),
		TEXT("move_widget"),
	};
}

// ==================== GetToolSchema ====================

TSharedPtr<FJsonObject> UAWidgetCommands::GetToolSchema(const FString& MethodName) const
{
	using namespace UAWidgetHelper;

	if (MethodName == TEXT("get_widget_tree"))
	{
		return MakeToolSchema(TEXT("get_widget_tree"),
			TEXT("获取 Widget Blueprint 的 UI 组件层级树。返回完整的 Widget 树结构，包括每个组件的类型、名称、Slot 属性。"),
			MakeInputSchema({
				{TEXT("asset_path"), MakeProp(TEXT("string"), TEXT("Widget Blueprint 资产路径，如 /Game/UI/WBP_MainMenu"))},
			}, {TEXT("asset_path")}));
	}

	if (MethodName == TEXT("add_widget"))
	{
		return MakeToolSchema(TEXT("add_widget"),
			TEXT("向 Widget Blueprint 添加 UI 组件。"
				"支持的 widget_type: CanvasPanel, VerticalBox, HorizontalBox, Overlay, "
				"SizeBox, ScaleBox, ScrollBox, GridPanel, WrapBox, Border, "
				"Button, TextBlock, Image, EditableTextBox, CheckBox, ProgressBar, "
				"Slider, ComboBoxString, Spacer, RichTextBlock。"
				"也可传任意 UWidget 子类名。"),
			MakeInputSchema({
				{TEXT("asset_path"), MakeProp(TEXT("string"), TEXT("Widget Blueprint 资产路径"))},
				{TEXT("widget_type"), MakeProp(TEXT("string"), TEXT("组件类型，如 Button, TextBlock, CanvasPanel"))},
				{TEXT("widget_name"), MakeProp(TEXT("string"), TEXT("组件名称，留空自动生成"))},
				{TEXT("parent_name"), MakeProp(TEXT("string"), TEXT("父组件名称。留空添加到根。父组件必须是 PanelWidget。"))},
			}, {TEXT("asset_path"), TEXT("widget_type")}));
	}

	if (MethodName == TEXT("remove_widget"))
	{
		return MakeToolSchema(TEXT("remove_widget"),
			TEXT("从 Widget Blueprint 中移除指定组件及其所有子组件。"),
			MakeInputSchema({
				{TEXT("asset_path"), MakeProp(TEXT("string"), TEXT("Widget Blueprint 资产路径"))},
				{TEXT("widget_name"), MakeProp(TEXT("string"), TEXT("要移除的组件名称"))},
			}, {TEXT("asset_path"), TEXT("widget_name")}));
	}

	if (MethodName == TEXT("set_widget_property"))
	{
		return MakeToolSchema(TEXT("set_widget_property"),
			TEXT("设置 Widget 组件的属性。"
				"常用属性: Text(文本), ToolTipText(提示), Visibility(可见性), "
				"RenderOpacity(不透明度), IsEnabled(是否启用), "
				"ColorAndOpacity(颜色), MinDesiredWidth/Height(最小尺寸)。"
				"对于 Slot 属性（锚点/位置/尺寸），使用 slot_ 前缀: "
				"slot_offset_left, slot_offset_top, slot_offset_right, slot_offset_bottom, "
				"slot_anchor_min_x, slot_anchor_min_y, slot_anchor_max_x, slot_anchor_max_y, "
				"slot_alignment_x, slot_alignment_y, slot_size_x, slot_size_y, slot_auto_size。"
				"对于 HAlign/VAlign: slot_h_align(Left/Center/Right/Fill), slot_v_align(Top/Center/Bottom/Fill)。"),
			MakeInputSchema({
				{TEXT("asset_path"), MakeProp(TEXT("string"), TEXT("Widget Blueprint 资产路径"))},
				{TEXT("widget_name"), MakeProp(TEXT("string"), TEXT("组件名称"))},
				{TEXT("property_name"), MakeProp(TEXT("string"), TEXT("属性名"))},
				{TEXT("property_value"), MakeProp(TEXT("string"), TEXT("属性值（字符串形式）"))},
			}, {TEXT("asset_path"), TEXT("widget_name"), TEXT("property_name"), TEXT("property_value")}));
	}

	if (MethodName == TEXT("move_widget"))
	{
		return MakeToolSchema(TEXT("move_widget"),
			TEXT("将组件移动到另一个父组件下。新父组件必须是 PanelWidget（可容纳子组件）。"),
			MakeInputSchema({
				{TEXT("asset_path"), MakeProp(TEXT("string"), TEXT("Widget Blueprint 资产路径"))},
				{TEXT("widget_name"), MakeProp(TEXT("string"), TEXT("要移动的组件名称"))},
				{TEXT("new_parent_name"), MakeProp(TEXT("string"), TEXT("新父组件名称。留空移到根。"))},
			}, {TEXT("asset_path"), TEXT("widget_name")}));
	}

	return nullptr;
}

// ==================== Execute dispatcher ====================

bool UAWidgetCommands::Execute(
	const FString& MethodName,
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	if (MethodName == TEXT("get_widget_tree"))      return ExecuteGetWidgetTree(Params, OutResult, OutError);
	if (MethodName == TEXT("add_widget"))            return ExecuteAddWidget(Params, OutResult, OutError);
	if (MethodName == TEXT("remove_widget"))         return ExecuteRemoveWidget(Params, OutResult, OutError);
	if (MethodName == TEXT("set_widget_property"))   return ExecuteSetWidgetProperty(Params, OutResult, OutError);
	if (MethodName == TEXT("move_widget"))            return ExecuteMoveWidget(Params, OutResult, OutError);

	OutError = FString::Printf(TEXT("Unknown method: %s"), *MethodName);
	return false;
}

// ==================== Helper: Load Widget Blueprint ====================

UWidgetBlueprint* UAWidgetCommands::LoadWidgetBlueprint(const FString& AssetPath, FString& OutError)
{
	UObject* Asset = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *AssetPath);
	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Asset);
	if (!WidgetBP)
	{
		OutError = FString::Printf(TEXT("Cannot load Widget Blueprint: %s"), *AssetPath);
	}
	return WidgetBP;
}

// ==================== Helper: Find widget by name ====================

UWidget* UAWidgetCommands::FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FString& OutError)
{
	if (!WidgetBP->WidgetTree)
	{
		OutError = TEXT("Widget Blueprint has no WidgetTree");
		return nullptr;
	}

	UWidget* Found = nullptr;
	WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Widget && Widget->GetName() == WidgetName)
		{
			Found = Widget;
		}
	});

	if (!Found)
	{
		OutError = FString::Printf(TEXT("Widget not found: %s"), *WidgetName);
	}
	return Found;
}

// ==================== Helper: Widget to JSON ====================

TSharedPtr<FJsonObject> UAWidgetCommands::WidgetToJson(UWidget* Widget)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Widget->GetName());
	Obj->SetStringField(TEXT("type"), Widget->GetClass()->GetName());
	Obj->SetStringField(TEXT("visibility"), UEnum::GetValueAsString(Widget->GetVisibility()));
	Obj->SetBoolField(TEXT("is_panel"), Widget->IsA<UPanelWidget>());

	if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
	{
		Obj->SetStringField(TEXT("text"), TextBlock->GetText().ToString());
	}

	if (UPanelSlot* Slot = Widget->Slot)
	{
		auto SlotObj = MakeShared<FJsonObject>();
		SlotObj->SetStringField(TEXT("slot_class"), Slot->GetClass()->GetName());

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			auto Offsets = CanvasSlot->GetOffsets();
			SlotObj->SetNumberField(TEXT("offset_left"), Offsets.Left);
			SlotObj->SetNumberField(TEXT("offset_top"), Offsets.Top);
			SlotObj->SetNumberField(TEXT("offset_right"), Offsets.Right);
			SlotObj->SetNumberField(TEXT("offset_bottom"), Offsets.Bottom);

			auto AnchorsMin = CanvasSlot->GetAnchors().Minimum;
			auto AnchorsMax = CanvasSlot->GetAnchors().Maximum;
			SlotObj->SetNumberField(TEXT("anchor_min_x"), AnchorsMin.X);
			SlotObj->SetNumberField(TEXT("anchor_min_y"), AnchorsMin.Y);
			SlotObj->SetNumberField(TEXT("anchor_max_x"), AnchorsMax.X);
			SlotObj->SetNumberField(TEXT("anchor_max_y"), AnchorsMax.Y);

			auto Alignment = CanvasSlot->GetAlignment();
			SlotObj->SetNumberField(TEXT("alignment_x"), Alignment.X);
			SlotObj->SetNumberField(TEXT("alignment_y"), Alignment.Y);

			auto Size = CanvasSlot->GetSize();
			SlotObj->SetNumberField(TEXT("size_x"), Size.X);
			SlotObj->SetNumberField(TEXT("size_y"), Size.Y);

			SlotObj->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
		}

		Obj->SetObjectField(TEXT("slot"), SlotObj);
	}

	return Obj;
}

// ==================== Helper: Build Widget Tree JSON (recursive) ====================

TSharedPtr<FJsonObject> UAWidgetCommands::BuildWidgetTreeJson(UWidget* Widget)
{
	auto Node = WidgetToJson(Widget);

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		TArray<TSharedPtr<FJsonValue>> ChildrenArr;
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			UWidget* Child = Panel->GetChildAt(i);
			if (Child)
			{
				ChildrenArr.Add(MakeShared<FJsonValueObject>(BuildWidgetTreeJson(Child)));
			}
		}
		Node->SetArrayField(TEXT("children"), ChildrenArr);
	}

	return Node;
}

// ==================== get_widget_tree ====================

bool UAWidgetCommands::ExecuteGetWidgetTree(
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { OutError = TEXT("'asset_path' required"); return false; }

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(AssetPath, OutError);
	if (!WidgetBP) return false;

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetStringField(TEXT("asset_path"), AssetPath);
	OutResult->SetStringField(TEXT("blueprint_name"), WidgetBP->GetName());

	if (WidgetBP->WidgetTree && WidgetBP->WidgetTree->RootWidget)
	{
		OutResult->SetObjectField(TEXT("root"), BuildWidgetTreeJson(WidgetBP->WidgetTree->RootWidget));
	}
	else
	{
		OutResult->SetField(TEXT("root"), MakeShared<FJsonValueNull>());
		OutResult->SetStringField(TEXT("message"), TEXT("Widget tree is empty. Use add_widget to add a root panel (e.g. CanvasPanel)."));
	}

	TArray<TSharedPtr<FJsonValue>> AllWidgets;
	if (WidgetBP->WidgetTree)
	{
		WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (Widget)
			{
				auto Info = MakeShared<FJsonObject>();
				Info->SetStringField(TEXT("name"), Widget->GetName());
				Info->SetStringField(TEXT("type"), Widget->GetClass()->GetName());
				AllWidgets.Add(MakeShared<FJsonValueObject>(Info));
			}
		});
	}
	OutResult->SetArrayField(TEXT("all_widgets"), AllWidgets);
	OutResult->SetNumberField(TEXT("widget_count"), AllWidgets.Num());

	return true;
}

// ==================== add_widget ====================

bool UAWidgetCommands::ExecuteAddWidget(
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	FString AssetPath, WidgetType, WidgetName, ParentName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { OutError = TEXT("'asset_path' required"); return false; }
	if (!Params->TryGetStringField(TEXT("widget_type"), WidgetType)) { OutError = TEXT("'widget_type' required"); return false; }
	Params->TryGetStringField(TEXT("widget_name"), WidgetName);
	Params->TryGetStringField(TEXT("parent_name"), ParentName);

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(AssetPath, OutError);
	if (!WidgetBP) return false;

	UClass* WidgetClass = ResolveWidgetClass(WidgetType);
	if (!WidgetClass)
	{
		OutError = FString::Printf(TEXT("Unknown widget type: %s"), *WidgetType);
		return false;
	}

	UWidgetTree* Tree = WidgetBP->WidgetTree;
	if (!Tree)
	{
		OutError = TEXT("Widget Blueprint has no WidgetTree");
		return false;
	}

	// Generate name if empty
	if (WidgetName.IsEmpty())
	{
		WidgetName = WidgetType + TEXT("_0");
		int32 Counter = 1;
		while (FindWidgetByName(WidgetBP, WidgetName, OutError) != nullptr)
		{
			OutError.Empty();
			WidgetName = FString::Printf(TEXT("%s_%d"), *WidgetType, Counter++);
		}
		OutError.Empty();
	}

	// Determine parent
	UPanelWidget* ParentPanel = nullptr;
	if (!ParentName.IsEmpty())
	{
		UWidget* ParentWidget = FindWidgetByName(WidgetBP, ParentName, OutError);
		if (!ParentWidget) return false;

		ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			OutError = FString::Printf(TEXT("Parent '%s' is not a PanelWidget, cannot contain children"), *ParentName);
			return false;
		}
	}

	WidgetBP->Modify();

	UWidget* NewWidget = Tree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
	if (!NewWidget)
	{
		OutError = FString::Printf(TEXT("Failed to construct widget of type %s"), *WidgetType);
		return false;
	}

	if (ParentPanel)
	{
		UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
		if (!Slot)
		{
			OutError = FString::Printf(TEXT("Failed to add widget to parent '%s'"), *ParentName);
			return false;
		}
	}
	else
	{
		if (Tree->RootWidget != nullptr)
		{
			// Root already exists; if it's a Panel, add as child; otherwise error
			UPanelWidget* RootPanel = Cast<UPanelWidget>(Tree->RootWidget);
			if (RootPanel)
			{
				RootPanel->AddChild(NewWidget);
			}
			else
			{
				OutError = TEXT("Root widget exists and is not a PanelWidget. Specify parent_name or replace root.");
				Tree->RemoveWidget(NewWidget);
				return false;
			}
		}
		else
		{
			Tree->RootWidget = NewWidget;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetBoolField(TEXT("success"), true);
	OutResult->SetStringField(TEXT("widget_name"), NewWidget->GetName());
	OutResult->SetStringField(TEXT("widget_type"), WidgetClass->GetName());
	OutResult->SetStringField(TEXT("parent"), ParentPanel ? ParentPanel->GetName() : TEXT("(root)"));

	UE_LOG(LogUAWidget, Log, TEXT("add_widget: %s (%s) -> parent: %s"), *NewWidget->GetName(), *WidgetType, ParentPanel ? *ParentPanel->GetName() : TEXT("root"));
	return true;
}

// ==================== remove_widget ====================

bool UAWidgetCommands::ExecuteRemoveWidget(
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	FString AssetPath, WidgetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { OutError = TEXT("'asset_path' required"); return false; }
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName)) { OutError = TEXT("'widget_name' required"); return false; }

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(AssetPath, OutError);
	if (!WidgetBP) return false;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, OutError);
	if (!Widget) return false;

	WidgetBP->Modify();

	UWidgetTree* Tree = WidgetBP->WidgetTree;
	bool bIsRoot = (Tree->RootWidget == Widget);

	if (bIsRoot)
	{
		Tree->RootWidget = nullptr;
	}

	Tree->RemoveWidget(Widget);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetBoolField(TEXT("success"), true);
	OutResult->SetStringField(TEXT("removed_widget"), WidgetName);
	OutResult->SetBoolField(TEXT("was_root"), bIsRoot);

	UE_LOG(LogUAWidget, Log, TEXT("remove_widget: %s (was_root=%d)"), *WidgetName, bIsRoot);
	return true;
}

// ==================== set_widget_property ====================

static EHorizontalAlignment ParseHAlign(const FString& Value)
{
	if (Value == TEXT("Left"))   return EHorizontalAlignment::HAlign_Left;
	if (Value == TEXT("Center")) return EHorizontalAlignment::HAlign_Center;
	if (Value == TEXT("Right"))  return EHorizontalAlignment::HAlign_Right;
	if (Value == TEXT("Fill"))   return EHorizontalAlignment::HAlign_Fill;
	return EHorizontalAlignment::HAlign_Fill;
}

static EVerticalAlignment ParseVAlign(const FString& Value)
{
	if (Value == TEXT("Top"))    return EVerticalAlignment::VAlign_Top;
	if (Value == TEXT("Center")) return EVerticalAlignment::VAlign_Center;
	if (Value == TEXT("Bottom")) return EVerticalAlignment::VAlign_Bottom;
	if (Value == TEXT("Fill"))   return EVerticalAlignment::VAlign_Fill;
	return EVerticalAlignment::VAlign_Fill;
}

bool UAWidgetCommands::ExecuteSetWidgetProperty(
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	FString AssetPath, WidgetName, PropName, PropValue;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { OutError = TEXT("'asset_path' required"); return false; }
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName)) { OutError = TEXT("'widget_name' required"); return false; }
	if (!Params->TryGetStringField(TEXT("property_name"), PropName)) { OutError = TEXT("'property_name' required"); return false; }
	if (!Params->TryGetStringField(TEXT("property_value"), PropValue)) { OutError = TEXT("'property_value' required"); return false; }

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(AssetPath, OutError);
	if (!WidgetBP) return false;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, OutError);
	if (!Widget) return false;

	WidgetBP->Modify();
	Widget->Modify();

	bool bHandled = false;

	// -- Slot properties (slot_ prefix) --
	if (PropName.StartsWith(TEXT("slot_")))
	{
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
		if (!CanvasSlot)
		{
			OutError = FString::Printf(TEXT("Widget '%s' does not have a CanvasPanelSlot (parent might not be a CanvasPanel)"), *WidgetName);
			return false;
		}

		double NumValue = FCString::Atod(*PropValue);

		if (PropName == TEXT("slot_offset_left"))        { auto O = CanvasSlot->GetOffsets(); O.Left = NumValue; CanvasSlot->SetOffsets(O); bHandled = true; }
		else if (PropName == TEXT("slot_offset_top"))     { auto O = CanvasSlot->GetOffsets(); O.Top = NumValue; CanvasSlot->SetOffsets(O); bHandled = true; }
		else if (PropName == TEXT("slot_offset_right"))   { auto O = CanvasSlot->GetOffsets(); O.Right = NumValue; CanvasSlot->SetOffsets(O); bHandled = true; }
		else if (PropName == TEXT("slot_offset_bottom"))  { auto O = CanvasSlot->GetOffsets(); O.Bottom = NumValue; CanvasSlot->SetOffsets(O); bHandled = true; }
		else if (PropName == TEXT("slot_size_x"))         { auto S = CanvasSlot->GetSize(); S.X = NumValue; CanvasSlot->SetSize(S); bHandled = true; }
		else if (PropName == TEXT("slot_size_y"))         { auto S = CanvasSlot->GetSize(); S.Y = NumValue; CanvasSlot->SetSize(S); bHandled = true; }
		else if (PropName == TEXT("slot_anchor_min_x"))   { auto A = CanvasSlot->GetAnchors(); A.Minimum.X = NumValue; CanvasSlot->SetAnchors(A); bHandled = true; }
		else if (PropName == TEXT("slot_anchor_min_y"))   { auto A = CanvasSlot->GetAnchors(); A.Minimum.Y = NumValue; CanvasSlot->SetAnchors(A); bHandled = true; }
		else if (PropName == TEXT("slot_anchor_max_x"))   { auto A = CanvasSlot->GetAnchors(); A.Maximum.X = NumValue; CanvasSlot->SetAnchors(A); bHandled = true; }
		else if (PropName == TEXT("slot_anchor_max_y"))   { auto A = CanvasSlot->GetAnchors(); A.Maximum.Y = NumValue; CanvasSlot->SetAnchors(A); bHandled = true; }
		else if (PropName == TEXT("slot_alignment_x"))    { auto Al = CanvasSlot->GetAlignment(); Al.X = NumValue; CanvasSlot->SetAlignment(Al); bHandled = true; }
		else if (PropName == TEXT("slot_alignment_y"))    { auto Al = CanvasSlot->GetAlignment(); Al.Y = NumValue; CanvasSlot->SetAlignment(Al); bHandled = true; }
		else if (PropName == TEXT("slot_auto_size"))      { CanvasSlot->SetAutoSize(PropValue.ToBool()); bHandled = true; }
		else if (PropName == TEXT("slot_h_align"))
		{
			// HAlign only applies to certain slot types (VerticalBoxSlot, OverlaySlot, etc.)
			if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))      { VSlot->SetHorizontalAlignment(ParseHAlign(PropValue)); bHandled = true; }
			else if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(Widget->Slot))          { OSlot->SetHorizontalAlignment(ParseHAlign(PropValue)); bHandled = true; }
			else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot)) { HSlot->SetHorizontalAlignment(ParseHAlign(PropValue)); bHandled = true; }
		}
		else if (PropName == TEXT("slot_v_align"))
		{
			if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))      { VSlot->SetVerticalAlignment(ParseVAlign(PropValue)); bHandled = true; }
			else if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(Widget->Slot))          { OSlot->SetVerticalAlignment(ParseVAlign(PropValue)); bHandled = true; }
			else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot)) { HSlot->SetVerticalAlignment(ParseVAlign(PropValue)); bHandled = true; }
		}
	}

	// -- Common widget properties --
	if (!bHandled && PropName == TEXT("Text"))
	{
		if (UTextBlock* TB = Cast<UTextBlock>(Widget))    { TB->SetText(FText::FromString(PropValue)); bHandled = true; }
		else if (UButton* Btn = Cast<UButton>(Widget))     { /* Button has no text directly */ }
	}
	if (!bHandled && PropName == TEXT("Visibility"))
	{
		ESlateVisibility V = ESlateVisibility::Visible;
		if (PropValue == TEXT("Hidden"))           V = ESlateVisibility::Hidden;
		else if (PropValue == TEXT("Collapsed"))   V = ESlateVisibility::Collapsed;
		else if (PropValue == TEXT("HitTestInvisible")) V = ESlateVisibility::HitTestInvisible;
		else if (PropValue == TEXT("SelfHitTestInvisible")) V = ESlateVisibility::SelfHitTestInvisible;
		Widget->SetVisibility(V);
		bHandled = true;
	}
	if (!bHandled && PropName == TEXT("IsEnabled"))
	{
		Widget->SetIsEnabled(PropValue.ToBool());
		bHandled = true;
	}
	if (!bHandled && PropName == TEXT("RenderOpacity"))
	{
		Widget->SetRenderOpacity(FCString::Atof(*PropValue));
		bHandled = true;
	}
	if (!bHandled && PropName == TEXT("ToolTipText"))
	{
		Widget->SetToolTipText(FText::FromString(PropValue));
		bHandled = true;
	}

	// -- Fallback: UE property reflection --
	if (!bHandled)
	{
		FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*PropName));
		if (Prop)
		{
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);
			if (Prop->ImportText_Direct(*PropValue, ValuePtr, Widget, PPF_None))
			{
				bHandled = true;
			}
			else
			{
				OutError = FString::Printf(TEXT("Failed to set property '%s' to '%s' (ImportText failed)"), *PropName, *PropValue);
				return false;
			}
		}
	}

	if (!bHandled)
	{
		OutError = FString::Printf(TEXT("Unknown property '%s' for widget '%s' (%s)"), *PropName, *WidgetName, *Widget->GetClass()->GetName());
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetBoolField(TEXT("success"), true);
	OutResult->SetStringField(TEXT("widget_name"), WidgetName);
	OutResult->SetStringField(TEXT("property_name"), PropName);
	OutResult->SetStringField(TEXT("property_value"), PropValue);

	UE_LOG(LogUAWidget, Log, TEXT("set_widget_property: %s.%s = %s"), *WidgetName, *PropName, *PropValue);
	return true;
}

// ==================== move_widget ====================

bool UAWidgetCommands::ExecuteMoveWidget(
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	FString AssetPath, WidgetName, NewParentName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { OutError = TEXT("'asset_path' required"); return false; }
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName)) { OutError = TEXT("'widget_name' required"); return false; }
	Params->TryGetStringField(TEXT("new_parent_name"), NewParentName);

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(AssetPath, OutError);
	if (!WidgetBP) return false;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, OutError);
	if (!Widget) return false;

	WidgetBP->Modify();
	UWidgetTree* Tree = WidgetBP->WidgetTree;

	// Remove from current parent
	if (Widget->Slot && Widget->Slot->Parent)
	{
		Widget->Slot->Parent->RemoveChild(Widget);
	}

	if (NewParentName.IsEmpty())
	{
		if (Tree->RootWidget && Tree->RootWidget != Widget)
		{
			UPanelWidget* RootPanel = Cast<UPanelWidget>(Tree->RootWidget);
			if (RootPanel)
			{
				RootPanel->AddChild(Widget);
			}
			else
			{
				OutError = TEXT("Root is not a PanelWidget, cannot add child. Specify a parent_name.");
				return false;
			}
		}
		else
		{
			Tree->RootWidget = Widget;
		}
	}
	else
	{
		UWidget* NewParent = FindWidgetByName(WidgetBP, NewParentName, OutError);
		if (!NewParent) return false;

		UPanelWidget* NewParentPanel = Cast<UPanelWidget>(NewParent);
		if (!NewParentPanel)
		{
			OutError = FString::Printf(TEXT("'%s' is not a PanelWidget, cannot contain children"), *NewParentName);
			return false;
		}
		NewParentPanel->AddChild(Widget);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetBoolField(TEXT("success"), true);
	OutResult->SetStringField(TEXT("widget_name"), WidgetName);
	OutResult->SetStringField(TEXT("new_parent"), NewParentName.IsEmpty() ? TEXT("(root)") : NewParentName);

	UE_LOG(LogUAWidget, Log, TEXT("move_widget: %s -> %s"), *WidgetName, NewParentName.IsEmpty() ? TEXT("root") : *NewParentName);
	return true;
}
