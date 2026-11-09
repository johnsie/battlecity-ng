class_name CommandHUD
extends Control

const PANEL_WIDTH := 264.0
const PANEL := Color("#111b22")
const PANEL_RAISED := Color("#1b2c36")
const EDGE := Color("#527080")
const CYAN := Color("#72d7e5")
const AMBER := Color("#f0bb59")
const TEXT := Color("#e7f0f2")

var health := 78
var speed := 0.0
var objective := "DEFEND NORTH FACTORY"
var objective_range := 420
var debug_visible := false
var fps := 0
var high_contrast := false
var buttons := ["STAFF", "MAP", "INFO", "POINTS", "OPTIONS", "HELP"]

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_PASS
	queue_redraw()

func set_status(new_health: int, new_speed: float, new_range: int, new_fps: int) -> void:
	health = new_health
	speed = new_speed
	objective_range = new_range
	fps = new_fps
	queue_redraw()

func _draw() -> void:
	var right := size.x
	var left := right - PANEL_WIDTH
	var panel_colour := Color("#061017") if high_contrast else PANEL
	var edge_colour := Color("#a8f3ff") if high_contrast else EDGE
	draw_rect(Rect2(left, 0, PANEL_WIDTH, size.y), panel_colour)
	draw_rect(Rect2(left, 0, 3, size.y), edge_colour)
	draw_rect(Rect2(left + 10, 12, PANEL_WIDTH - 20, 190), Color("#071218"))
	draw_rect(Rect2(left + 12, 14, PANEL_WIDTH - 24, 186), Color("#102b35"))
	draw_circle(Vector2(left + PANEL_WIDTH * 0.5, 107), 74, Color("#09171e"))
	for radius in [28, 52, 74]:
		draw_arc(Vector2(left + PANEL_WIDTH * 0.5, 107), radius, 0, TAU, 64, Color(0.35, 0.72, 0.78, 0.22), 1.0)
	draw_line(Vector2(left + 38, 107), Vector2(right - 38, 107), Color(0.35, 0.72, 0.78, 0.18))
	draw_line(Vector2(left + 132, 31), Vector2(left + 132, 183), Color(0.35, 0.72, 0.78, 0.18))
	draw_circle(Vector2(left + 132, 107), 4, CYAN)
	draw_circle(Vector2(left + 177, 78), 4, Color("#d95a51"))
	_text(Vector2(left + 20, 225), "COMMAND STATUS", 16, CYAN)
	_text(Vector2(left + 20, 252), "ARMOUR", 13, TEXT)
	draw_rect(Rect2(left + 88, 243, 150, 17), Color("#071014"))
	draw_rect(Rect2(left + 91, 246, 144.0 * health / 100.0, 11), Color("#d8524c") if health < 30 else Color("#55c781"))
	_text(Vector2(left + 20, 282), "SPEED", 13, TEXT)
	_text(Vector2(left + 174, 282), "%03d" % int(speed), 13, CYAN)
	_text(Vector2(left + 20, 318), "ACTIVE OBJECTIVE", 13, AMBER)
	draw_rect(Rect2(left + 14, 329, PANEL_WIDTH - 28, 63), PANEL_RAISED)
	_text(Vector2(left + 25, 353), objective, 14, TEXT)
	_text(Vector2(left + 25, 377), "%dm" % objective_range, 13, CYAN)
	_text(Vector2(left + 20, 425), "INVENTORY", 13, CYAN)
	for row in range(2):
		for column in range(4):
			var slot := Rect2(left + 18 + column * 57, 438 + row * 54, 49, 46)
			draw_rect(slot, Color("#0a151b"))
			draw_rect(slot, EDGE, false, 1.0)
	# Supply crate in the first slot, keeping empty slots calm and readable.
	draw_rect(Rect2(left + 33, 451, 20, 17), Color("#d7a34c"))
	draw_line(Vector2(left + 43, 451), Vector2(left + 43, 468), Color("#fff0b0"), 2.0)
	var button_y := 558.0
	for index in range(buttons.size()):
		var row: int = index / 2
		var column := index % 2
		var rect := Rect2(left + 16 + column * 119, button_y + row * 38, 110, 30)
		draw_rect(rect, PANEL_RAISED)
		draw_rect(rect, EDGE, false, 1.0)
		_text(rect.position + Vector2(10, 20), buttons[index], 13, TEXT)
	var build_rect := Rect2(left + 16, button_y + 122, PANEL_WIDTH - 32, 38)
	draw_rect(build_rect, Color("#244453"))
	draw_rect(build_rect, CYAN, false, 1.0)
	_text(build_rect.position + Vector2(78, 25), "BUILD", 15, TEXT)
	var board_top := button_y + 177
	if size.y > board_top + 100:
		draw_rect(Rect2(left + 14, board_top, PANEL_WIDTH - 28, size.y - board_top - 14), Color("#0b1c25"))
		_text(Vector2(left + 24, board_top + 24), "WEEKLY TOP", 14, CYAN)
		_text(Vector2(left + 24, board_top + 49), "1. nightowl     142", 13, AMBER)
		_text(Vector2(left + 24, board_top + 70), "2. redbaron     117", 13, TEXT)
		_text(Vector2(left + 24, board_top + 91), "You: #7          34", 13, Color("#bcecf2"))
	if debug_visible:
		_text(Vector2(18, size.y - 18), "MODERN PREVIEW  |  %d FPS  |  OFFLINE / NO AUTHORITY" % fps, 13, Color(0.78, 0.9, 0.9, 0.82))

func _text(position: Vector2, value: String, font_size: int, colour: Color) -> void:
	draw_string(ThemeDB.fallback_font, position, value, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, colour)
