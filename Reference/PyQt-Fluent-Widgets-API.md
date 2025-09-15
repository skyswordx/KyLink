好的，这是一份根据您提供的 `qfluentwidgets` 源代码文件整理出的 API 文档。

### **qfluentwidgets API 文档**

#### **顶层包: `qfluentwidgets`**

- **`components`**: 包含各种 UI 组件。
- **`multimedia`**: 包含多媒体相关的组件。
- **`window`**: 包含窗口相关的组件。
- **`common`**: 包含一些通用工具和类。

---

### **`qfluentwidgets.components`**

这个包是 `qfluentwidgets` 库的核心，包含了构建用户界面的主要组件。

#### **`dialog_box`**

该模块提供多种对话框。

- `ColorDialog`: 颜色选择对话框。
- `Dialog`: 自定义内容的对话框。
- `FolderListDialog`: 文件夹列表对话框。
- `MessageBox`: 消息提示框。
- `MessageDialog`: 消息对话框。
- `MessageBoxBase`: 消息框基类。
- `MaskDialogBase`: 遮罩对话框基类。

#### **`layout`**

该模块提供自定义布局。

- `ExpandLayout`: 可展开的布局。
- `FlowLayout`: 流式布局。
- `VBoxLayout`: 垂直布局。

#### **`settings`**

该模块提供设置界面相关的组件。

- `SettingCard`: 设置卡片。
- `SwitchSettingCard`: 带开关的设置卡片。
- `RangeSettingCard`: 范围选择设置卡片。
- `PushSettingCard`: 按钮式设置卡片。
- `ColorSettingCard`: 颜色选择设置卡片。
- `HyperlinkCard`: 超链接卡片。
- `PrimaryPushSettingCard`: 主要按钮式设置卡片。
- `ColorPickerButton`: 颜色选择器按钮。
- `ComboBoxSettingCard`: 下拉框设置卡片。
- `ExpandSettingCard`: 可展开的设置卡片。
- `ExpandGroupSettingCard`: 可展开的分组设置卡片。
- `SimpleExpandGroupSettingCard`: 简单的可展开分组设置卡片。
- `FolderListSettingCard`: 文件夹列表设置卡片。
- `OptionsSettingCard`: 选项设置卡片。
- `CustomColorSettingCard`: 自定义颜色设置卡片。
- `SettingCardGroup`: 设置卡片组。

#### **`widgets`**

该模块提供各种通用的小组件。

- **按钮**:
    - `DropDownPushButton`
    - `DropDownToolButton`
    - `PrimaryPushButton`
    - `PushButton`
    - `RadioButton`
    - `HyperlinkButton`
    - `ToolButton`
    - `TransparentToolButton`
    - `ToggleButton`
    - `SplitWidgetBase`
    - `SplitPushButton`
    - `SplitToolButton`
    - `PrimaryToolButton`
    - `PrimarySplitPushButton`
    - `PrimarySplitToolButton`
    - `PrimaryDropDownPushButton`
    - `PrimaryDropDownToolButton`
    - `TogglePushButton`
    - `ToggleToolButton`
    - `TransparentPushButton`
    - `TransparentTogglePushButton`
    - `TransparentToggleToolButton`
    - `TransparentDropDownPushButton`
    - `TransparentDropDownToolButton`
    - `PillPushButton`
    - `PillToolButton`
- **卡片**:
    - `CardWidget`
    - `ElevatedCardWidget`
    - `SimpleCardWidget`
    - `HeaderCardWidget`
    - `CardGroupWidget`
    - `GroupHeaderCardWidget`
- **选择框**:
    - `CheckBox`
- **下拉框**:
    - `ComboBox`
    - `EditableComboBox`
    - `ModelComboBox`
    - `EditableModelComboBox`
- **命令栏**:
    - `CommandBar`
    - `CommandButton`
    - `CommandBarView`
- **翻转视图**:
    - `FlipView`
    - `HorizontalFlipView`
    - `VerticalFlipView`
    - `FlipImageDelegate`
- **文本输入**:
    - `LineEdit`
    - `TextEdit`
    - `PlainTextEdit`
    - `LineEditButton`
    - `SearchLineEdit`
    - `PasswordLineEdit`
    - `TextBrowser`
- **图标**:
    - `IconWidget`
- **标签**:
    - `PixmapLabel`
    - `CaptionLabel`
    - `StrongBodyLabel`
    - `BodyLabel`
    - `SubtitleLabel`
    - `TitleLabel`
    - `LargeTitleLabel`
    - `DisplayLabel`
    - `FluentLabelBase`
    - `ImageLabel`
    - `AvatarWidget`
    - `HyperlinkLabel`
- **列表视图**:
    - `ListWidget`
    - `ListView`
    - `ListItemDelegate`
- **菜单**:
    - `DWMMenu`
    - `LineEditMenu`
    - `RoundMenu`
    - `MenuAnimationManager`
    - `MenuAnimationType`
    - `IndicatorMenuItemDelegate`
    - `MenuItemDelegate`
    - `ShortcutMenuItemDelegate`
    - `CheckableMenu`
    - `MenuIndicatorType`
    - `SystemTrayMenu`
    - `CheckableSystemTrayMenu`
- **信息栏**:
    - `InfoBar`
    - `InfoBarIcon`
    - `InfoBarPosition`
    - `InfoBarManager`
- **信息徽章**:
    - `InfoBadge`
    - `InfoLevel`
    - `DotInfoBadge`
    - `IconInfoBadge`
    - `InfoBadgePosition`
    - `InfoBadgeManager`
- **滚动区域**:
    - `SingleDirectionScrollArea`
    - `SmoothMode`
    - `SmoothScrollArea`
    - `ScrollArea`
- **滑块**:
    - `Slider`
    - `HollowHandleStyle`
    - `ClickableSlider`
- **微调框**:
    - `SpinBox`
    - `DoubleSpinBox`
    - `DateEdit`
    - `DateTimeEdit`
    - `TimeEdit`
    - `CompactSpinBox`
    - `CompactDoubleSpinBox`
    - `CompactDateEdit`
    - `CompactDateTimeEdit`
    - `CompactTimeEdit`
- **堆叠小组件**:
    - `PopUpAniStackedWidget`
    - `OpacityAniStackedWidget`
- **状态提示**:
    - `StateToolTip`
- **开关按钮**:
    - `SwitchButton`
    - `IndicatorPosition`
- **表格视图**:
    - `TableView`
    - `TableWidget`
    - `TableItemDelegate`
- **工具提示**:
    - `ToolTip`
    - `ToolTipFilter`
    - `ToolTipPosition`
- **树状视图**:
    - `TreeWidget`
    - `TreeView`
    - `TreeItemDelegate`
- `CycleListWidget`: 循环列表组件。
- **进度条**:
    - `IndeterminateProgressBar`
    - `ProgressBar`
- **进度环**:
    - `ProgressRing`
    - `IndeterminateProgressRing`
- **滚动条**:
    - `ScrollBar`
    - `SmoothScrollBar`
    - `SmoothScrollDelegate`
    - `ScrollBarHandleDisplayMode`
- **教学提示**:
    - `TeachingTip`
    - `TeachingTipTailPosition`
    - `TeachingTipView`
    - `PopupTeachingTip`
- **弹出框**:
    - `FlyoutView`
    - `FlyoutViewBase`
    - `Flyout`
    - `FlyoutAnimationType`
    - `FlyoutAnimationManager`
- **标签页**:
    - `TabBar`
    - `TabItem`
    - `TabCloseButtonDisplayMode`
- **分页指示器**:
    - `PipsPager`
    - `VerticalPipsPager`
    - `HorizontalPipsPager`
    - `PipsScrollButtonDisplayMode`
- **分隔符**:
    - `HorizontalSeparator`
    - `VerticalSeparator`

#### **`navigation`**

该模块提供导航相关的组件。

- `NavigationWidget`: 导航组件。
- `NavigationPushButton`: 导航按钮。
- `NavigationSeparator`: 导航分隔符。
- `NavigationToolButton`: 导航工具按钮。
- `NavigationTreeWidget`: 导航树状组件。
- `NavigationTreeWidgetBase`: 导航树状组件基类。
- `NavigationAvatarWidget`: 导航头像组件。
- `NavigationPanel`: 导航面板。
- `NavigationItemPosition`: 导航项位置。
- `NavigationDisplayMode`: 导航显示模式。
- `NavigationInterface`: 导航界面。
- `NavigationBarPushButton`: 导航栏按钮。
- `NavigationBar`: 导航栏。
- `Pivot`: 枢轴导航。
- `PivotItem`: 枢轴导航项。
- `SegmentedItem`: 分段项。
- `SegmentedWidget`: 分段组件。
- `SegmentedToolItem`: 分段工具项。
- `SegmentedToolWidget`: 分段工具组件。
- `SegmentedToggleToolItem`: 分段切换工具项。
- `SegmentedToggleToolWidget`: 分段切换工具组件。
- `BreadcrumbBar`: 面包屑导航栏。
- `BreadcrumbItem`: 面包屑导航项。

#### **`date_time`**

该模块提供日期和时间相关的组件。

- `CalendarPicker`: 日历选择器。
- `FastCalendarPicker`: 快速日历选择器。
- `DatePickerBase`: 日期选择器基类。
- `DatePicker`: 日期选择器。
- `ZhDatePicker`: 中文日期选择器。
- `PickerBase`: 选择器基类。
- `PickerPanel`: 选择器面板。
- `PickerColumnFormatter`: 选择器列格式化工具。
- `TimePicker`: 时间选择器。
- `AMTimePicker`: AM/PM 时间选择器。

#### **`material`**

该模块提供具有亚克力效果的组件。

- `AcrylicMenu`: 亚克力菜单。
- `AcrylicLineEditMenu`: 亚克力行编辑菜单。
- `AcrylicCheckableMenu`: 亚克力可选中菜单。
- `AcrylicCheckableSystemTrayMenu`: 亚克力可选中系统托盘菜单。
- `AcrylicSystemTrayMenu`: 亚克力系统托盘菜单。
- `AcrylicLineEditBase`: 亚克力行编辑基类。
- `AcrylicLineEdit`: 亚克力行编辑。
- `AcrylicSearchLineEdit`: 亚克力搜索行编辑。
- `AcrylicComboBox`: 亚克力下拉框。
- `AcrylicComboBoxSettingCard`: 亚克力下拉框设置卡片。
- `AcrylicEditableComboBox`: 亚克力可编辑下拉框。
- `AcrylicWidget`: 亚克力组件。
- `AcrylicBrush`: 亚克力画刷。
- `AcrylicFlyoutView`: 亚克力弹出视图。
- `AcrylicFlyoutViewBase`: 亚克力弹出视图基类。
- `AcrylicFlyout`: 亚克力弹出框。
- `AcrylicToolTip`: 亚克力工具提示。
- `AcrylicToolTipFilter`: 亚克力工具提示过滤器。