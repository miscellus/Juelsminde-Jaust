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
#define BULLET_ENERGY_COST 4.0f
#define BULLET_RADIUS 15.0f
#define BULLET_TIME_END_FADE 7.0f
#define BULLET_TIME_BEGIN_FADE (BULLET_TIME_END_FADE-0.4f)

#define PLAYER_STARTING_HEALTH 30
#define PLAYER_MINIMUM_RADIUS 30.0f   
#define PLAYER_ACCELERATION_FORCE 1100.0f
#define PLAYER_FRICTION 0.94f

#define FONT_SPACING_FOR_SIZE 0.12f


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


typedef struct Player_Parameters
{
	KeyboardKey key_left;
	KeyboardKey key_right;
	KeyboardKey key_up;
	KeyboardKey key_down;
	Sound sound_pop;
	Sound sound_hit;
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

typedef struct Game_State {
	View view;
	Sound sound_win;
	int triumphant_player;
	float title_alpha;
#define NUM_PLAYERS 2
	Player players[NUM_PLAYERS];
	Player_Parameters player_parameters[NUM_PLAYERS];
#define MAX_ACTIVE_RINGS 128
	int active_rings;
	Ring rings[MAX_ACTIVE_RINGS];
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

void spawn_bullet_ring(Player *player, uint64_t *random_state) {

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

	if (game_state->active_rings < MAX_ACTIVE_RINGS) {

		game_state->rings[game_state->active_rings++] = (Ring){
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

	game_state->active_rings = 0;

	game_state->players[0] = (Player){
		.position = { game_state->view.width * 2.0f/3.0f, game_state->view.height / 2.0f },
		.health = PLAYER_STARTING_HEALTH,
		.hit_animation_t = 1.0f,
		.parameters = &game_state->player_parameters[0],
	};
	game_state->players[1] = (Player){
		.position = { game_state->view.width * 1.0f/3.0f, game_state->view.height / 2.0f },
		.health = PLAYER_STARTING_HEALTH,
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
	{
		CFBundleRef mainBundle = CFBundleGetMainBundle();
		CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
		char path[PATH_MAX];
		if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
		{
			// error!
		}
		CFRelease(resourcesURL);

		chdir(path);
	}
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

	const char *title = "Juelsminde Joust";

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
		.sound_pop = LoadSound("resources/player_1_pop.wav"),
		.sound_hit = LoadSound("resources/player_1_hit.wav"),
		.color = (Color){240, 120, 0, 255},
	};
	player_parameters[1] = (Player_Parameters){
		.key_left = KEY_A,
		.key_right = KEY_D,
		.key_up = KEY_W,
		.key_down = KEY_S,
		.sound_pop = LoadSound("resources/player_2_pop.wav"),
		.sound_hit = LoadSound("resources/player_2_hit.wav"),
		.color = (Color){0, 120, 240, 255},
	};

	game_state->sound_win = LoadSound("resources/win.wav");


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

			// Update player input and velocity
			for (int player_index = 0; player_index < NUM_PLAYERS; ++player_index) {

				Player *player = game_state->players + player_index;
				
				
				Vector2 player_movement_control = {0.0f, 0.0f};

				Player_Parameters *parameters = player->parameters;

				if (IsKeyDown(parameters->key_left)) player_movement_control.x = -1.0f;
				if (IsKeyDown(parameters->key_right)) player_movement_control.x += 1.0f;
				if (IsKeyDown(parameters->key_up)) player_movement_control.y = -1.0f;
				if (IsKeyDown(parameters->key_down)) player_movement_control.y += 1.0f;

				float friction_fraction = 1.0f;

				if (player_movement_control.x == 0.0f && player_movement_control.y == 0.0f) {
					friction_fraction = 0.1f;
				}
				else {
					// Normalize player movement controls
					player_movement_control = Vector2Scale(player_movement_control, INV_SQRT_TWO);
				}

				Vector2 acceleration = Vector2Scale(player_movement_control, PLAYER_ACCELERATION_FORCE * dt);

				acceleration = Vector2Subtract(acceleration, Vector2Scale(player->velocity, PLAYER_FRICTION * friction_fraction * dt));

				player->velocity = Vector2Add(player->velocity, acceleration);
			}

			// Player collision detection and response
			{
				Player *player1 = game_state->players + 0;
				Player *player2 = game_state->players + 1;

				Vector2 player1_to_position = Vector2Add(player1->position, Vector2Scale(player1->velocity, dt));
				Vector2 player2_to_position = Vector2Add(player2->position, Vector2Scale(player2->velocity, dt));

				float player1_radius = radius_from_energy(player1->energy);
				float player2_radius = radius_from_energy(player2->energy);

				float radii_sum = player2_radius + player1_radius;

				Vector2 position_difference = Vector2Subtract(player2_to_position, player1_to_position);
				float distance = Vector2Length(position_difference);

				if (distance <= radii_sum) {

					
					// Static collision
					float half_overlap = 0.5f*(distance - radii_sum);


					float inv_distance = 1.0f/distance;

					player1->position = player1_to_position = Vector2Add(player1_to_position, Vector2Scale(position_difference, half_overlap*inv_distance));
					player2->position = player2_to_position = Vector2Add(player2_to_position, Vector2Scale(position_difference, -half_overlap*inv_distance));

					// Dynamic collision
					// assert(fabs(radii_sum - Vector2Length(position_difference)) < 0.0001f);
					distance = radii_sum;//Vector2Length(position_difference);; // After the move, the distance is exactly equal to the sum of radii
					inv_distance = 1.0f/distance; // After the move, the distance is exactly equal to the sum of radii
					position_difference = Vector2Subtract(player2_to_position, player1_to_position);

					Vector2 normal = Vector2Scale(position_difference, inv_distance);
					Vector2 tangent = (Vector2){normal.y, -normal.x};

					float normal_response_1 = Vector2DotProduct(normal, player1->velocity);
					float normal_response_2 = Vector2DotProduct(normal, player2->velocity);
				
					float tangental_response_1 = Vector2DotProduct(tangent, player1->velocity);
					float tangental_response_2 = Vector2DotProduct(tangent, player2->velocity);

					float mass_1 = radius_from_energy(player1->energy);
					float mass_2 = radius_from_energy(player2->energy);

					float momentum_1 = (normal_response_1 * (mass_1 - mass_2) + 2.0f * mass_2 * normal_response_2) / (mass_1 + mass_2);
					float momentum_2 = (normal_response_2 * (mass_2 - mass_1) + 2.0f * mass_1 * normal_response_1) / (mass_1 + mass_2);

					player1->velocity = Vector2Add(
						Vector2Scale(tangent, tangental_response_1),
						Vector2Scale(normal, momentum_1)
					);
					
					player2->velocity = Vector2Add(
						Vector2Scale(tangent, tangental_response_2),
						Vector2Scale(normal, momentum_2)
					);

					if (hit_is_hard_enough(normal_response_1, dt)) {
						spawn_bullet_ring(player1, &random_state);
					}
					
					if (hit_is_hard_enough(normal_response_2, dt)) {
						spawn_bullet_ring(player2, &random_state);
					}
				}
			}

			for (int player_index = 0; player_index < NUM_PLAYERS; ++player_index) {

				Player *player = game_state->players + player_index;
				Player *opponent = game_state->players + (1-player_index);

				Vector2 target_position = player->position = Vector2Add(
					player->position,
					Vector2Scale(player->velocity, dt)
				);

				float player_radius = radius_from_energy(player->energy);
				float opponent_radius = radius_from_energy(opponent->energy);


				// Bounce on view edges
				{
					float bounce_back_factor = -0.6f;
					float edge_offset;

					float cumulative_edge_bounce = 0.0f;

					edge_offset = view.width - (target_position.x + player_radius);

					if (edge_offset < 0) {
						target_position.x = view.width - player_radius;
						
						cumulative_edge_bounce += fabs(player->velocity.x);

						player->velocity.x = bounce_back_factor*(player->velocity.x + edge_offset);	
					}
					
					edge_offset = (target_position.x - player_radius) - 0;

					if (edge_offset < 0) {
						target_position.x = 0 + player_radius;

						cumulative_edge_bounce += fabs(player->velocity.x);

						player->velocity.x = bounce_back_factor*(player->velocity.x + edge_offset);
					}

					edge_offset = view.height - (target_position.y + player_radius);

					if (edge_offset < 0) {
						target_position.y = view.height - player_radius;

						cumulative_edge_bounce += fabs(player->velocity.y);

						player->velocity.y = bounce_back_factor*(player->velocity.y + edge_offset);

					}
				
					edge_offset = (target_position.y - player_radius) - 0;

					if (edge_offset < 0) {
						target_position.y = 0 + player_radius;
						
						cumulative_edge_bounce += fabs(player->velocity.y);
						
						player->velocity.y = bounce_back_factor*(player->velocity.y + edge_offset);
					}


					if (hit_is_hard_enough(cumulative_edge_bounce, dt)) {
						spawn_bullet_ring(player, &random_state);
					}
				}

				player->position = target_position;

				float speed = Vector2Length(player->velocity);

				player->energy += speed * dt / (player->energy*1.0f + 1.0f);

				if (player->hit_animation_t < 1.0f) {
					player->hit_animation_t += dt*4.0f;
				}

				if (game_state->title_alpha > 0) {
					game_state->title_alpha -= Vector2Length(player->velocity)*0.0001f;
				}


				// Update bullets
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

						// If bullet overlaps opponent player, subtract health and remove bullet from pool
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
		for (int ring_index = 0; ring_index < game_state->active_rings; ) {
			
			Ring *ring = &game_state->rings[ring_index];

			ring->t += dt * 2.5f;

			if (ring->t > 1.0f) {
				game_state->rings[ring_index] = game_state->rings[--game_state->active_rings];
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
		for (int ring_index = 0; ring_index < game_state->active_rings; ++ring_index) {
			
			Ring ring = game_state->rings[ring_index];

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



				float s = bullet->time < 0.3f ? bullet->time/0.3f : 1.0f;

				Vector2 direction = Vector2Scale(bullet->velocity, -0.2f*s);

				Vector2 point_tail = Vector2Add(bullet->position, direction);

				Vector2 tail_to_position_difference = Vector2Subtract(bullet->position, point_tail);

				float mid_circle_radius = 0.5f*Vector2Length(tail_to_position_difference);

				Vector2 mid_point = Vector2Add(point_tail, Vector2Scale(tail_to_position_difference, 0.5f));

				float t = 1.0f;

				if (bullet->time >= BULLET_TIME_BEGIN_FADE) {
					t = (bullet->time - BULLET_TIME_BEGIN_FADE)/(BULLET_TIME_END_FADE - BULLET_TIME_BEGIN_FADE);
					t *= t*t;
					t = 1.0f - t;
				}

				Circle mid_circle = {mid_point, mid_circle_radius};

				Circle bullet_circle = {bullet->position, BULLET_RADIUS*t};

				Intersection_Points result = intersection_points_from_two_circles(bullet_circle, mid_circle);


				float bullet_scale = t*view.scale;

				if (result.are_intersecting) {
					Color tail_color = parameters->color;
					tail_color.a = 32;
					DrawTriangle(Vector2Scale(point_tail, view.scale), Vector2Scale(result.intersection_points[0], view.scale), Vector2Scale(result.intersection_points[1], view.scale), tail_color);
				}

				Vector2 bullet_screen_position = Vector2Scale(bullet->position, view.scale);
				DrawCircleV(bullet_screen_position, BULLET_RADIUS*bullet_scale, parameters->color);

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

						Color controls_color = parameters->color;

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


