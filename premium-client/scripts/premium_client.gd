extends Node

@onready var battlefield: PremiumBattlefield = $Battlefield
@onready var camera: PresentationCamera = $Battlefield/PresentationCamera
@onready var hud: CommandHUD = $Interface/CommandHUD
@onready var modern_menu: ModernMenu = $Interface/ModernMenu

var player_position := Vector2.ZERO
var velocity := Vector2.ZERO
var body_heading := 0.0
var turret_heading := 0.0
var health := 78
var shot_cooldown := 0.0
var diagnostics_elapsed := 0.0

const MAX_SPEED := 285.0
const ACCELERATION := 740.0
const BRAKING := 920.0
const SHOT_INTERVAL := 0.22

func _ready() -> void:
	get_viewport().get_window().min_size = Vector2i(960, 540)
	battlefield.set_player_state(player_position, body_heading, turret_heading, health)
	camera.follow(player_position, velocity)
	modern_menu.start_requested.connect(_start_preview)
	modern_menu.accessibility_changed.connect(_set_accessibility)
	if OS.has_environment("BC_PREMIUM_CAPTURE"):
		_capture_test_frame.call_deferred()

func _start_preview() -> void:
	# The demonstration remains an offline presentation layer until the protocol
	# workstream reaches its connection gate.
	pass

func _set_accessibility(reduced_motion: bool, high_contrast: bool) -> void:
	camera.motion_scale = 0.0 if reduced_motion else 1.0
	hud.high_contrast = high_contrast
	hud.queue_redraw()

func _capture_test_frame() -> void:
	# CI can capture the deterministic mock scene without needing account or server data.
	for frame in range(8):
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var image := get_viewport().get_texture().get_image()
	var result := image.save_png(OS.get_environment("BC_PREMIUM_CAPTURE"))
	if result != OK:
		push_error("Could not save premium-client capture: %s" % error_string(result))
		get_tree().quit(1)
	else:
		get_tree().quit()

func _process(delta: float) -> void:
	var input_vector := Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	# WASD is handled explicitly so the prototype works without hidden InputMap state.
	input_vector += Vector2(
		float(Input.is_physical_key_pressed(KEY_D)) - float(Input.is_physical_key_pressed(KEY_A)),
		float(Input.is_physical_key_pressed(KEY_S)) - float(Input.is_physical_key_pressed(KEY_W))
	).limit_length(1.0)
	input_vector = input_vector.limit_length(1.0)
	if input_vector.length_squared() > 0.01:
		velocity = velocity.move_toward(input_vector * MAX_SPEED, ACCELERATION * delta)
		body_heading = lerp_angle(body_heading, input_vector.angle(), minf(1.0, delta * 8.0))
	else:
		velocity = velocity.move_toward(Vector2.ZERO, BRAKING * delta)
	player_position += velocity * delta
	var visible_world_center := Vector2(get_viewport().get_visible_rect().size.x - camera.hud_width, get_viewport().get_visible_rect().size.y) * 0.5
	var aim_world := camera.get_screen_center_position() + get_viewport().get_mouse_position() - visible_world_center
	turret_heading = (aim_world - player_position).angle()
	shot_cooldown = maxf(0.0, shot_cooldown - delta)
	if (Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT) or Input.is_physical_key_pressed(KEY_SPACE)) and shot_cooldown <= 0.0:
		_fire()
	battlefield.set_player_state(player_position, body_heading, turret_heading, health)
	battlefield.step(delta)
	camera.follow(player_position, velocity)
	var objective_range := int(player_position.distance_to(Vector2(35, -380)) / 10.0)
	diagnostics_elapsed += delta
	hud.set_status(health, velocity.length() * 0.42, objective_range, Engine.get_frames_per_second())

func _fire() -> void:
	shot_cooldown = SHOT_INTERVAL
	var direction := Vector2.from_angle(turret_heading)
	var muzzle := player_position + direction * 48.0
	battlefield.spawn_projectile(muzzle, direction * 760.0)
	camera.add_impulse(-direction * 4.5)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_F1:
			hud.debug_visible = not hud.debug_visible
			hud.queue_redraw()
		elif event.keycode == KEY_ESCAPE:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
			modern_menu.open()
