#include <assert.h>
#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include "entities.c"

#define ASPECT_RATIO (16.0f/10.0f)
#define VIEW_WIDTH 1440
#define VIEW_HEIGHT (VIEW_WIDTH/ASPECT_RATIO)

#define BULLET_DEFAULT_SPEED 100.0f
#define BULLET_ENERGY_COST 3.0f
#define BULLET_RADIUS 18.0f

#define PLAYER_MINIMUM_RADIUS 30.0f   


#define MINIMUM(a, b) ((a) < (b) ? (a) : (b))
#define MAXIMUM(a, b) ((a) > (b) ? (a) : (b))
	

typedef struct Bullet {
	Vector2 position;
	Vector2 velocity;
} Bullet;

#define NUM_PLAYERS 2

typedef struct Player_Parameters
{
	KeyboardKey key_left;
	KeyboardKey key_right;
	KeyboardKey key_up;
	KeyboardKey key_down;
	Sound sound_pop;
	Sound sound_hit;
	float acceleration_force;
	float friction;
	Color color;
} Player_Parameters;

typedef struct Player {
	Vector2 position;
	Vector2 velocity;
	int health;
	float energy;
	double controls_text_timeout;
#define MAX_ACTIVE_BULLETS 2048
	int active_bullets;
	Bullet bullets[MAX_ACTIVE_BULLETS];
	Player_Parameters *parameters;
} Player;

typedef struct Ring {
	Vector2 position;
	int player_index;
	float angle;
	float t;
} Ring;

#define MAX_ACTIVE_RINGS 128

typedef struct Ring_Pool {
	int active_rings;
	Ring rings[MAX_ACTIVE_RINGS];
} Ring_Pool;

typedef struct Game_State {
	Player players[NUM_PLAYERS];
	Player_Parameters player_parameters[NUM_PLAYERS];
	Ring_Pool ring_pool;
	Sound sound_win;
	int triumphant_player;
	float title_alpha;
} Game_State;

float sign_float(float v) {
	return v > 0 ? 1.0f : v < 0 ? -1.0f : 0.0f;
}

float abs_float(float v) {
	return v >= 0 ? v : -v;
}

Vector2 Vector2Sign(Vector2 v) {
	Vector2 result;

	result.x = sign_float(v.x); 
	result.y = sign_float(v.y); 

	return result;
}

Vector2 Vector2Abs(Vector2 v) {
	Vector2 result;

	result.x = abs_float(v.x); 
	result.y = abs_float(v.y); 

	return result;
}

Vector2 Vector2NormalizeOrZero(Vector2 v) {
	float length = Vector2Length(v);
	
	if (length == 0.0f) return v;

	return Vector2Scale(v, 1.0f/length);
}

int player_compute_max_bullets(Player *player) {
	return (int)player->energy/BULLET_ENERGY_COST;
}

float radius_from_energy(float energy) {
	return PLAYER_MINIMUM_RADIUS + energy*1.1f;
}


bool hit_is_hard_enough(float hit, float delta_time) {

	return 4000.0f*delta_time <= fabs(hit);
}

float random_01() {
	int i = GetRandomValue(0, (1<<23)-1) | (127 << 23);

	float result = *(float *)&i - 1.0f;

	return result;
}

void spawn_bullets(Player *player) {

	int count = player_compute_max_bullets(player);
	float speed = 400;

	if (player->active_bullets + count > MAX_ACTIVE_BULLETS) {
		count = MAX_ACTIVE_BULLETS - player->active_bullets;
	}

	player->energy -= count * BULLET_ENERGY_COST;

	if (player->energy < 0) {
		player->energy = 0.0f; 
	}

	float angle_quantum = 2.0f*PI / (float)count;

	float angle = random_01()*2.0f*PI;

	for (int i = 0; i < count; ++i) {
		Bullet *bullet = &player->bullets[player->active_bullets + i];
		bullet->position = player->position;
		bullet->velocity = Vector2Scale((Vector2){cosf(angle), sinf(angle)}, speed);
		angle += angle_quantum;
	}

	player->active_bullets += count;

	PlaySound(player->parameters->sound_pop);
}


void spawn_ring(Game_State *game_state, Vector2 position, int player_index, float ring_angle) {
	
	Ring_Pool *ring_pool = &game_state->ring_pool;

	if (ring_pool->active_rings < MAX_ACTIVE_RINGS) {

		ring_pool->rings[ring_pool->active_rings++] = (Ring){
			.position = position,
			.player_index = player_index,
			.t = 0.0f,
			.angle = ring_angle,
		};

	}

}

void player_update(Player *player, float dt) {
	Vector2 player_direction_control = {0.0f, 0.0f};

	Player_Parameters *parameters = player->parameters;

	if (IsKeyDown(parameters->key_left)) player_direction_control.x = -1.0f;
	if (IsKeyDown(parameters->key_right)) player_direction_control.x += 1.0f;
	if (IsKeyDown(parameters->key_up)) player_direction_control.y = -1.0f;
	if (IsKeyDown(parameters->key_down)) player_direction_control.y += 1.0f;

	Vector2 velocity_sign = Vector2Sign(player->velocity);

	Vector2 d = Vector2Subtract(velocity_sign, player_direction_control);

	player_direction_control = Vector2NormalizeOrZero(player_direction_control);

	d = Vector2Abs(d);

	d = Vector2Scale(d, 5.5f);

	// d = Vector2Add(d, Vector2One());
	d = Vector2One();

	Vector2 acceleration = Vector2Multiply(
		Vector2Scale(
			player_direction_control,
			parameters->acceleration_force * dt
		),
		d
	);

	// printf("(%.2f, %.2f)\n", player_direction_control.x, player_direction_control.y);

	player->velocity = Vector2Add(
		player->velocity,
		acceleration
	);


	player->velocity = Vector2Subtract(
		player->velocity,
		Vector2Scale(player->velocity, parameters->friction * dt)
	);

	Vector2 to_position = player->position = Vector2Add(
		player->position,
		Vector2Scale(player->velocity, dt)
	);

	float player_radius = radius_from_energy(player->energy);

	if (player->velocity.x > 0) {
		float difference = VIEW_WIDTH - (to_position.x + player_radius);

		if (difference < 0) {
			to_position.x = VIEW_WIDTH - player_radius;
			
			if (hit_is_hard_enough(player->velocity.x, dt)) spawn_bullets(player);

			player->velocity.x = (1.0f-parameters->friction)*(-player->velocity.x + difference);	
		}
	}
	
	if (player->velocity.x < 0) {
		float difference = 0 - (to_position.x - player_radius);

		if (difference > 0) {
			to_position.x = 0 + player_radius;

			if (hit_is_hard_enough(player->velocity.x, dt)) spawn_bullets(player);

			player->velocity.x = (1.0f-parameters->friction)*(-player->velocity.x + difference);
		}
	}

	if (player->velocity.y > 0) {
		float difference = VIEW_HEIGHT - (to_position.y + player_radius);

		if (difference < 0) {
			to_position.y = VIEW_HEIGHT - player_radius;
			
			if (hit_is_hard_enough(player->velocity.y, dt)) spawn_bullets(player);

			player->velocity.y = (1.0f-parameters->friction)*(-player->velocity.y + difference);
		}
	}
	
	if (player->velocity.y < 0) {
		float difference = 0 - (to_position.y - player_radius);

		if (difference > 0) {
			to_position.y = 0 + player_radius;
			
			if (hit_is_hard_enough(player->velocity.y, dt)) spawn_bullets(player);
			
			player->velocity.y = (1.0f-parameters->friction)*(-player->velocity.y + difference);
		}
	}

	player->position = to_position;

	float speed = Vector2Length(player->velocity);

	player->energy += speed * dt / (player->energy*1.0f + 1.0f);

}

bool position_outside_playzone(Vector2 position) {
	return (
		position.x < -100 ||
		position.x >= VIEW_WIDTH + 100 ||
		position.y < -100 ||
		position.y >= VIEW_HEIGHT + 100
	);
}

void draw_text_shadowed(Font font, const char *text, Vector2 position, float font_size, float font_spacing, Color front_color, Color background_color) {
	DrawTextEx(
		font,
		text,
		Vector2Add(position, (Vector2){3, 3}),
		font_size,
		font_spacing,
		background_color
	);

	DrawTextEx(
		font,
		text,
		position,
		font_size,
		font_spacing,
		front_color
	);
}


void reset_game(Game_State *game_state) {
	game_state->triumphant_player = -1;

	game_state->title_alpha = 1.0f;

	game_state->ring_pool.active_rings = 0;

	game_state->players[0] = (Player){
		.position = { GetScreenWidth() * 2.0f/3.0f, GetScreenHeight() / 2.0f },
		.velocity = { 0.0f, 0.0f },
		.health = 30,
		.energy = 0.0f,
		.controls_text_timeout = 0,
		.active_bullets = 0,
		.bullets = {0},
		.parameters = &game_state->player_parameters[0],
	};
	game_state->players[1] = (Player){
		.position = { GetScreenWidth() * 1.0f/3.0f, GetScreenHeight() / 2.0f },
		.velocity = { 0.0f, 0.0f },
		.health = 30,
		.energy = 0.0f,
		.controls_text_timeout = 0,
		.active_bullets = 0,
		.bullets = {0},
		.parameters = &game_state->player_parameters[1],
	};
}

int main(void)
{
	// Initialization
	//---------------------------------------------------------

	// Possible window flags
	/*
	FLAG_VSYNC_HINT
	FLAG_FULLSCREEN_MODE    -> not working properly -> wrong scaling!
	FLAG_WINDOW_RESIZABLE
	FLAG_WINDOW_UNDECORATED
	FLAG_WINDOW_TRANSPARENT
	FLAG_WINDOW_HIDDEN
	FLAG_WINDOW_MINIMIZED   -> Not supported on window creation
	FLAG_WINDOW_MAXIMIZED   -> Not supported on window creation
	FLAG_WINDOW_UNFOCUSED
	FLAG_WINDOW_TOPMOST
	FLAG_WINDOW_HIGHDPI     -> errors after minimize-resize, fb size is recalculated
	FLAG_WINDOW_ALWAYS_RUN
	FLAG_MSAA_4X_HINT
	*/

	InitAudioDevice();

	Game_State *game_state = malloc(sizeof(*game_state));
	assert(game_state);

	Player_Parameters *player_parameters = game_state->player_parameters;

	player_parameters[0] = (Player_Parameters){
		.key_left = KEY_LEFT,
		.key_right = KEY_RIGHT,
		.key_up = KEY_UP,
		.key_down = KEY_DOWN,
		.sound_pop = LoadSound("resources/pop.wav"),
		.sound_hit = LoadSound("resources/hit.wav"),
		.acceleration_force = 800.0f, 
		.friction = 0.5f,
		.color = (Color){240, 120, 0, 255},
	};
	player_parameters[1] = (Player_Parameters){
		.key_left = KEY_A,
		.key_right = KEY_D,
		.key_up = KEY_W,
		.key_down = KEY_S,
		.sound_pop = LoadSound("resources/pop2.wav"),
		.sound_hit = LoadSound("resources/hit2.wav"),
		.acceleration_force = 800.0f, 
		.friction = 0.5f,
		.color = (Color){0, 120, 240, 255},
	};

	game_state->sound_win = LoadSound("resources/win.wav");

	const char *title = "Juelsminde Jaust";

	// Set configuration flags for window creation
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(VIEW_WIDTH, VIEW_HEIGHT, title);
	ToggleFullscreen();
	HideCursor();


	reset_game(game_state);


	Camera2D camera = {
		.offset = {0},
		.target = {0},
		.rotation = 0.0f,
		.zoom = 1.0f,
	};

	Font default_font = GetFontDefault();


	//SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
	//----------------------------------------------------------

	// Main game loop
	while (!WindowShouldClose())    // Detect window close button or ESC key
	{
		float dt = GetFrameTime();
		double current_time = GetTime();

		// Update
		//-----------------------------------------------------

		if (IsKeyPressed(KEY_ENTER)) {
			if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
				ToggleFullscreen();  // modifies window size when scaling!
			}
			else {
				reset_game(game_state);
			}
		}

		if (game_state->triumphant_player < 0) {

			// Update players

			for (int player_index = 0; player_index < NUM_PLAYERS; ++player_index) {

				Player *player = game_state->players + player_index;
				Player *opponent = game_state->players + (1-player_index);

				float opponent_radius = radius_from_energy(opponent->energy);

				player_update(player, dt);

				if (game_state->title_alpha > 0) {
					game_state->title_alpha -= Vector2LengthSqr(player->velocity)*0.000001f;
				}

				for (int bullet_index = 0; bullet_index < player->active_bullets; ++bullet_index) {

					Bullet *bullet = &player->bullets[bullet_index];

					Vector2 bullet_position = bullet->position;

					bullet->position = Vector2Add(bullet_position, Vector2Scale(bullet->velocity, dt));

					bool bullet_overlaps_opponent = CheckCollisionCircles(bullet_position, BULLET_RADIUS, opponent->position, opponent_radius);

					if (bullet_overlaps_opponent || position_outside_playzone(bullet_position)) {
						player->bullets[bullet_index--] = player->bullets[--player->active_bullets];


						if (bullet_overlaps_opponent) {
							--opponent->health;
							
							Vector2 hit_direction = Vector2NormalizeOrZero(Vector2Subtract(bullet_position, opponent->position));

							float ring_angle = (180+90)-Vector2Angle(bullet_position, opponent->position);

							PlaySound(opponent->parameters->sound_hit);
							spawn_ring(game_state, bullet_position, player_index, ring_angle);

							if (opponent->health == 0) {
								game_state->triumphant_player = player_index;
								PlaySound(game_state->sound_win);
							}
							else {
							}
						}
					}
				}
			}

			// Update Rings
			for (int ring_index = 0; ring_index < game_state->ring_pool.active_rings; ++ring_index) {
				
				Ring *ring = &game_state->ring_pool.rings[ring_index];

				ring->t += dt * 2.5f;

				if (ring->t > 1.0f) {
					game_state->ring_pool.rings[ring_index--] = game_state->ring_pool.rings[--game_state->ring_pool.active_rings];
				}
			}
		}
		
		
		//
		//-----------------------------------------------------
		// Draw
		//-----------------------------------------------------
		//

		BeginDrawing();
		BeginMode2D(camera);


		ClearBackground((Color){255, 255, 255, 255});

		// Draw Title
		if (game_state->title_alpha > 0) {

			Color title_color = (Color){192, 192, 192, game_state->title_alpha*255.0f};

			float font_size = 100.0f;
			float font_spacing = 0.15f*font_size;

			Vector2 text_bounds = MeasureTextEx(default_font, title, font_size, font_spacing);
			Vector2 text_position = Vector2Add((Vector2){GetScreenWidth()/2.0f, GetScreenHeight()/3.0f}, Vector2Scale(text_bounds, -0.5f));

			DrawTextEx(default_font, title, text_position, font_size, font_spacing, title_color);

			font_size *= 0.5f;
			font_spacing *= 0.5f;

			const char *author = "By Jakob Kjær-Kammersgaard";
			text_bounds = MeasureTextEx(default_font, author, font_size, font_spacing);
			text_position = Vector2Add((Vector2){GetScreenWidth()/2.0f, GetScreenHeight()* 2.0f/3.0f}, Vector2Scale(text_bounds, -0.5f));

			DrawTextEx(default_font, author, text_position, font_size, font_spacing, title_color);
		}


		// Draw rings
		for (int ring_index = 0; ring_index < game_state->ring_pool.active_rings; ++ring_index) {
			
			Ring ring = game_state->ring_pool.rings[ring_index];

			Color ring_color = game_state->player_parameters[ring.player_index].color;

			float t1 = 1.0f - ring.t;
			float t2 = ring.t;
			t1 = 1.0f - t1*t1*t1;


			float outer_radius = t1*100.0f;
			float inner_radius = t2*100.0f;

			float start_angle = ring.angle - 60.0;
			float end_angle = ring.angle + 60.0f;

			DrawRing(ring.position, inner_radius, outer_radius, start_angle, end_angle, 10, ring_color);
		}

		//
		// Draw players
		//
		for (int player_index = 0; player_index < NUM_PLAYERS; ++player_index) {

			Player *player = game_state->players + player_index;

			Player_Parameters *parameters = player->parameters; 

			// Draw player's bullets
			for (int bullet_index = 0; bullet_index < player->active_bullets; ++bullet_index) {

				Bullet *bullet = &player->bullets[bullet_index];

				DrawCircleV(bullet->position, BULLET_RADIUS, parameters->color);

			}

			float radius = radius_from_energy(player->energy);
			float font_size = radius*1.0f;
			float font_spacing = font_size*0.15f;

			// Draw player's body
			DrawCircleV(player->position, radius, parameters->color);

			// Draw player's health text
			{
				const char *health_text_string = TextFormat("%i", player->health);


				Vector2 health_text_bounds = MeasureTextEx(default_font, health_text_string, font_size, font_spacing);

				Vector2 health_text_position = Vector2Add(player->position, Vector2Scale(health_text_bounds, -0.5f));

				draw_text_shadowed(default_font, health_text_string, health_text_position, font_size, font_spacing, WHITE, BLACK);
			}

			// Draw player's controls
			{
				float player_speed = Vector2Length(player->velocity);

				float max_speed_while_drawing_control_text = 4000.0f*dt;

				if (player_speed <= max_speed_while_drawing_control_text) {

					if (player->controls_text_timeout < current_time) {

						// float t = 1.0f - (player_speed / max_speed_while_drawing_control_text);

						Color controls_color = parameters->color;
						// controls_color.a = t*255.0f;

						const char *text = (const char *[]){"Arrow Keys", "W, A, S, D"}[player_index];

						font_size = 30.0f;
						font_spacing = font_size*0.15f;

						Vector2 text_bounds = MeasureTextEx(default_font, text, font_size, font_spacing);

						Vector2 text_position = Vector2Add(player->position, Vector2Scale(text_bounds, -0.5f));

						text_position.y -= radius*1.5f;

						DrawTextEx(default_font, text, text_position, font_size, font_spacing, controls_color);
					}
				}
				else {
					player->controls_text_timeout = current_time + 2.0;
				}
			}
		}

		int triumphant_player = game_state->triumphant_player;

		if (triumphant_player >= 0) {

			assert(triumphant_player < 2);

			DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){255, 255, 255, 128});

			DrawRectangle(0, GetScreenHeight() * 2.0f/6.0f, GetScreenWidth(), GetScreenHeight() * 2.0f/6.0f, player_parameters[triumphant_player].color);

			const char *win_text = (const char *[]){
				"Orange Player Wins",
				"Blue Player Wins",
			}[triumphant_player];

			float font_size = 100;
			float font_spacing = 10;

			Vector2 win_text_bounds = MeasureTextEx(default_font, win_text, font_size, font_spacing);

			Vector2 win_text_position = Vector2Scale(
				Vector2Subtract(
					(Vector2){GetScreenWidth(), GetScreenHeight()},
					win_text_bounds
				),
				0.5f
			);

			win_text_position.y -= 60;

			draw_text_shadowed(default_font, win_text, win_text_position, font_size, font_spacing, WHITE, BLACK);

			win_text = "Press [Enter] to Reset";

			win_text_bounds = MeasureTextEx(default_font, win_text, font_size*0.5f, font_spacing*0.5f);

			win_text_position = Vector2Scale(
				Vector2Subtract(
					(Vector2){GetScreenWidth(), GetScreenHeight()},
					win_text_bounds
				),
				0.5f
			);

			win_text_position.y += 60;


			draw_text_shadowed(default_font, win_text, win_text_position, font_size*0.5f, font_spacing*0.5f, WHITE, BLACK);


		}

#if 0
		Vector2 a = {300, 300};
		Vector2 b = Vector2Add(a, Vector2Scale((Vector2){cosf(current_time), sinf(current_time)}, 100.0f));

		DrawCircleV(a, 10, RED);
		DrawCircleV(b, 10, GREEN);

		if (IsKeyPressed(KEY_ONE)) {

			float angle = (360+90)-Vector2Angle(b, a);

			spawn_ring(game_state, b, 1, angle);
		}
#endif

		// DrawFPS(GetScreenWidth()-100, 10);
#if 0


		DrawText(TextFormat("Screen Size: [%i, %i]", GetScreenWidth(), GetScreenHeight()), 10, 40, 10, GREEN);

		DrawText(TextFormat("Frame Counter: %i", framesCounter), GetScreenWidth()/2, GetScreenHeight()/2, 20, GRAY);

		// Draw window state info
		DrawText("Following flags can be set after window creation:", 10, 60, 10, GRAY);
		if (IsWindowState(FLAG_FULLSCREEN_MODE)) DrawText("[F] FLAG_FULLSCREEN_MODE: on", 10, 80, 10, LIME);
		else DrawText("[F] FLAG_FULLSCREEN_MODE: off", 10, 80, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_RESIZABLE)) DrawText("[R] FLAG_WINDOW_RESIZABLE: on", 10, 100, 10, LIME);
		else DrawText("[R] FLAG_WINDOW_RESIZABLE: off", 10, 100, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_UNDECORATED)) DrawText("[D] FLAG_WINDOW_UNDECORATED: on", 10, 120, 10, LIME);
		else DrawText("[D] FLAG_WINDOW_UNDECORATED: off", 10, 120, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_HIDDEN)) DrawText("[H] FLAG_WINDOW_HIDDEN: on", 10, 140, 10, LIME);
		else DrawText("[H] FLAG_WINDOW_HIDDEN: off", 10, 140, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_MINIMIZED)) DrawText("[N] FLAG_WINDOW_MINIMIZED: on", 10, 160, 10, LIME);
		else DrawText("[N] FLAG_WINDOW_MINIMIZED: off", 10, 160, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_MAXIMIZED)) DrawText("[M] FLAG_WINDOW_MAXIMIZED: on", 10, 180, 10, LIME);
		else DrawText("[M] FLAG_WINDOW_MAXIMIZED: off", 10, 180, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_UNFOCUSED)) DrawText("[G] FLAG_WINDOW_UNFOCUSED: on", 10, 200, 10, LIME);
		else DrawText("[U] FLAG_WINDOW_UNFOCUSED: off", 10, 200, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_TOPMOST)) DrawText("[T] FLAG_WINDOW_TOPMOST: on", 10, 220, 10, LIME);
		else DrawText("[T] FLAG_WINDOW_TOPMOST: off", 10, 220, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_ALWAYS_RUN)) DrawText("[A] FLAG_WINDOW_ALWAYS_RUN: on", 10, 240, 10, LIME);
		else DrawText("[A] FLAG_WINDOW_ALWAYS_RUN: off", 10, 240, 10, MAROON);
		if (IsWindowState(FLAG_VSYNC_HINT)) DrawText("[V] FLAG_VSYNC_HINT: on", 10, 260, 10, LIME);
		else DrawText("[V] FLAG_VSYNC_HINT: off", 10, 260, 10, MAROON);

		DrawText("Following flags can only be set before window creation:", 10, 300, 10, GRAY);
		if (IsWindowState(FLAG_WINDOW_HIGHDPI)) DrawText("FLAG_WINDOW_HIGHDPI: on", 10, 320, 10, LIME);
		else DrawText("FLAG_WINDOW_HIGHDPI: off", 10, 320, 10, MAROON);
		if (IsWindowState(FLAG_WINDOW_TRANSPARENT)) DrawText("FLAG_WINDOW_TRANSPARENT: on", 10, 340, 10, LIME);
		else DrawText("FLAG_WINDOW_TRANSPARENT: off", 10, 340, 10, MAROON);
		if (IsWindowState(FLAG_MSAA_4X_HINT)) DrawText("FLAG_MSAA_4X_HINT: on", 10, 360, 10, LIME);
		else DrawText("FLAG_MSAA_4X_HINT: off", 10, 360, 10, MAROON);
#endif



		EndMode2D();
		
		// DrawText(TextFormat("Orange Health: %i", players[0].health), 30, 30, 20, WHITE);
		// DrawText(TextFormat("Blue Health: %i", players[1].health), 30, 60, 20, WHITE);
		
		EndDrawing();
		//-----------------------------------------------------
	}

	CloseAudioDevice();

	// De-Initialization
	//---------------------------------------------------------
	CloseWindow();        // Close window and OpenGL context
	//----------------------------------------------------------

	return 0;
}


