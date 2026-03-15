"""Widget Blueprint (UMG) editing tools — manage UI component trees in Widget Blueprints."""

from ..server import mcp, connection


@mcp.tool()
async def get_widget_tree(asset_path: str) -> dict:
    """获取 Widget Blueprint 的 UI 组件层级树。

    返回完整的 Widget 树结构，包括每个组件的类型、名称、Slot 属性（位置/锚点/尺寸）。

    Args:
        asset_path: Widget Blueprint 资产路径，如 /Game/UI/WBP_MainMenu
    """
    return await connection.send_request("get_widget_tree", {"asset_path": asset_path})


@mcp.tool()
async def add_widget(
    asset_path: str,
    widget_type: str,
    widget_name: str = "",
    parent_name: str = "",
) -> dict:
    """向 Widget Blueprint 添加 UI 组件。

    常用 widget_type:
      布局: CanvasPanel, VerticalBox, HorizontalBox, Overlay, SizeBox, ScaleBox,
            ScrollBox, GridPanel, WrapBox, Border
      交互: Button, CheckBox, Slider, ComboBoxString, EditableTextBox
      显示: TextBlock, Image, ProgressBar, Spacer, RichTextBlock

    也支持任意 UWidget 子类全名。
    第一个添加的组件将成为根组件（通常应是 CanvasPanel 或 Overlay）。

    Args:
        asset_path: Widget Blueprint 资产路径
        widget_type: 组件类型名
        widget_name: 组件名称，留空自动生成
        parent_name: 父组件名称。留空则添加到根。父组件必须是 PanelWidget（能容纳子组件）。
    """
    params: dict = {"asset_path": asset_path, "widget_type": widget_type}
    if widget_name:
        params["widget_name"] = widget_name
    if parent_name:
        params["parent_name"] = parent_name
    return await connection.send_request("add_widget", params)


@mcp.tool()
async def remove_widget(asset_path: str, widget_name: str) -> dict:
    """从 Widget Blueprint 中移除指定组件及其所有子组件。

    Args:
        asset_path: Widget Blueprint 资产路径
        widget_name: 要移除的组件名称
    """
    return await connection.send_request("remove_widget", {
        "asset_path": asset_path,
        "widget_name": widget_name,
    })


@mcp.tool()
async def set_widget_property(
    asset_path: str,
    widget_name: str,
    property_name: str,
    property_value: str,
) -> dict:
    """设置 Widget 组件的属性。

    常用属性:
      Text - 文本内容 (TextBlock)
      Visibility - Visible/Hidden/Collapsed/HitTestInvisible/SelfHitTestInvisible
      IsEnabled - true/false
      RenderOpacity - 0.0~1.0
      ToolTipText - 提示文本

    Slot 属性（CanvasPanel 子组件的位置/锚点/尺寸）用 slot_ 前缀:
      slot_offset_left/top/right/bottom - 位置偏移
      slot_size_x/size_y - 尺寸
      slot_anchor_min_x/min_y/max_x/max_y - 锚点 (0.0~1.0)
      slot_alignment_x/alignment_y - 对齐 (0.0~1.0)
      slot_auto_size - true/false
      slot_h_align - Left/Center/Right/Fill (VerticalBox/Overlay 子组件)
      slot_v_align - Top/Center/Bottom/Fill (HorizontalBox/Overlay 子组件)

    对于未列出的属性，会尝试通过 UE 反射系统设置。

    Args:
        asset_path: Widget Blueprint 资产路径
        widget_name: 组件名称
        property_name: 属性名
        property_value: 属性值（字符串形式）
    """
    return await connection.send_request("set_widget_property", {
        "asset_path": asset_path,
        "widget_name": widget_name,
        "property_name": property_name,
        "property_value": property_value,
    })


@mcp.tool()
async def move_widget(
    asset_path: str,
    widget_name: str,
    new_parent_name: str = "",
) -> dict:
    """将组件移动到另一个父组件下。

    新父组件必须是 PanelWidget（能容纳子组件的类型，如 CanvasPanel, VerticalBox 等）。
    留空 new_parent_name 会尝试添加到根组件下。

    Args:
        asset_path: Widget Blueprint 资产路径
        widget_name: 要移动的组件名称
        new_parent_name: 新父组件名称，留空添加到根
    """
    params: dict = {"asset_path": asset_path, "widget_name": widget_name}
    if new_parent_name:
        params["new_parent_name"] = new_parent_name
    return await connection.send_request("move_widget", params)
