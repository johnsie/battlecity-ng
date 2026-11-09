class_name PremiumBattlefield
extends Node2D

const TILE := 96.0
const GROUND_A := Color("#334744")
const GROUND_B := Color("#3b5049")
const GRID := Color(0.7, 0.82, 0.72, 0.055)

var player_position := Vector2.ZERO
var player_heading := 0.0
var turret_heading := 0.0
var player_health := 78
var projectiles: Array[Dictionary] = []
var effects: Array[Dictionary] = []
var enemies := [
	{"position": Vector2(-330, 130), "heading": -0.7, "health": 100, "heavy": false},
	{"position": Vector2(410, -175), "heading": 2.4, "health": 100, "heavy": true},
]
var buildings := [
	{"position": Vector2(-510, -300), "kind": "COMMAND", "health": 100},
	{"position": Vector2(35, -380), "kind": "FACTORY", "health": 74},
	{"position": Vector2(540, 275), "kind": "HOSPITAL", "health": 100},
]
var pickups := [
	{"position": Vector2(250, 120), "kind": "SUPPLY"},
	{"position": Vector2(-180, -210), "kind": "PLASMA"},
]
var elapsed := 0.0

func set_player_state(world_position: Vector2, body_heading: float, aim_heading: float, health: int) -> void:
	player_position = world_position
	player_heading = body_heading
	turret_heading = aim_heading
	player_health = health
	queue_redraw()

func spawn_projectile(position: Vector2, velocity: Vector2) -> void:
	projectiles.append({"position": position, "velocity": velocity, "life": 1.25})
	effects.append({"position": position, "life": 0.13, "max_life": 0.13, "kind": "muzzle"})
	queue_redraw()

func step(delta: float) -> void:
	elapsed += delta
	for projectile in projectiles:
		projectile.position += projectile.velocity * delta
		projectile.life -= delta
	projectiles = projectiles.filter(func(value: Dictionary) -> bool: return value.life > 0.0)
	for effect in effects:
		effect.life -= delta
	effects = effects.filter(func(value: Dictionary) -> bool: return value.life > 0.0)
	queue_redraw()

func _draw() -> void:
	_draw_ground()
	_draw_routes()
	for building in buildings:
		_draw_building(building)
	for pickup in pickups:
		_draw_pickup(pickup)
	for enemy in enemies:
		_draw_tank(enemy.position, enemy.heading, enemy.heading, Color("#bd554d"), enemy.heavy, false)
	for projectile in projectiles:
		_draw_projectile(projectile)
	_draw_tank(player_position, player_heading, turret_heading, Color("#61b886"), false, true)
	for effect in effects:
		_draw_effect(effect)

func _draw_ground() -> void:
	for y in range(-12, 13):
		for x in range(-18, 19):
			var hash_value: int = absi(x * 92821 + y * 68917 + x * y * 31)
			var colour := GROUND_A if hash_value % 3 else GROUND_B
			var rect := Rect2(Vector2(x, y) * TILE, Vector2.ONE * TILE)
			draw_rect(rect, colour)
			draw_line(rect.position, rect.position + Vector2(TILE, 0), GRID, 1.0)
			draw_line(rect.position, rect.position + Vector2(0, TILE), GRID, 1.0)
			if hash_value % 11 == 0:
				var stone := rect.position + Vector2(18 + hash_value % 55, 20 + (hash_value / 7) as int % 50)
				draw_circle(stone, 4.0 + hash_value % 5, Color(0.18, 0.24, 0.23, 0.65))

func _draw_routes() -> void:
	var road := PackedVector2Array([Vector2(-950, 390), Vector2(-430, 150), Vector2(40, 70), Vector2(560, -70), Vector2(980, -360)])
	draw_polyline(road, Color(0.17, 0.20, 0.19, 0.8), 105.0, true)
	draw_polyline(road, Color(0.31, 0.34, 0.31, 0.9), 82.0, true)
	draw_polyline(road, Color(0.67, 0.62, 0.44, 0.35), 3.0, true)

func _draw_building(building: Dictionary) -> void:
	var p: Vector2 = building.position
	var accent := Color("#67c8d4")
	if building.kind == "FACTORY": accent = Color("#e58b4c")
	if building.kind == "HOSPITAL": accent = Color("#64d98a")
	draw_set_transform(p)
	_draw_flat_ellipse(Vector2(8, 30), Vector2(78, 34), Color(0.02, 0.03, 0.035, 0.40))
	draw_colored_polygon(PackedVector2Array([Vector2(-66, 30), Vector2(-66, -35), Vector2(-30, -68), Vector2(58, -55), Vector2(72, 18), Vector2(35, 47)]), Color("#58646a"))
	draw_polyline(PackedVector2Array([Vector2(-66, 30), Vector2(-66, -35), Vector2(-30, -68), Vector2(58, -55), Vector2(72, 18), Vector2(35, 47), Vector2(-66, 30)]), Color("#aeb9b8"), 4.0, true)
	draw_rect(Rect2(-48, -29, 100, 12), Color(accent, 0.7))
	draw_string(ThemeDB.fallback_font, Vector2(-44, 4), building.kind, HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Color("#e8f2ee"))
	var health_width: float = 100.0 * building.health / 100.0
	draw_rect(Rect2(-50, 57, 100, 5), Color(0.05, 0.07, 0.07, 0.75))
	draw_rect(Rect2(-50, 57, health_width, 5), accent)
	draw_set_transform(Vector2.ZERO)

func _draw_tank(position: Vector2, body_angle: float, aim_angle: float, colour: Color, heavy: bool, player: bool) -> void:
	var size := Vector2(70, 48) if heavy else Vector2(58, 40)
	draw_set_transform(position)
	_draw_flat_ellipse(Vector2(7, 17), Vector2(size.x * 0.62, size.y * 0.45), Color(0.01, 0.02, 0.02, 0.48))
	draw_set_transform(position, body_angle)
	draw_rect(Rect2(-size.x * 0.5, -size.y * 0.5, size.x, size.y), Color("#18221f"), true)
	draw_rect(Rect2(-size.x * 0.43, -size.y * 0.36, size.x * 0.86, size.y * 0.72), colour, true)
	draw_line(Vector2(-size.x * 0.34, -size.y * 0.37), Vector2(size.x * 0.35, -size.y * 0.37), Color("#d2ddd5"), 2.0)
	draw_set_transform(position, aim_angle)
	draw_circle(Vector2.ZERO, 15.0 if heavy else 12.0, colour.lightened(0.12))
	draw_rect(Rect2(3, -4, 43 if heavy else 36, 8), Color("#202b28"), true)
	draw_circle(Vector2(43 if heavy else 36, 0), 4.5, Color("#d4ded6"))
	draw_set_transform(Vector2.ZERO)
	if player:
		draw_arc(position, 44.0, 0.0, TAU, 48, Color(0.55, 1.0, 0.75, 0.3), 2.0)

func _draw_projectile(projectile: Dictionary) -> void:
	var p: Vector2 = projectile.position
	var direction: Vector2 = projectile.velocity.normalized()
	draw_line(p - direction * 30.0, p, Color(1.0, 0.55, 0.18, 0.25), 8.0)
	draw_circle(p, 5.5, Color("#fff2b0"))

func _draw_pickup(pickup: Dictionary) -> void:
	var p: Vector2 = pickup.position
	var pulse := 1.0 + sin(elapsed * 4.0 + p.x) * 0.08
	var colour := Color("#e9bd58") if pickup.kind == "SUPPLY" else Color("#9b76ef")
	draw_circle(p, 28.0 * pulse, Color(colour, 0.12))
	draw_circle(p, 13.0, colour)
	draw_string(ThemeDB.fallback_font, p + Vector2(-28, 38), pickup.kind, HORIZONTAL_ALIGNMENT_CENTER, 56, 13, Color("#f4f4e9"))

func _draw_effect(effect: Dictionary) -> void:
	var ratio: float = effect.life / effect.max_life
	draw_circle(effect.position, 23.0 * (1.2 - ratio), Color(1.0, 0.55, 0.18, ratio * 0.65))
	draw_circle(effect.position, 8.0 * ratio, Color(1.0, 0.95, 0.72, ratio))

func _draw_flat_ellipse(center: Vector2, radii: Vector2, colour: Color) -> void:
	var points := PackedVector2Array()
	for index in range(32):
		var angle := TAU * index / 32.0
		points.append(center + Vector2(cos(angle) * radii.x, sin(angle) * radii.y))
	draw_colored_polygon(points, colour)
