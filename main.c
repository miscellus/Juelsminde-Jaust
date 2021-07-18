#include <assert.h>
#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

// Unity-build
#include "jj_math.c"

#define BULLET_DEFAULT_SPEED 100.0f
#define BULLET_ENERGY_COST 3.0f
#define BULLET_RADIUS 15.0f
#define BULLET_TIME_END_FADE 7.0f
#define BULLET_TIME_BEGIN_FADE (BULLET_TIME_END_FADE-0.4f)

#define PLAYER_MINIMUM_RADIUS 30.0f   

#define FONT_SPACING_FOR_SIZE 0.15f


#define MINIMUM(a, b) ((a) < (b) ? (a) : (b))
#define MAXIMUM(a, b) ((a) > (b) ? (a) : (b))
	

typedef struct View {
	float width;
	float height;
	float scale;
	float inv_scale;
} View;

typedef struct Bullet {
	Vector2 position;
	Vector2 velocity;
	float time;
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
	float hit_animation_t;
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
	View view;
	Player players[NUM_PLAYERS];
	Player_Parameters player_parameters[NUM_PLAYERS];
	Ring_Pool ring_pool;
	Sound sound_win;
	int triumphant_player;
	float title_alpha;
} Game_State;


int player_compute_max_bullets(Player *player) {
	return (int)player->energy/BULLET_ENERGY_COST;
}

float radius_from_energy(float energy) {
	return PLAYER_MINIMUM_RADIUS + energy*1.1f;
}


bool hit_is_hard_enough(float hit, float delta_time) {

	return 4000.0f*delta_time <= fabs(hit);
}


uint64_t xorshift64(uint64_t *state)
{
	uint64_t x = *state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	return *state = x;
}

float random_01(uint64_t *random_state) {

	union {
		uint32_t as_int;
		float as_float;
	} val;

	val.as_int = (xorshift64(random_state)&((1 << 23)-1)) | (127 << 23);

	float result = val.as_float - 1.0f;

	return result;
}

void spawn_bullets(Player *player, uint64_t *random_state) {

	int count = player_compute_max_bullets(player);
	float speed = Vector2LengthSqr(player->velocity)/1565;

	if (player->active_bullets + count > MAX_ACTIVE_BULLETS) {
		count = MAX_ACTIVE_BULLETS - player->active_bullets;
	}

	player->energy -= count * BULLET_ENERGY_COST;

	if (player->energy < 0) {
		player->energy = 0.0f; 
	}

	float angle_quantum = 2.0f*PI / (float)count;

	float angle = random_01(random_state)*2.0f*PI;

	for (int i = 0; i < count; ++i) {
		Bullet *bullet = &player->bullets[player->active_bullets + i];
		bullet->position = player->position;
		bullet->velocity = Vector2Scale((Vector2){cosf(angle), sinf(angle)}, speed);
		bullet->time = 0;
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

bool position_outside_playzone(Vector2 position, View view) {
	return (
		position.x < -100 ||
		position.x >= view.width + 100 ||
		position.y < -100 ||
		position.y >= view.height + 100
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

	const int player_start_health = 30;

	game_state->players[0] = (Player){
		.position = { game_state->view.width * 2.0f/3.0f, game_state->view.height / 2.0f },
		.health = player_start_health,
		.hit_animation_t = 1.0f,
		.parameters = &game_state->player_parameters[0],
	};
	game_state->players[1] = (Player){
		.position = { game_state->view.width * 1.0f/3.0f, game_state->view.height / 2.0f },
		.health = player_start_health,
		.hit_animation_t = 1.0f,
		.parameters = &game_state->player_parameters[1],
	};
}

void set_window_to_monitor_dimensions(void) {
	int monitor_index = GetCurrentMonitor();
	int monitor_width = GetMonitorWidth(monitor_index);
	int monitor_height = GetMonitorHeight(monitor_index);
	SetWindowSize(monitor_width, monitor_height);
}

View get_view(void) {

	View result;

	float screen_width = GetScreenWidth();
	float screen_height = GetScreenHeight();

	result.scale = 0.5f*(screen_width/1440.0f + screen_height/900.0f);
	result.inv_scale = 1.0f/result.scale;
	result.width = screen_width*result.inv_scale;
	result.height = screen_height*result.inv_scale;
	return result;
}

int main(void)
{
#ifdef __APPLE__
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
	char path[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
	{
		// error!
	}
	CFRelease(resourcesURL);

	chdir(path);
#endif


	uint64_t random_state = 0x1955dc895a81c9;

	{
		long t = time(NULL);
		random_state ^= (t*13) ^ (t>>4);
	}

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
	assert(IsAudioDeviceReady());

	const char *title = "Juelsminde Jaust";

	// Set configuration flags for window creation
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(800, 600, title);

	set_window_to_monitor_dimensions();
	ToggleFullscreen();
	HideCursor();

	Game_State *game_state = malloc(sizeof(*game_state));
	assert(game_state);

	View view = game_state->view = get_view();

	Player_Parameters *player_parameters = game_state->player_parameters;

	player_parameters[0] = (Player_Parameters){
		.key_left = KEY_LEFT,
		.key_right = KEY_RIGHT,
		.key_up = KEY_UP,
		.key_down = KEY_DOWN,
		.sound_pop = LoadSound("resources/pop.wav"),
		.sound_hit = LoadSound("resources/hit3.wav"),
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
		.sound_hit = LoadSound("resources/hit4.wav"),
		.acceleration_force = 800.0f, 
		.friction = 0.5f,
		.color = (Color){0, 120, 240, 255},
	};

	game_state->sound_win = LoadSound("resources/win2.wav");


	reset_game(game_state);

	Font default_font = GetFontDefault();


	SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
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
				if (!IsWindowFullscreen()) {
					set_window_to_monitor_dimensions();
				}
				ToggleFullscreen();  // modifies window size when scaling!
			}
			else {
				reset_game(game_state);
			}
		}

		if (IsWindowResized()) {

			view = game_state->view = get_view();

			for (int player_index = 0; player_index < NUM_PLAYERS; ++player_index) {

				Player *player = game_state->players + player_index;

				float radius = radius_from_energy(player->energy); 

				Rectangle view_rectangle = {0, 0, view.width, view.height};

				if (!CheckCollisionPointRec(player->position, view_rectangle)) {

					Vector2 min_corner = Vector2SubtractValue(player->position, radius);
					Vector2 max_corner = Vector2AddValue(player->position, radius);

					if (min_corner.x < 0) {
						player->position.x = radius;
					}
					else if (max_corner.x > view.width) {
						player->position.x = view.width - radius;
					}

					if (min_corner.y < 0) {
						player->position.y = radius;
					}
					else if (max_corner.y > view.height) {
						player->position.y = view.height - radius;
					}

				}
			}
		}

		if (game_state->triumphant_player < 0 && IsWindowFocused()) {

			// Update players

			for (int player_index = 0; player_index < NUM_PLAYERS; ++player_index) {

				Player *player = game_state->players + player_index;
				
				// player_update(player, dt);
				{
					Vector2 player_direction_control = {0.0f, 0.0f};

					Player_Parameters *parameters = player->parameters;

					if (IsKeyDown(parameters->key_left)) player_direction_control.x = -1.0f;
					if (IsKeyDown(parameters->key_right)) player_direction_control.x += 1.0f;
					if (IsKeyDown(parameters->key_up)) player_direction_control.y = -1.0f;
					if (IsKeyDown(parameters->key_down)) player_direction_control.y += 1.0f;

					player_direction_control = Vector2NormalizeOrZero(player_direction_control);

					Vector2 acceleration = Vector2Scale(
						player_direction_control,
						parameters->acceleration_force * dt
					);

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
						float difference = view.width - (to_position.x + player_radius);

						if (difference < 0) {
							to_position.x = view.width - player_radius;
							
							if (hit_is_hard_enough(player->velocity.x, dt)) spawn_bullets(player, &random_state);

							player->velocity.x = (1.0f-parameters->friction)*(-player->velocity.x + difference);	
						}
					}
					
					if (player->velocity.x < 0) {
						float difference = 0 - (to_position.x - player_radius);

						if (difference > 0) {
							to_position.x = 0 + player_radius;

							if (hit_is_hard_enough(player->velocity.x, dt)) spawn_bullets(player, &random_state);

							player->velocity.x = (1.0f-parameters->friction)*(-player->velocity.x + difference);
						}
					}

					if (player->velocity.y > 0) {
						float difference = view.height - (to_position.y + player_radius);

						if (difference < 0) {
							to_position.y = view.height - player_radius;
							
							if (hit_is_hard_enough(player->velocity.y, dt)) spawn_bullets(player, &random_state);

							player->velocity.y = (1.0f-parameters->friction)*(-player->velocity.y + difference);
						}
					}
					
					if (player->velocity.y < 0) {
						float difference = 0 - (to_position.y - player_radius);

						if (difference > 0) {
							to_position.y = 0 + player_radius;
							
							if (hit_is_hard_enough(player->velocity.y, dt)) spawn_bullets(player, &random_state);
							
							player->velocity.y = (1.0f-parameters->friction)*(-player->velocity.y + difference);
						}
					}

					player->position = to_position;

					float speed = Vector2Length(player->velocity);

					player->energy += speed * dt / (player->energy*1.0f + 1.0f);

					if (player->hit_animation_t < 1.0f) {
						player->hit_animation_t += dt*4.0f;
					}
				}

				if (game_state->title_alpha > 0) {
					game_state->title_alpha -= Vector2Length(player->velocity)*0.0001f;
				}

				Player *opponent = game_state->players + (1-player_index);
				float opponent_radius = radius_from_energy(opponent->energy);

				for (int bullet_index = 0; bullet_index < player->active_bullets; ++bullet_index) {

					Bullet *bullet = &player->bullets[bullet_index];

					bullet->time += dt;
					if (bullet->time > BULLET_TIME_END_FADE) {
						player->bullets[bullet_index--] = player->bullets[--player->active_bullets];
						continue;
					}

					Vector2 bullet_position = bullet->position;

					bullet->position = Vector2Add(bullet_position, Vector2Scale(bullet->velocity, dt));

					bool bullet_overlaps_opponent = CheckCollisionCircles(bullet_position, BULLET_RADIUS, opponent->position, opponent_radius);

					if (bullet_overlaps_opponent || position_outside_playzone(bullet_position, view)) {
						player->bullets[bullet_index--] = player->bullets[--player->active_bullets];


						if (bullet_overlaps_opponent) {
							--opponent->health;

							float ring_angle = (180+90)-Vector2Angle(bullet_position, opponent->position);

							PlaySound(opponent->parameters->sound_hit);
							spawn_ring(game_state, bullet_position, player_index, ring_angle);

							opponent->hit_animation_t = 0.0f;

							if (opponent->health == 0) {
								game_state->triumphant_player = player_index;
								PlaySound(game_state->sound_win);
							}
						}
					}
				}
			}

		}
		
		// Update Rings
		for (int ring_index = 0; ring_index < game_state->ring_pool.active_rings; ) {
			
			Ring *ring = &game_state->ring_pool.rings[ring_index];

			ring->t += dt * 2.5f;

			if (ring->t > 1.0f) {
				game_state->ring_pool.rings[ring_index] = game_state->ring_pool.rings[--game_state->ring_pool.active_rings];
			}
			else {
				++ring_index;
			}
		}
		
		
		//
		//-----------------------------------------------------
		// Draw
		//-----------------------------------------------------
		//

		BeginDrawing();

		float screen_width = GetScreenWidth();
		float screen_height = GetScreenHeight();


		ClearBackground((Color){255, 255, 255, 255});

		//
		// Draw Title
		//
		if (game_state->title_alpha > 0) {

			Color title_color = (Color){192, 192, 192, game_state->title_alpha*255.0f};

			float font_size = 100.0f*view.scale;
			float font_spacing = 0.15f*font_size;

			Vector2 text_bounds = MeasureTextEx(default_font, title, font_size, font_spacing);
			Vector2 text_position = Vector2Add((Vector2){screen_width/2.0f, screen_height/3.0f}, Vector2Scale(text_bounds, -0.5f));

			DrawTextEx(default_font, title, text_position, font_size, font_spacing, title_color);

			font_size *= 0.5f;
			font_spacing *= 0.5f;

			const char *author = "By Jakob Kjær-Kammersgaard";
			text_bounds = MeasureTextEx(default_font, author, font_size, font_spacing);
			text_position = Vector2Add((Vector2){screen_width/2.0f, screen_height* 2.0f/3.0f}, Vector2Scale(text_bounds, -0.5f));

			DrawTextEx(default_font, author, text_position, font_size, font_spacing, title_color);
		}


		//
		// Draw rings
		//
		for (int ring_index = 0; ring_index < game_state->ring_pool.active_rings; ++ring_index) {
			
			Ring ring = game_state->ring_pool.rings[ring_index];

			Color ring_color = game_state->player_parameters[ring.player_index].color;

			float t1 = 1.0f - ring.t;
			float t2 = ring.t;
			t1 = 1.0f - t1*t1*t1;


			float outer_radius = t1*100.0f*view.scale;
			float inner_radius = t2*100.0f*view.scale;

			float start_angle = ring.angle - 60.0;
			float end_angle = ring.angle + 60.0f;

			ring.position = Vector2Scale(ring.position, view.scale);

			DrawRing(ring.position, inner_radius, outer_radius, start_angle, end_angle, 10, ring_color);
		}

		//
		// Draw player's bullets
		//
		for (int player_index = 0; player_index < NUM_PLAYERS; ++player_index) {

			Player *player = game_state->players + player_index;

			Player_Parameters *parameters = player->parameters; 

			for (int bullet_index = 0; bullet_index < player->active_bullets; ++bullet_index) {

				Bullet *bullet = &player->bullets[bullet_index];

				float t = 1.0f;

				if (bullet->time >= BULLET_TIME_BEGIN_FADE) {
					t = (bullet->time - BULLET_TIME_BEGIN_FADE)/(BULLET_TIME_END_FADE - BULLET_TIME_BEGIN_FADE);
					t *= t*t;
					t = 1.0f - t;
				}
				float bullet_radius = BULLET_RADIUS*t*view.scale;

				Vector2 bullet_screen_position = Vector2Scale(bullet->position, view.scale);

				float s = bullet->time < 0.5f ? bullet->time/0.5f : 1.0f;

				Vector2 direction = Vector2Scale(bullet->velocity, -0.2f*s*view.scale);

				Vector2 point_tail = Vector2Add(bullet_screen_position, direction);

				Vector2 tail_to_position_difference = Vector2Subtract(bullet_screen_position, point_tail);

				float mid_circle_radius = 0.5f*Vector2Length(tail_to_position_difference);

				Vector2 mid_point = Vector2Add(point_tail, Vector2Scale(tail_to_position_difference, 0.5f));

				Intersection_Points result = intersection_points_from_two_circles(bullet_screen_position, bullet_radius, mid_point, mid_circle_radius);

				if (result.are_intersecting) {
					Color tail_color = parameters->color;
					tail_color.a = 32;
					DrawTriangle(point_tail, result.intersection_points[0], result.intersection_points[1], tail_color);
				}

				DrawCircleV(bullet_screen_position, bullet_radius, parameters->color);

			}
		}

		//
		// Draw players
		//
		for (int player_index = 0; player_index < NUM_PLAYERS; ++player_index) {

			Player *player = game_state->players + player_index;
			Player_Parameters *parameters = player->parameters;

			float player_radius = radius_from_energy(player->energy)*view.scale;
			float font_size = player_radius*1.0f;
			float font_spacing = font_size*FONT_SPACING_FOR_SIZE;

			Vector2 player_screen_position = Vector2Scale(player->position, view.scale);

			// Draw player's body
			DrawCircleV(player_screen_position, player_radius, parameters->color);

			// Draw player's health text
			{
				const char *health_text_string = TextFormat("%i", player->health);

				float t = player->hit_animation_t;
				if (t < 1.0f) {
					float font_size_factor = 1 + 0.5f * sinf(PI*player->hit_animation_t);
					font_size *= font_size_factor;
					font_spacing = font_size*FONT_SPACING_FOR_SIZE;
				}


				Vector2 health_text_bounds = MeasureTextEx(default_font, health_text_string, font_size, font_spacing);

				Vector2 health_text_position = Vector2Add(player_screen_position, Vector2Scale(health_text_bounds, -0.5f));

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

						font_size = 30.0f*view.scale;
						font_spacing = font_size*FONT_SPACING_FOR_SIZE;

						Vector2 text_bounds = MeasureTextEx(default_font, text, font_size, font_spacing);

						Vector2 text_position = Vector2Add(player_screen_position, Vector2Scale(text_bounds, -0.5f));

						text_position.y -= player_radius + font_size;

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

			DrawRectangle(0, 0, screen_width, screen_height, (Color){255, 255, 255, 128});

			Color win_box_color = player_parameters[triumphant_player].color;
			win_box_color.a = 192;

			Vector2 screen_center = {0.5f*screen_width, 0.5*screen_height};


			const char *win_text = (const char *[]){
				"Orange Player Wins",
				"Blue Player Wins",
			}[triumphant_player];

			const char *reset_button_text = "Press [Enter] to Reset";

			float win_text_font_size = 100*view.scale;
			float win_text_font_spacing = win_text_font_size*FONT_SPACING_FOR_SIZE;
			float reset_button_text_font_size = 30*view.scale;
			float reset_button_text_font_spacing = reset_button_text_font_size*FONT_SPACING_FOR_SIZE;

			Vector2 win_text_metrics = MeasureTextEx(default_font, win_text, win_text_font_size, win_text_font_spacing);
			Vector2 reset_button_text_metrics = MeasureTextEx(default_font, reset_button_text, reset_button_text_font_size, reset_button_text_font_spacing);

			float padding = 60.0f*view.scale;

			float colored_background_height = (
				padding +
				win_text_metrics.y +
				padding +
				reset_button_text_metrics.y +
				padding
			); 

			Vector2 draw_position = {0, screen_center.y - 0.5*colored_background_height};

			// Draw colored rectangle behind win text
			DrawRectangleV(draw_position, (Vector2){screen_width, colored_background_height}, win_box_color);

			draw_position.x = screen_center.x - 0.5f*win_text_metrics.x;
			draw_position.y += padding;

			draw_text_shadowed(default_font, win_text, draw_position, win_text_font_size, win_text_font_spacing, WHITE, BLACK);

			draw_position.y += win_text_metrics.y;

			draw_position.x = screen_center.x - 0.5f*reset_button_text_metrics.x;
			draw_position.y += padding;

			draw_text_shadowed(default_font, reset_button_text, draw_position, reset_button_text_font_size, reset_button_text_font_spacing, WHITE, BLACK);
		}

		// DrawFPS(view_width-100, 10);

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


