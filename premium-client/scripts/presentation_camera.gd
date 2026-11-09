class_name PresentationCamera
extends Camera2D

@export var hud_width := 264.0
@export var look_ahead_distance := 54.0
@export var impulse_decay := 13.0
@export var max_impulse := 12.0

var target_position := Vector2.ZERO
var target_velocity := Vector2.ZERO
var impulse := Vector2.ZERO
var motion_scale := 1.0

func follow(world_position: Vector2, velocity: Vector2) -> void:
	target_position = world_position
	target_velocity = velocity

func add_impulse(amount: Vector2) -> void:
	impulse = (impulse + amount * motion_scale).limit_length(max_impulse)

func _process(delta: float) -> void:
	var direction := target_velocity.normalized() if target_velocity.length_squared() > 1.0 else Vector2.ZERO
	# Shift the world left so the usable play area, excluding the retained HUD, is centred.
	var panel_compensation := Vector2(hud_width * 0.5, 0.0)
	position = target_position + direction * look_ahead_distance + panel_compensation + impulse
	impulse = impulse.move_toward(Vector2.ZERO, impulse_decay * delta * max_impulse)
