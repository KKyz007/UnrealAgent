// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "Commands/UACommandBase.h"

class UWidgetBlueprint;
class UWidget;
class UPanelWidget;

/**
 * Widget Blueprint (UMG) 编辑命令组。
 * 操作 Widget Blueprint 的组件树（Designer 面板），区别于蓝图节点图编辑。
 *
 * 支持的方法:
 *   查询:
 *     - get_widget_tree:        获取 Widget 层级树
 *   操作:
 *     - add_widget:             添加 UI 组件（Button, TextBlock, Image 等）
 *     - remove_widget:          移除 UI 组件
 *     - set_widget_property:    设置组件属性（文本、颜色、锚点、尺寸等）
 *     - move_widget:            移动组件到另一个父组件下
 */
class UAWidgetCommands : public UACommandBase
{
public:
	virtual TArray<FString> GetSupportedMethods() const override;
	virtual TSharedPtr<FJsonObject> GetToolSchema(const FString& MethodName) const override;
	virtual bool Execute(
		const FString& MethodName,
		const TSharedPtr<FJsonObject>& Params,
		TSharedPtr<FJsonObject>& OutResult,
		FString& OutError
	) override;

private:
	// ========== 查询 ==========
	bool ExecuteGetWidgetTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError);

	// ========== 操作 ==========
	bool ExecuteAddWidget(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError);
	bool ExecuteRemoveWidget(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError);
	bool ExecuteSetWidgetProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError);
	bool ExecuteMoveWidget(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError);

	// ========== 辅助方法 ==========
	UWidgetBlueprint* LoadWidgetBlueprint(const FString& AssetPath, FString& OutError);
	UWidget* FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FString& OutError);
	TSharedPtr<FJsonObject> WidgetToJson(UWidget* Widget);
	TSharedPtr<FJsonObject> BuildWidgetTreeJson(UWidget* Widget);
	UClass* ResolveWidgetClass(const FString& WidgetType);
};
