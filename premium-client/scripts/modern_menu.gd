class_name ModernMenu
extends Control

signal start_requested
signal accessibility_changed(reduced_motion: bool, high_contrast: bool)

var reduced_motion := false
var high_contrast := false

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_build_menu()

func _build_menu() -> void:
	var shade := ColorRect.new()
	shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	shade.color = Color(0.018, 0.028, 0.037, 0.94)
	add_child(shade)

	var centre := CenterContainer.new()
	centre.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(centre)

	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(570, 590)
	centre.add_child(panel)

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 52)
	margin.add_theme_constant_override("margin_right", 52)
	margin.add_theme_constant_override("margin_top", 44)
	margin.add_theme_constant_override("margin_bottom", 44)
	panel.add_child(margin)

	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 16)
	margin.add_child(column)

	var eyebrow := Label.new()
	eyebrow.text = "BATTLE CITY NG"
	eyebrow.add_theme_color_override("font_color", Color("#72d7e5"))
	eyebrow.add_theme_font_size_override("font_size", 18)
	column.add_child(eyebrow)

	var title := Label.new()
	title.text = "MODERN PREVIEW"
	title.add_theme_font_size_override("font_size", 42)
	title.add_theme_color_override("font_color", Color("#f1f5f4"))
	column.add_child(title)

	var description := Label.new()
	description.text = "Try the new battlefield presentation and retained command HUD. This preview is offline and does not change your character or the live world."
	description.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	description.custom_minimum_size.y = 82
	description.add_theme_font_size_override("font_size", 17)
	description.add_theme_color_override("font_color", Color("#b9c8ce"))
	column.add_child(description)

	var start := Button.new()
	start.text = "ENTER MODERN PREVIEW"
	start.custom_minimum_size.y = 56
	start.tooltip_text = "Open the offline modern presentation demonstration"
	start.pressed.connect(_start)
	column.add_child(start)

	var classic := Button.new()
	classic.text = "RETURN TO CLASSIC WEB CLIENT"
	classic.custom_minimum_size.y = 48
	classic.tooltip_text = "Return to the current online Battle City client"
	classic.pressed.connect(_return_to_classic)
	column.add_child(classic)

	var rule := HSeparator.new()
	column.add_child(rule)

	var accessibility_title := Label.new()
	accessibility_title.text = "ACCESSIBILITY"
	accessibility_title.add_theme_font_size_override("font_size", 16)
	accessibility_title.add_theme_color_override("font_color", Color("#72d7e5"))
	column.add_child(accessibility_title)

	var motion := CheckButton.new()
	motion.text = "Reduce camera motion"
	motion.tooltip_text = "Disable recoil movement and reduce animated presentation"
	motion.toggled.connect(_set_reduced_motion)
	column.add_child(motion)

	var contrast := CheckButton.new()
	contrast.text = "High-contrast HUD"
	contrast.tooltip_text = "Increase command HUD edge and text contrast"
	contrast.toggled.connect(_set_high_contrast)
	column.add_child(contrast)

	var controls := Label.new()
	controls.text = "Drive: WASD or arrow keys    Aim: mouse    Fire: click or Space\nPress Escape in the preview to return to this menu."
	controls.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	controls.add_theme_font_size_override("font_size", 14)
	controls.add_theme_color_override("font_color", Color("#8fa4ad"))
	column.add_child(controls)

	start.grab_focus()

func _start() -> void:
	hide()
	start_requested.emit()

func open() -> void:
	show()
	var first := find_children("*", "Button", true, false)
	if not first.is_empty():
		(first[0] as Button).grab_focus()

func _return_to_classic() -> void:
	if OS.has_feature("web"):
		JavaScriptBridge.eval("window.location.href = '../';")
	else:
		OS.shell_open("https://bc.outiva.com/")

func _set_reduced_motion(enabled: bool) -> void:
	reduced_motion = enabled
	accessibility_changed.emit(reduced_motion, high_contrast)

func _set_high_contrast(enabled: bool) -> void:
	high_contrast = enabled
	accessibility_changed.emit(reduced_motion, high_contrast)
