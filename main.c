#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include <raylib.h>

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

// Unity-build
#include "jj_math.c"

#define FONT_SPACING_FOR_SIZE 0.12f


#define MINIMUM(a, b) ((a) < (b) ? (a) : (b))
#define MAXIMUM(a, b) ((a) > (b) ? (a) : (b))

#define UNUSED(var) ((void)(var))

typedef struct View {
	float width;
	float height;
	float scale;
	float inv_scale;
	float screen_width;
	float screen_height;
} View;

typedef struct Bullet {
	Vector2 position;
	Vector2 velocity;
	float time;
	float spin;
} Bullet;

typedef struct Virtual_Input_Button {
	bool is_down;
	bool is_pressed;
	bool is_released;
	// TODO(jakob): Make possible to detect multiple keystrokes per frame (state_transition_count) 
	// int state_transition_count;
} Virtual_Input_Button;

enum {
	VIRTUAL_BUTTON_ACTION = 0,
	VIRTUAL_BUTTON_MENU = 1,

	VIRTUAL_BUTTON_COUNT
};

typedef struct Virtual_Input_Device_State {
	Vector2 direction; // NOTE(jakob): Valid in unit circle
	Virtual_Input_Button buttons[VIRTUAL_BUTTON_COUNT];
} Virtual_Input_Device_State;

typedef struct Virtual_Input_Key_Map {
	KeyboardKey key_left;
	KeyboardKey key_right;
	KeyboardKey key_up;
	KeyboardKey key_down;
	KeyboardKey key_action;
	KeyboardKey key_menu;
} Virtual_Input_Key_Map;

typedef struct Virtual_Input_Device {
	bool use_gamepad;
	struct {
		bool available;
		int gamepad_number;
	} gamepad;

	Virtual_Input_Key_Map keys;
	Virtual_Input_Device_State state;
} Virtual_Input_Device;

typedef struct Virtual_Input {
#define MAX_ACTIVE_INPUT_DEVICES 4
#define NUM_INPUT_DEVICES 4 // @hardcode
	Virtual_Input_Device devices[MAX_ACTIVE_INPUT_DEVICES];
	Virtual_Input_Device_State input_common;
} Virtual_Input;


typedef struct Player_Parameters {
	Virtual_Input_Device *input_device;
	const char *key_text;
	Sound sound_pop;
	Sound sound_hit;
	Color color;
} Player_Parameters;


typedef struct Player {
	Vector2 position;
	Vector2 velocity;
	float angular_velocity;
	float shoot_angle;
	int health;
	float energy;
	float controls_text_timeout;
	float shoot_time_out;
	float hit_animation_t;
	float shoot_charge_t;
	float death_animation_t;
#define MAX_ACTIVE_BULLETS 2048
	int active_bullets;
	Bullet bullets[MAX_ACTIVE_BULLETS];
	Player_Parameters params;
} Player;

typedef struct Ring {
	Vector2 position;
	int player_index;
	float angle;
	float t;
} Ring;

enum Menu_Item_Type {
	MENU_ITEM_LABEL = 0,
	MENU_ITEM_ACTION,
	MENU_ITEM_BOOL,
	MENU_ITEM_INT,
	MENU_ITEM_FLOAT,
	MENU_ITEM_INT_RANGE,
	MENU_ITEM_FLOAT_RANGE,
	MENU_ITEM_MENU,
	MENU_ITEM_MENU_BACK,
};

enum Menu_Action {
	MENU_ACTION_NEW_GAME,
	MENU_ACTION_CONTINUE,
	MENU_ACTION_QUIT,

	MENU_ACTION_FULLSCREEN_TOGGLE,
};

typedef union Value_Range {
	struct {int *value, min, max;} int_range;
	struct {float *value, min, max;} float_range;
} Value_Range;

typedef struct Menu_Item {
	enum Menu_Item_Type type;
	const char *text;
	int minimum;
	int maximum;
	union {
		int *int_ref;
		float *float_ref;
		Value_Range range;
		bool *bool_ref;
		struct Menu *menu_ref;
		int int_value;
	} u;
	int (* action)(int change, char *user_data);
} Menu_Item;

typedef struct Menu {
	struct Menu *parent;
	Menu_Item *items;
	unsigned int item_count;
	unsigned int selected_index;
} Menu;

typedef struct Game_Parameters {
	int num_players;
	int starting_health;
	float minimum_radius;
	float acceleration_force;
	float friction;
	float comeback_base_factor; // TODO(jakob) spell comeback -> comeback

	float bullet_energy_cost_ring;
	float bullet_energy_cost_fan;
	float bullet_radius;
	float bullet_time_end_fade;
	float bullet_time_begin_fade;

	float slowdowns_per_second;
	float full_charges_per_second;

	float slow_motion_slowest_factor;
} Game_Parameters;

typedef struct Game_State {
	bool running;
	bool show_menu;

	Virtual_Input input;
	Game_Parameters params;

	Sound sound_win;
	int triumphant_player;
	int num_dead_players;
	
	float title_alpha;
	float time_scale;
	bool game_in_progress; // TODO(jakob): Do we need this?
	float game_play_time;
	float slow_motion_t;

#define MAX_ACTIVE_PLAYERS 4
// #define NUM_PLAYERS 3
	Player players[MAX_ACTIVE_PLAYERS];
#define MAX_ACTIVE_RINGS 128
	int active_rings;
	Ring rings[MAX_ACTIVE_RINGS];
	int color_red;
	int color_green;
	int color_blue;
} Game_State;

static Game_Parameters game_params_for_new_game = {
	.num_players = 4,

	.starting_health = 2,
	.minimum_radius = 32.0f,
	.acceleration_force = 1100.0f,
	.friction = 0.94f,

	.bullet_energy_cost_ring = 2.5f,
	.bullet_energy_cost_fan = 4.0f,
	.bullet_radius = 15.0f,
	.bullet_time_end_fade = 7.0f,
	.bullet_time_begin_fade = (7.0f-0.4f),

	.slowdowns_per_second = 1.0f,
	.full_charges_per_second = 1.5f,

	.comeback_base_factor = 1.0f, // NOTE(jakob & patrick): energy_gained = xxxx + comeback_base_factor*(1.0f - ((float)health/(float)starting_health))
	.slow_motion_slowest_factor = 0.3f,
};


float calculate_player_radius(Player *player, Game_Parameters *game_params) {
	return game_params->minimum_radius + player->energy*1.2f;
}

float calculate_player_comeback_factor(Player *player, Game_Parameters *game_params) {
	float result = game_params->comeback_base_factor*(1.0f - ((float)player->health / (float)game_params->starting_health));
	return result;
}


bool hit_is_hard_enough(float hit) {

	return 200.0f <= fabs(hit);
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


void spawn_bullet_ring_ex(Player *player, Game_Parameters *game_params, uint64_t *random_state, int count, float speed, float spin) {
	if (player->active_bullets + count > MAX_ACTIVE_BULLETS) {
		count = MAX_ACTIVE_BULLETS - player->active_bullets;
	}

	player->energy -= count * game_params->bullet_energy_cost_ring;

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
		bullet->spin = spin;
		angle += angle_quantum;
	}

	player->active_bullets += count;

	PlaySound(player->params.sound_pop);
}

void spawn_bullet_ring(Player *player, Game_Parameters *game_params, uint64_t *random_state) {

	float comeback_factor = calculate_player_comeback_factor(player, game_params);
	int count = (int)((player->energy * (0.5f + 0.5f*comeback_factor)) / game_params->bullet_energy_cost_ring);
	float speed = 50.0f + Vector2LengthSqr(player->velocity)/1565.0f;
	float spin = 0.3f*player->angular_velocity;
	spawn_bullet_ring_ex(player, game_params, random_state, count, speed, spin);
}

void spawn_bullet_fan(Player *player, Game_Parameters *game_params, int count, float speed, float angle_span) {

	if (player->active_bullets + count > MAX_ACTIVE_BULLETS) {
		count = MAX_ACTIVE_BULLETS - player->active_bullets;
	}

	player->energy -= count * game_params->bullet_energy_cost_fan;

	if (player->energy < 0) {
		player->energy = 0.0f;
	}

	float angle_quantum = angle_span / (float)count;

	float angle = player->shoot_angle - 0.5f*angle_span + 0.5f*angle_quantum;

	Vector2 quater_player_velocity = Vector2Scale(player->velocity, 0.25f);

	for (int i = 0; i < count; ++i) {
		Bullet *bullet = &player->bullets[player->active_bullets + i];
		bullet->position = player->position;
		bullet->velocity = Vector2Scale((Vector2){cosf(angle), sinf(angle)}, speed);
		bullet->velocity = Vector2Add(bullet->velocity, quater_player_velocity);
		bullet->time = 0;
		bullet->spin = 0.3f*player->angular_velocity;
		angle += angle_quantum;
	}

	player->active_bullets += count;

	PlaySound(player->params.sound_pop);
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
	DrawTextEx(font, text, Vector2Add(position, (Vector2){3, 3}), font_size, font_spacing, background_color);
	DrawTextEx(font, text, position, font_size, font_spacing, front_color);
}

static bool is_game_over(Game_State *game_state) {
	return game_state->num_dead_players >= game_state->params.num_players - 1;
}

void reset_game(Game_State *game_state, View view) {

	// Init game parameters
	game_state->params = game_params_for_new_game;

	game_state->show_menu = false;
	game_state->triumphant_player = -1;
	game_state->time_scale = 1.0f;
	game_state->num_dead_players = 0;
	game_state->title_alpha = 1.0f;
	game_state->active_rings = 0;
	game_state->game_play_time = 0.0f;
	game_state->game_in_progress = true;

	Game_Parameters *game_params = &game_state->params;

	int column_count = game_params->num_players;
	float column_width = view.width / (float)column_count;

	for (int player_index = 0; player_index < game_params->num_players; ++player_index) {
		Player *player = &game_state->players[player_index];

		Vector2 screen_center = (Vector2){0.5f*view.width, 0.5f*view.height};

		Vector2 aim_dir = Vector2Subtract(screen_center, player->position);
		aim_dir = Vector2NormalizeOrZero(aim_dir);

		memset(player, 0, (char *)&player->params - (char *)player);
		player->position = (Vector2){ column_width*(player_index + 0.5f), view.height / 2.0f };
		player->shoot_angle = atan2f(aim_dir.y, aim_dir.x);
		player->health = game_params->starting_health;
		player->hit_animation_t = 1.0f;

	}
}

void set_window_to_monitor_dimensions(void) {
	int monitor_index = GetCurrentMonitor();
	int monitor_width = GetMonitorWidth(monitor_index);
	int monitor_height = GetMonitorHeight(monitor_index);
	// fprintf(stderr, "Monitor (%d): %d x %d\n", monitor_index, monitor_width, monitor_height);
	SetWindowSize(monitor_width, monitor_height);
}

View get_updated_view(void) {

	View result;

	float width;
	float height;

	if (IsWindowFullscreen()) {
		int monitor = GetCurrentMonitor();
		width = (float)GetMonitorWidth(monitor);
		height = (float)GetMonitorHeight(monitor);
	}
	else {
		width = GetScreenWidth();
		height = GetScreenHeight();
	}

	result.scale = 0.5f*(width/1440.0f + height/900.0f);
	result.inv_scale = 1.0f/result.scale;
	result.width = width*result.inv_scale;
	result.height = height*result.inv_scale;
	result.screen_width = width;
	result.screen_height = height;
	return result;
}

static void calculate_bullet_count_and_angle_span(Player *player, Game_Parameters *game_params, int *count_out, float *angle_span_out) {
	float narrow_angle_span = 2.0*PI / 60.0f;
	float wide_angle_span = 2.0*PI / 4.0f;
	float t = player->shoot_charge_t;
	float comeback_factor = calculate_player_comeback_factor(player, game_params);
	*count_out = (player->energy * (0.25f + 0.75f*comeback_factor)) / game_params->bullet_energy_cost_fan;
	*angle_span_out = Lerp(wide_angle_span, narrow_angle_span, t);
}

float shortest_angle_difference(float a, float b) {
	// NOTE(jakob & patrick): This assumes normalized angles between 0 and 2*PI
	// assert(a >= 0.0f);
	// assert(b >= 0.0f);
	// assert(a <= 2*PI + 0.00001f);
	// assert(b <= 2*PI + 0.00001f);

	float angle_difference = b - a;

	if (angle_difference > PI) {
		angle_difference -= 2*PI;
	} else if (angle_difference < -PI) {
		angle_difference += 2*PI;
	}

	return angle_difference;
}

float lerp_angle(float a, float b, float t) {
	float difference = shortest_angle_difference(a, b);
	float result = a + t*difference;

	if (result >= 2*PI) {
		result -= 2*PI;
	}
	else if (result < 0.0f) {
		result += 2*PI;
	}
	return result;
}

int menu_action_toggle_fullscreen(int change, char *user_data) {
	UNUSED(change);
	UNUSED(user_data);
	ToggleFullscreen();
	return 0;
}

float _lerp_angle(float a, float b, float t) {
	// From https://gist.github.com/itsmrpeck/be41d72e9d4c72d2236de687f6f53974

	float result;

	float angle_difference = b - a;

	if (angle_difference < -PI)
	{
		// lerp upwards past 2*PI
		b += 2*PI;
		result = Lerp(a, b, t);
		if (result >= 2*PI)
		{
			result -= 2*PI;
		}
	}
	else if (angle_difference > PI)
	{
		// lerp downwards past 0
		b -= 2*PI;
		result = Lerp(a, b, t);
		if (result < 0.f)
		{
			result += 2*PI;
		}
	}
	else
	{
		// straight lerp
		result = Lerp(a, b, t);
	}

	return result;
}

static const Virtual_Input_Key_Map global_key_maps[] = {
	{
		KEY_LEFT,
		KEY_RIGHT,
		KEY_UP,
		KEY_DOWN,
		KEY_RIGHT_CONTROL,
		KEY_ESCAPE,
	},
	{
		KEY_A,
		KEY_D,
		KEY_W,
		KEY_S,
		KEY_LEFT_CONTROL,
		KEY_ESCAPE,
	},
	{
		KEY_J,
		KEY_L,
		KEY_I,
		KEY_K,
		KEY_U,
		KEY_ESCAPE,
	},
	{
		KEY_KP_4,
		KEY_KP_6,
		KEY_KP_8,
		KEY_KP_5,
		KEY_KP_7,
		KEY_ESCAPE,
	},
};

static void virtual_input_init(Virtual_Input *input) {
	for (int device_index = 0; device_index < NUM_INPUT_DEVICES; ++device_index) {
		Virtual_Input_Device *dev = &input->devices[device_index];

		dev->gamepad.gamepad_number = device_index;
		dev->gamepad.available = IsGamepadAvailable(device_index);
		dev->use_gamepad = dev->gamepad.available;
		dev->keys = global_key_maps[device_index];
		dev->state = (Virtual_Input_Device_State){0};
	}

	input->input_common = (Virtual_Input_Device_State){0};
}


static void virtual_input_update(Virtual_Input *input) {

	Virtual_Input_Device_State *common = &input->input_common;
	*common = (Virtual_Input_Device_State){0};

	for (int device_index = 0; device_index < NUM_INPUT_DEVICES; ++device_index) {

		Virtual_Input_Device *dev = &input->devices[device_index];

		if (dev->use_gamepad) {
			dev->gamepad.available = IsGamepadAvailable(device_index);

			// TODO(jakob)
			int gamepad_number = dev->gamepad.gamepad_number;

			Vector2 direction = {0.0f, 0.0f};

			direction.x = GetGamepadAxisMovement(gamepad_number, GAMEPAD_AXIS_LEFT_X);
            direction.y = GetGamepadAxisMovement(gamepad_number, GAMEPAD_AXIS_LEFT_Y);

			dev->state.direction = direction;

			dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_down = IsGamepadButtonDown(gamepad_number, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
			dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_pressed = IsGamepadButtonPressed(gamepad_number, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
			dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_released = IsGamepadButtonReleased(gamepad_number, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);

			dev->state.buttons[VIRTUAL_BUTTON_MENU].is_down = IsGamepadButtonDown(gamepad_number, GAMEPAD_BUTTON_MIDDLE_RIGHT);
			dev->state.buttons[VIRTUAL_BUTTON_MENU].is_pressed = IsGamepadButtonPressed(gamepad_number, GAMEPAD_BUTTON_MIDDLE_RIGHT);
			dev->state.buttons[VIRTUAL_BUTTON_MENU].is_released = IsGamepadButtonReleased(gamepad_number, GAMEPAD_BUTTON_MIDDLE_RIGHT);

			Vector2 direction_keys = (Vector2){0.0f, 0.0f};
			if (IsGamepadButtonDown(gamepad_number, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) direction_keys.x -= 1.0f;
			if (IsGamepadButtonDown(gamepad_number, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) direction_keys.x += 1.0f;
			if (IsGamepadButtonDown(gamepad_number, GAMEPAD_BUTTON_LEFT_FACE_UP)) direction_keys.y -= 1.0f;
			if (IsGamepadButtonDown(gamepad_number, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) direction_keys.y += 1.0f;
			common->direction = Vector2Add(common->direction, direction_keys);	
		}
		else {
			
			Vector2 direction = {0.0f, 0.0f};

			if (IsKeyDown(dev->keys.key_left))  direction.x  = -1.0f;
			if (IsKeyDown(dev->keys.key_right)) direction.x +=  1.0f;
			if (IsKeyDown(dev->keys.key_up))    direction.y  = -1.0f;
			if (IsKeyDown(dev->keys.key_down))  direction.y +=  1.0f;

			dev->state.direction = direction;
			dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_down = IsKeyDown(dev->keys.key_action);
			dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_pressed = IsKeyPressed(dev->keys.key_action);
			dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_released = IsKeyReleased(dev->keys.key_action);

			dev->state.buttons[VIRTUAL_BUTTON_MENU].is_down = IsKeyDown(dev->keys.key_menu);
			dev->state.buttons[VIRTUAL_BUTTON_MENU].is_pressed = IsKeyPressed(dev->keys.key_menu);
			dev->state.buttons[VIRTUAL_BUTTON_MENU].is_released = IsKeyReleased(dev->keys.key_menu);
		}

		common->direction = Vector2Add(common->direction, dev->state.direction);
		common->buttons[VIRTUAL_BUTTON_ACTION].is_down |= dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_down;
		common->buttons[VIRTUAL_BUTTON_ACTION].is_pressed |= dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_pressed;
		common->buttons[VIRTUAL_BUTTON_ACTION].is_released |= dev->state.buttons[VIRTUAL_BUTTON_ACTION].is_released;
		common->buttons[VIRTUAL_BUTTON_MENU].is_down |= dev->state.buttons[VIRTUAL_BUTTON_MENU].is_down;
		common->buttons[VIRTUAL_BUTTON_MENU].is_pressed |= dev->state.buttons[VIRTUAL_BUTTON_MENU].is_pressed;
		common->buttons[VIRTUAL_BUTTON_MENU].is_released |= dev->state.buttons[VIRTUAL_BUTTON_MENU].is_released;
	}

	Vector2 direction_keys = (Vector2){0.0f, 0.0f};
	if (IsKeyDown(KEY_LEFT)) direction_keys.x -= 1.0f;
	if (IsKeyDown(KEY_RIGHT)) direction_keys.x += 1.0f;
	if (IsKeyDown(KEY_UP)) direction_keys.y -= 1.0f;
	if (IsKeyDown(KEY_DOWN)) direction_keys.y += 1.0f;
	common->direction = Vector2Add(common->direction, direction_keys);

	common->direction = Vector2NormalizeOrZero(input->input_common.direction);

	common->buttons[VIRTUAL_BUTTON_ACTION].is_down |= IsKeyDown(KEY_ENTER);
	common->buttons[VIRTUAL_BUTTON_ACTION].is_pressed |= IsKeyPressed(KEY_ENTER);
	common->buttons[VIRTUAL_BUTTON_ACTION].is_released |= IsKeyReleased(KEY_ENTER);
	common->buttons[VIRTUAL_BUTTON_MENU].is_down |= IsKeyDown(KEY_ESCAPE);
	common->buttons[VIRTUAL_BUTTON_MENU].is_pressed |= IsKeyPressed(KEY_ESCAPE);
	common->buttons[VIRTUAL_BUTTON_MENU].is_released |= IsKeyReleased(KEY_ESCAPE);
}

static void read_file_into_c_string(const char *filepath, char **contents_out, size_t *file_size_out) {
	FILE *file = fopen(filepath, "rb");

	fseek(file, 0, SEEK_END);
	*file_size_out = ftell(file);
	fseek(file, 0, SEEK_SET);

	*contents_out = calloc(1, *file_size_out + 1);

	fread(*contents_out, *file_size_out, 1, file);

	fclose(file);
}

extern int glfwUpdateGamepadMappings(const char *string);
extern int glfwGetError(const char **description);

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


#if 0
	{
		int major, minor, rev;
		glfwGetVersion(&major, &minor, &rev);

		fprintf(stderr, "GLFW Version: %d.%d.%d\n", major, minor, rev);
		exit(0);
	}
#endif

	uint64_t random_state = time(0);

	{
		long t = time(NULL);
		random_state ^= (t*13) ^ (t>>4);
	}

	// Initialization
	//---------------------------------------------------------

	InitAudioDevice();
	assert(IsAudioDeviceReady());

	const char *title = "Juelsminde Joust";

	// Set configuration flags for window creation
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);



	// InitWindow(2560, 1440, title);
	InitWindow(1024, 768, title);
	HideCursor();
	ToggleFullscreen();

    SetTargetFPS(60);

	// NOTE(jakob): Zeroed memory as part of initialization
	Game_State *game_state = calloc(1, sizeof(*game_state));
	assert(game_state);

	Virtual_Input *input = &game_state->input;

	virtual_input_init(input);

	// Init player parameters
	{
		Player_Parameters *params;

		params = &game_state->players[0].params;

		*params = (Player_Parameters){0};
		params->input_device = &input->devices[0];
		params->key_text = "Arrow Keys + Right CTRL";
		params->sound_pop = LoadSound("resources/player_1_pop.wav");
		params->sound_hit = LoadSound("resources/player_1_hit.wav");
		params->color = (Color){240.0f, 120.0f, 0.0f, 255.0f};

		params = &game_state->players[1].params;

		*params = (Player_Parameters){0};
		params->input_device = &input->devices[1];
		params->key_text = "W,A,S,D + Left CTRL";
		params->sound_pop = LoadSound("resources/player_2_pop.wav");
		params->sound_hit = LoadSound("resources/player_2_hit.wav");
		params->color = (Color){0.0f, 120.0f, 240.0f, 255.0f};

		params = &game_state->players[2].params;

		*params = (Player_Parameters){0};
		params->input_device = &input->devices[2];
		params->key_text = "I,J,K,L + U";
		params->sound_pop = LoadSound("resources/player_2_pop.wav");
		params->sound_hit = LoadSound("resources/player_2_hit.wav");
		params->color = (Color){40.0f, 220.0f, 80.0f, 255.0f};

		params = &game_state->players[3].params;

		*params = (Player_Parameters){0};
		params->input_device = &input->devices[3];
		params->key_text = "Num 8,4,5,6 + Num 7";
		params->sound_pop = LoadSound("resources/player_1_pop.wav");
		params->sound_hit = LoadSound("resources/player_1_hit.wav");
		params->color = (Color){180.0f, 50.0f, 220.0f, 255.0f};
	}
	game_state->sound_win = LoadSound("resources/win.wav");

	game_state->color_red = 255;
	game_state->color_green = 255;
	game_state->color_blue = 255;

	game_state->time_scale = 1.0f;

	Menu main_menu = {0};
	Menu settings_menu = {0};

	main_menu.item_count = 4;
	main_menu.items = (Menu_Item []) {
		{MENU_ITEM_ACTION, "Continue", .u.int_value=MENU_ACTION_CONTINUE},
		{MENU_ITEM_ACTION, "New Game", .u.int_value=MENU_ACTION_NEW_GAME},
		{MENU_ITEM_MENU, "Settings", .u.menu_ref = &settings_menu},
		{MENU_ITEM_ACTION, "Quit", .u.int_value=MENU_ACTION_QUIT},
	};

	Menu gameplay_settings_menu = {0};
	Menu video_settings_menu = {0};
	Menu controls_menu = {0};

	settings_menu.item_count = 4;
	settings_menu.items = (Menu_Item []){
		{MENU_ITEM_MENU_BACK, "Back", .u = {0}},
		{MENU_ITEM_MENU, "Gameplay Settings", .u.menu_ref = &gameplay_settings_menu},
		{MENU_ITEM_MENU, "Video Settings", .u.menu_ref = &video_settings_menu},
		{MENU_ITEM_MENU, "Controls", .u.menu_ref = &controls_menu},
	};

	controls_menu.item_count = 5;
	controls_menu.items = (Menu_Item []){
		{MENU_ITEM_MENU_BACK, "Back", .u = {0}},
		{MENU_ITEM_BOOL, "Use Gamepad for Orange Player", .u.bool_ref = &game_state->players[0].params.input_device->use_gamepad},
		{MENU_ITEM_BOOL, "Use Gamepad for Blue Player", .u.bool_ref = &game_state->players[1].params.input_device->use_gamepad},
		{MENU_ITEM_BOOL, "Use Gamepad for Green Player", .u.bool_ref = &game_state->players[2].params.input_device->use_gamepad},
		{MENU_ITEM_BOOL, "Use Gamepad for Purple Player", .u.bool_ref = &game_state->players[3].params.input_device->use_gamepad},
	};

	gameplay_settings_menu.item_count = 4;
	gameplay_settings_menu.items = (Menu_Item []){
		{MENU_ITEM_MENU_BACK, "Back", .u = {0}},
		{MENU_ITEM_INT_RANGE, "Number of Players", .u.range.int_range = {
			.value = &game_params_for_new_game.num_players,
			.min = 2,
			.max = 4,
		}},
		{MENU_ITEM_INT_RANGE, "Starting Player Health", .u.range.int_range = {
			.value = &game_params_for_new_game.starting_health,
			.min = 1,
			.max = 999,
		}},
		{MENU_ITEM_FLOAT, "Time Scale", .u.float_ref = &game_state->time_scale},
	};

	video_settings_menu.item_count = 5;
	video_settings_menu.items = (Menu_Item []){
		{MENU_ITEM_MENU_BACK, "Back", .u = {0}},
		{MENU_ITEM_INT_RANGE, "Background (Red)", .u.range.int_range = {
			.value = &game_state->color_red,
			.min = 0,
			.max = 255,
		}},
		{MENU_ITEM_INT_RANGE, "Background (Green)", .u.range.int_range = {
			.value = &game_state->color_green,
			.min = 0,
			.max = 255,
		}},
		{MENU_ITEM_INT_RANGE, "Background (Blue)", .u.range.int_range = {
			.value = &game_state->color_blue,
			.min = 0,
			.max = 255,
		}},
		{MENU_ITEM_ACTION, "Full Screen", .action = menu_action_toggle_fullscreen, .u.int_value = MENU_ACTION_FULLSCREEN_TOGGLE},
	};

	Menu *menu = &main_menu;
	float menu_item_cooldown = 0.0f;
	float menu_action_time_start = 0.0f;

	View view = get_updated_view();

	reset_game(game_state, view);
	game_state->show_menu = false;

	Font default_font = GetFontDefault();

	SetExitKey(0);

	SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
	//----------------------------------------------------------


#if 1
	{
		char *custom_sdl_gamepad_mappings;
		size_t contents_length;
		read_file_into_c_string("resources/gamecontrollerdb.txt", &custom_sdl_gamepad_mappings, &contents_length);
		// printf("%.*s\n", (int)contents_length, custom_sdl_gamepad_mappings);
		// SetGamepadMappings("03000000c62400002a54000001010000,PowerA Xbox One Spectra Infinity,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Linux,");
		// SetGamepadMappings("030000005e040000a002000000010000,Generic X-Box pad Piranha,a:b0,b:b1,back:b6,dpdown:h0.1,dpleft:h0.2,dpright:h0.8,dpup:h0.4,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Linux,");

		int success = glfwUpdateGamepadMappings(custom_sdl_gamepad_mappings);
		if (!success) {
			const char *error_str = NULL;
			int code = glfwGetError(&error_str);
			fprintf(stderr, "GLFW_ERROR (%d): %s\n", code, error_str);
			exit(-1);
		}
	}
#endif

	game_state->running = true;

	Game_Parameters *game_params = &game_state->params;

	// Main game loop
	while (game_state->running) {   // Detect window close button or ESC key
		float dt = game_state->time_scale * GetFrameTime();

		bool window_resized = IsWindowResized();

		if (window_resized) {
			view = get_updated_view();
		}

		virtual_input_update(input);

		// Update
		//-----------------------------------------------------

		bool toggle_fullscreen = IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT));

		if (toggle_fullscreen) {
			ToggleFullscreen();  // modifies window size when scaling!
		}

		if (is_game_over(game_state) && !game_state->show_menu && input->input_common.buttons[VIRTUAL_BUTTON_MENU].is_pressed) {
			reset_game(game_state, view);
			input->input_common.buttons[VIRTUAL_BUTTON_MENU].is_pressed = false;
		}
		else if (input->input_common.buttons[VIRTUAL_BUTTON_MENU].is_pressed) {
			if (game_state->show_menu) {
				Menu *parent = menu->parent;
				if (parent) {
					menu = menu->parent;
				}
				else {
					game_state->show_menu = false;
				}
			}
			else {
				game_state->show_menu = true;
				menu = &main_menu;
			}
		}

		if (WindowShouldClose()) {
			game_state->running = false;
		}

		if (window_resized) {

			for (int player_index = 0; player_index < game_params->num_players; ++player_index) {

				Player *player = game_state->players + player_index;
				if (player->health <= 0) continue;

				float radius = calculate_player_radius(player, game_params);

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

		if (!game_state->show_menu && IsWindowFocused()) {
			game_state->game_play_time += dt;

			// Slow motion
			if (false && game_state->slow_motion_t < 1.0f) {
				game_state->slow_motion_t += game_params->slowdowns_per_second*dt;
			
				float t = game_state->slow_motion_t;
				float slow_motion_factor;

				float t1 = 0.1f;
				float t2 = 0.6f;
				float t3 = 0.9f;

				if (t < t1) {
					t = t/t1;
					slow_motion_factor = Lerp(1.0f, 0.01f, t);
				}
				else if (t < t2) {
					t = (t-t1)/(t2-t1);
					slow_motion_factor = Lerp(0.01f, 0.4f, t);
				}
				else if (t < t3) {
					t = (t-t2)/(t3-t2);
					slow_motion_factor = Lerp(0.4f, 0.6f, t);
				}
				else {
					t = (t-t3)/(1.0f - t3);
					slow_motion_factor = Lerp(0.6f, 1.0f, t);
				}

				// Apply slow motion
				dt *= slow_motion_factor;
			}

			// Update player input and velocity
			for (int player_index = 0; player_index < game_params->num_players; ++player_index) {

				Player *player = &game_state->players[player_index];
				if (player->health <= 0) continue;


				Virtual_Input_Device_State input = player->params.input_device->state;
				Vector2 control = input.direction;

				float friction_fraction = 1.0f;

				if (control.x == 0.0f && control.y == 0.0f) {
					friction_fraction = 0.1f;
				}
				else {

					float control_angle = atan2f(control.y, control.x);

					// Normalize player movement controls
					control = Vector2NormalizeOrZero(control);

					float old_angle = player->shoot_angle;
					player->shoot_angle = lerp_angle(player->shoot_angle, control_angle, 3.0f * dt);

					float angular_pulse = 10.0f*shortest_angle_difference(old_angle, player->shoot_angle);

					player->angular_velocity = 0.5f*player->angular_velocity + 0.5f*angular_pulse;

				}

				Vector2 acceleration = Vector2Scale(control, game_params->acceleration_force * dt);

				acceleration = Vector2Subtract(acceleration, Vector2Scale(player->velocity, game_params->friction * friction_fraction * dt));

				//
				// Shooting:
				//
				if (player->shoot_time_out < game_state->game_play_time) {

					if (input.buttons[VIRTUAL_BUTTON_ACTION].is_down && player->shoot_charge_t < 1.0f) {
						float full_charges_per_second = game_params->full_charges_per_second;

						player->shoot_charge_t += full_charges_per_second * dt;
						if (player->shoot_charge_t > 1.0f) {
							player->shoot_charge_t = 1.0f;
						}
					}
					else if (input.buttons[VIRTUAL_BUTTON_ACTION].is_released) {
						float shoot_cooldown_seconds = 1.0f;
						player->shoot_time_out = game_state->game_play_time + shoot_cooldown_seconds;

						float comeback_factor = calculate_player_comeback_factor(player, game_params);
						float speed = 50.0f + (400.0f + comeback_factor*650.0f)*player->shoot_charge_t;

						Vector2 shoot_vector = (Vector2){cosf(player->shoot_angle), sinf(player->shoot_angle)};

						float recoil_factor = Vector2DotProduct(shoot_vector, Vector2NormalizeOrZero(player->velocity));

						if (recoil_factor < 0) {
							recoil_factor = 0;
						}

						float angle_span;
						int bullet_count;
						calculate_bullet_count_and_angle_span(player, game_params, &bullet_count, &angle_span);

						spawn_bullet_fan(player, game_params, bullet_count, speed, angle_span);

						acceleration = Vector2Subtract(acceleration, Vector2Scale(shoot_vector, speed*recoil_factor));

						player->shoot_charge_t = 0.0f;
					}
				}

				player->velocity = Vector2Add(player->velocity, acceleration);
			}

			// Player collision detection and response
			//
			for (int collision_iteration = 0; collision_iteration < 8; ++collision_iteration) {

				float cumulative_collision_overlap = 0;

				for (int player_1_index = 0; player_1_index < game_params->num_players; ++player_1_index) {
					for (int player_2_index = player_1_index + 1; player_2_index < game_params->num_players; ++player_2_index) {

						Player *player1 = game_state->players + player_1_index;
						Player *player2 = game_state->players + player_2_index;
						if (player1->health <= 0) continue;
						if (player2->health <= 0) continue;

						Vector2 player1_to_position = Vector2Add(player1->position, Vector2Scale(player1->velocity, dt));
						Vector2 player2_to_position = Vector2Add(player2->position, Vector2Scale(player2->velocity, dt));

						float player1_radius = calculate_player_radius(player1, game_params);
						float player2_radius = calculate_player_radius(player2, game_params);

						float radii_sum = player2_radius + player1_radius;

						Vector2 position_difference = Vector2Subtract(player2_to_position, player1_to_position);
						float distance = Vector2Length(position_difference);

						if (distance <= radii_sum) {


							// Static collision
							float half_overlap = 0.5f*(distance - radii_sum);

							float inv_distance = 1.0f/distance;
							
							cumulative_collision_overlap += 2.0f*half_overlap*inv_distance;

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

							float mass_1 = calculate_player_radius(player1, game_params);
							float mass_2 = calculate_player_radius(player2, game_params);

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

							if (hit_is_hard_enough(normal_response_1)) {
								spawn_bullet_ring(player1, game_params, &random_state);
							}

							if (hit_is_hard_enough(normal_response_2)) {
								spawn_bullet_ring(player2, game_params, &random_state);
							}
						}
					}
				}

				if (cumulative_collision_overlap < 0.0001f) {
					// printf("cumulative_collision_overlap: %.4f\n", cumulative_collision_overlap);
					break;
				}
			}

			for (int player_index = 0; player_index < game_params->num_players; ++player_index) {

				Player *player = game_state->players + player_index;
				if (player->health <= 0) continue;

				float player_radius = calculate_player_radius(player, game_params);

				Vector2 target_position = player->position = Vector2Add(
					player->position,
					Vector2Scale(player->velocity, dt)
				);

				// Bounce on view edges
				{
					float bounce_back_factor = -0.6f;
					float edge_offset;

					float cumulative_edge_bounce = 0.0f;

					if (player->velocity.x > 0) {
						edge_offset = view.width - (target_position.x + player_radius);

						if (edge_offset < 0) {
							target_position.x = view.width - player_radius;

							cumulative_edge_bounce += fabs(player->velocity.x);

							player->velocity.x = bounce_back_factor*(player->velocity.x + edge_offset);
						}
					}
					else {
						edge_offset = (target_position.x - player_radius) - 0;

						if (edge_offset < 0) {
							target_position.x = 0 + player_radius;

							cumulative_edge_bounce += fabs(player->velocity.x);

							player->velocity.x = bounce_back_factor*(player->velocity.x + edge_offset);
						}
					}

					if (player->velocity.y > 0) {

						edge_offset = view.height - (target_position.y + player_radius);

						if (edge_offset < 0) {
							target_position.y = view.height - player_radius;

							cumulative_edge_bounce += fabs(player->velocity.y);

							player->velocity.y = bounce_back_factor*(player->velocity.y + edge_offset);

						}
					}
					else {
						edge_offset = (target_position.y - player_radius) - 0;

						if (edge_offset < 0) {
							target_position.y = 0 + player_radius;

							cumulative_edge_bounce += fabs(player->velocity.y);

							player->velocity.y = bounce_back_factor*(player->velocity.y + edge_offset);
						}
					}

					if (hit_is_hard_enough(cumulative_edge_bounce)) {
						spawn_bullet_ring(player, game_params, &random_state);
					}
				}

				player->position = target_position;

				float speed = Vector2Length(player->velocity);

				float comeback_energy = calculate_player_comeback_factor(player, game_params);
				player->energy += dt * (speed * (1 + comeback_energy)) / (player->energy*2.0f + 1.0f);

				if (player->hit_animation_t < 1.0f) {
					player->hit_animation_t += dt*4.0f;
				}

				if (game_state->title_alpha > 0) {
					game_state->title_alpha -= Vector2Length(player->velocity)*0.0001f;
				}
			}

			// Update bullets
			for (int player_index = 0; player_index < game_params->num_players; ++player_index) {
				Player *player = game_state->players + player_index;
				for (int bullet_index = 0; bullet_index < player->active_bullets; ++bullet_index) {

					Bullet *bullet = &player->bullets[bullet_index];

					bullet->time += dt;
					if (bullet->time > game_params->bullet_time_end_fade) {
						player->bullets[bullet_index--] = player->bullets[--player->active_bullets];
						continue;
					}

					Vector2 bullet_position = bullet->position;

					bullet->position = Vector2Add(bullet_position, Vector2Scale(bullet->velocity, dt));

					if (position_outside_playzone(bullet->position, view)) {
						player->bullets[bullet_index--] = player->bullets[--player->active_bullets];
						continue;
					}

					// This enables spin moves
					bullet->velocity = Vector2Rotate(bullet->velocity, dt*bullet->spin);

					float bullet_radius = game_params->bullet_radius;
					bool destroy_bullet = false;

					for (int opponent_index = 0; opponent_index < game_params->num_players; ++opponent_index) {
						if (opponent_index == player_index) continue;

						Player *opponent = game_state->players + opponent_index;
						if (opponent->health <= 0) continue;

						float opponent_radius = calculate_player_radius(opponent, game_params);
						bool bullet_overlaps_opponent = CheckCollisionCircles(bullet_position, bullet_radius, opponent->position, opponent_radius);

						if (bullet_overlaps_opponent) {
							destroy_bullet = true;
							// If bullet overlaps opponent player, subtract health and (defer) remove bullet from pool

							--opponent->health;

							Vector2 diff = Vector2Subtract(bullet_position, opponent->position);

							diff = Vector2NormalizeOrZero(diff);

							float bullet_mass = 0.125f;
							float bullet_speed = Vector2Length(bullet->velocity);

							opponent->velocity = Vector2Subtract(opponent->velocity, Vector2Scale(diff, bullet_mass*bullet_speed));

							float ring_angle = atan2f(diff.x, diff.y)*(180.0f/PI);

							PlaySound(opponent->params.sound_hit);
							spawn_ring(game_state, bullet_position, player_index, ring_angle);

							opponent->hit_animation_t = 0.0f;

							if (opponent->health <= 0) {

								float bullet_speed = 8 + (2*(opponent->energy/game_params->bullet_energy_cost_ring));

								spawn_bullet_ring_ex(
									opponent,
									game_params,
									&random_state,
									bullet_speed,
									200.0f + 10.0f*opponent->energy,
									bullet_speed * 0.025f
								);

								opponent->death_animation_t = 0.0f;

								// Game ends

								if (game_state->triumphant_player == -1) {
									++game_state->num_dead_players;

									if (is_game_over(game_state)) {

										int triumphant_player = 0;

										while (triumphant_player < game_params->num_players) {
											if (game_state->players[triumphant_player].health > 0) break;
											++triumphant_player;
										}

										game_state->triumphant_player = triumphant_player;
										game_state->game_in_progress = false;
										game_state->time_scale = 0.25f;
										PlaySound(game_state->sound_win);
									}
									else {
										// Start dramatic slow motion
										game_state->slow_motion_t = 0.0f;
									}
								}

							}
						}
					}

					if (destroy_bullet) {
						player->bullets[bullet_index--] = player->bullets[--player->active_bullets];
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
		}
		else if (game_state->show_menu && !toggle_fullscreen) {

			int item_change_y = (int)input->input_common.direction.y;
			int item_change_x = (int)input->input_common.direction.x;
			int item_enter = (int)input->input_common.buttons[VIRTUAL_BUTTON_ACTION].is_down;

			// if (IsKeyDown(KEY_UP)) {
			// 	item_change_y = -1;
			// }

			// if (IsKeyDown(KEY_DOWN)) {
			// 	++item_change_y;
			// }

			// if (IsKeyDown(KEY_LEFT)) {
			// 	item_change_x = -1;
			// }

			// if (IsKeyDown(KEY_RIGHT)) {
			// 	++item_change_x;
			// }


			double current_time = GetTime();

			if (!item_change_y && !item_change_x && !item_enter) {
				menu_item_cooldown = 0.0;
			}
			else {
				if (menu_item_cooldown < current_time) {
					if (item_change_y) {
						menu->selected_index += menu->item_count + item_change_y;
						menu->selected_index %= menu->item_count;
					}
					menu_item_cooldown = current_time + 0.2f;
				}
				else {
					item_change_y = 0;
					item_change_x = 0;
					item_enter = 0;
				}
			}

			Menu_Item *item = &menu->items[menu->selected_index];

			switch (item->type) {
				case MENU_ITEM_INT:
					if (item_change_x) {
						*item->u.int_ref += item_change_x;
						if (*item->u.int_ref < 0) {
							*item->u.int_ref = 0;
						}
					}
					break;
				case MENU_ITEM_INT_RANGE:
					if (item_change_x) {
						int *value = item->u.range.int_range.value;
						int min = item->u.range.int_range.min;
						int max = item->u.range.int_range.max;
						*value += item_change_x;
						if (*value < min) {
							*value = max;
						}
						else if (*value > max) {
							*value = min;
						}
					}
					break;
				case MENU_ITEM_FLOAT:
					if (item_change_x) {
						*item->u.float_ref += item_change_x * 0.1f;
						if (*item->u.float_ref < 0) {
							*item->u.float_ref = 0;
						}
					}
					break;
				case MENU_ITEM_FLOAT_RANGE:
					if (item_change_x) {
						float *value = item->u.range.float_range.value;
						float min = item->u.range.float_range.min;
						float max = item->u.range.float_range.max;
						*value += item_change_x * 0.1f;
						if (*value < min) {
							*value = min;
						}
						else if (*value > max) {
							*value = max;
						}
					}
					break;
				case MENU_ITEM_BOOL:
					if (item_change_x || item_enter) {
						*item->u.bool_ref = !*item->u.bool_ref;
					}
					break;
				case MENU_ITEM_ACTION:
					if (item_enter) {
						switch (item->u.int_value) {
							case MENU_ACTION_CONTINUE:
								game_state->show_menu = false;
								break;
							case MENU_ACTION_NEW_GAME:
								game_state->show_menu = false;
								reset_game(game_state, view);
								break;
							case MENU_ACTION_QUIT:
								game_state->running = false;
								break;

							case MENU_ACTION_FULLSCREEN_TOGGLE:
								item->action(1, NULL);
								break;
						}
					}
					break;
				case MENU_ITEM_MENU:
					if (item_enter) {
						item->u.menu_ref->parent = menu;
						menu = item->u.menu_ref;
						menu->selected_index = 0;
					}
					break;
				case MENU_ITEM_MENU_BACK:
					if (item_enter) {
						Menu *parent = menu->parent;
						if (parent) {
							menu = menu->parent;
						}
					}
					break;
				default: break;
			}
		}


		//
		//-----------------------------------------------------
		// Draw
		//-----------------------------------------------------
		//

		BeginDrawing();

		ClearBackground((Color){game_state->color_red, game_state->color_green, game_state->color_blue, 255});

		Vector2 screen = (Vector2){view.screen_width, view.screen_height};

		#define SCREEN_TEXT_POS(u, v) Vector2Add((Vector2){screen.x*(u),screen.y*(v)}, Vector2Scale(text_bounds, -0.5f))
		//
		// Draw Title
		//
		if (game_state->title_alpha > 0) {

			float title_gray = 160;
			Color title_color = (Color){title_gray, title_gray, title_gray, game_state->title_alpha*255.0f};

			float font_size = 100.0f*view.scale;
			float font_spacing = 0.15f*font_size;

			Vector2 text_bounds = MeasureTextEx(default_font, title, font_size, font_spacing);
			Vector2 text_position = SCREEN_TEXT_POS(0.5f, 1.0f/3.0f);

			DrawTextEx(default_font, title, text_position, font_size, font_spacing, title_color);

			font_size *= 0.5f;
			font_spacing *= 0.5f;

			const char *author = "By Jakob Kjær-Kammersgaard";
			text_bounds = MeasureTextEx(default_font, author, font_size, font_spacing);
			text_position = SCREEN_TEXT_POS(0.5f, 2.0f/3.0f);

			DrawTextEx(default_font, author, text_position, font_size, font_spacing, title_color);

			const char *website = "www.miscellus.com";
			font_size *= 0.75f;
			font_spacing *= 0.75f;

			text_bounds = MeasureTextEx(default_font, website, font_size, font_spacing);
			text_position.x = 0.5f*(screen.x - text_bounds.x);
			text_position.y += 2.0f*text_bounds.y;

			DrawTextEx(default_font, website, text_position, font_size, font_spacing, title_color);
		}

		//
		// Draw player death animations
		//
		for (int player_index = 0; player_index < game_params->num_players; ++player_index) {
			Player *player = game_state->players + player_index;
			if (player->health == 0 && player->death_animation_t < 1.0f) {
				float t = player->death_animation_t;

				Color ring_color = player->params.color;
				ring_color.a = 64;

				float t1 = 1.0f - t;
				float t2 = t;
				t1 = 1.0f - t1*t1;

				float ring_radius = 0.5f*(view.width + view.height)*view.scale;

				float outer_radius = t1*ring_radius;
				float inner_radius = t2*ring_radius;

				Vector2 pos = Vector2Scale(player->position, view.scale);

				DrawRing(pos, inner_radius, outer_radius, 0, 360, 60, ring_color);

				player->death_animation_t += dt;
			}
		}

		//
		// Draw player's bullets
		//
		for (int player_index = 0; player_index < game_params->num_players; ++player_index) {

			Player *player = game_state->players + player_index;

			Player_Parameters *parameters = &player->params;

			for (int bullet_index = 0; bullet_index < player->active_bullets; ++bullet_index) {

				Bullet *bullet = &player->bullets[bullet_index];

				float s = bullet->time < 0.3f ? bullet->time/0.3f : 1.0f;

				Vector2 direction = Vector2Scale(bullet->velocity, -0.2f*s);
				Vector2 point_tail = Vector2Add(bullet->position, direction);
				Vector2 tail_to_position_difference = Vector2Subtract(bullet->position, point_tail);

				float mid_circle_radius = 0.5f*Vector2Length(tail_to_position_difference);

				Vector2 mid_point = Vector2Add(point_tail, Vector2Scale(tail_to_position_difference, 0.5f));

				float t = 1.0f;

				if (bullet->time >= game_params->bullet_time_begin_fade) {
					t = (bullet->time - game_params->bullet_time_begin_fade)/(game_params->bullet_time_end_fade - game_params->bullet_time_begin_fade);
					t *= t*t;
					t = 1.0f - t;
				}

				Circle mid_circle = {mid_point, mid_circle_radius};

				float bullet_radius = game_params->bullet_radius;
				Circle bullet_circle = {bullet->position, bullet_radius*t};

				Intersection_Points result = intersection_points_from_two_circles(bullet_circle, mid_circle);


				float bullet_scale = t*view.scale;

				if (result.are_intersecting) {
					Color tail_color = parameters->color;
					tail_color.a = 32;
					DrawTriangle(Vector2Scale(point_tail, view.scale), Vector2Scale(result.intersection_points[0], view.scale), Vector2Scale(result.intersection_points[1], view.scale), tail_color);
				}

				Vector2 bullet_screen_position = Vector2Scale(bullet->position, view.scale);
				DrawCircleV(bullet_screen_position, bullet_radius*bullet_scale, parameters->color);

			}
		}

		//
		// Draw players
		//
		for (int player_index = 0; player_index < game_params->num_players; ++player_index) {

			Player *player = game_state->players + player_index;
			if (player->health <= 0) continue;

			Player_Parameters *parameters = &player->params;

			float player_radius = calculate_player_radius(player, game_params);
			float player_radius_screen = player_radius*view.scale;
			float font_size = player_radius_screen*1.0f;
			float font_spacing = font_size*FONT_SPACING_FOR_SIZE;

			Vector2 player_position_screen = Vector2Scale(player->position, view.scale);
			// printf("Player(%d) Pos: {%.2f,%.2f}\n", player_index, player_position_screen.x, player_position_screen.y);

			// Draw player's body
			DrawCircleV(player_position_screen, player_radius_screen, parameters->color);

			// Draw player's move direction arrow
			if (1) {
				float angle_span;
				int spear_count;
				calculate_bullet_count_and_angle_span(player, game_params, &spear_count, &angle_span);

				float pointiness = Lerp(15.0f, 50.0f, player->shoot_charge_t);
				float half_spear_width = PI/16.0f + 0.5f*angle_span;

				float angle_quantum = angle_span / (float)spear_count;
				float angle = player->shoot_angle - 0.5f*angle_span + 0.5f*angle_quantum;

				for (int i = 0; i < spear_count; ++i) {
					float angle1 = angle - half_spear_width;
					float angle2 = angle + half_spear_width;

					Vector2 arrow_point = (Vector2){cosf(angle), sinf(angle)};
					Vector2 arrow_left = (Vector2){cosf(angle1), sinf(angle1)};
					Vector2 arrow_right = (Vector2){cosf(angle2), sinf(angle2)};
					arrow_point = Vector2Scale(arrow_point, view.scale*(player_radius + pointiness));
					arrow_left = Vector2Scale(arrow_left, player_radius_screen);
					arrow_right = Vector2Scale(arrow_right, player_radius_screen);
					arrow_point = Vector2Add(arrow_point, player_position_screen);
					arrow_left = Vector2Add(arrow_left, player_position_screen);
					arrow_right = Vector2Add(arrow_right, player_position_screen);
					// DrawCircleV(spot, player_radius*0.2f, parameters->color);

					DrawTriangle(arrow_right, arrow_point, arrow_left, parameters->color);
					
					angle += angle_quantum;
				}
			}
			else {
				float angle_offset;
				float pointiness;
				{
					float t = player->shoot_charge_t;
					if (t > 1.0f) t = 1.0f;

					float narrow_angle_offset = 2.0f*PI / 24.0f;
					float wide_angle_offset = 2.0f*PI / 7.0f;
					angle_offset = (1.0f-t)*wide_angle_offset + t*narrow_angle_offset;
					pointiness = (1.0f-t)*15.0f + t*50.0f;
				}

				float angle1 = player->shoot_angle - angle_offset;
				float angle2 = player->shoot_angle + angle_offset;

				Vector2 arrow_point = (Vector2){cosf(player->shoot_angle), sinf(player->shoot_angle)};
				Vector2 arrow_left = (Vector2){cosf(angle1), sinf(angle1)};
				Vector2 arrow_right = (Vector2){cosf(angle2), sinf(angle2)};
				arrow_point = Vector2Scale(arrow_point, view.scale*(player_radius + pointiness));
				arrow_left = Vector2Scale(arrow_left, player_radius_screen);
				arrow_right = Vector2Scale(arrow_right, player_radius_screen);
				arrow_point = Vector2Add(arrow_point, player_position_screen);
				arrow_left = Vector2Add(arrow_left, player_position_screen);
				arrow_right = Vector2Add(arrow_right, player_position_screen);
				// DrawCircleV(spot, player_radius*0.2f, parameters->color);

				DrawTriangle(arrow_right, arrow_point, arrow_left, parameters->color);
			}

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

				Vector2 health_text_position = Vector2Add(player_position_screen, Vector2Scale(health_text_bounds, -0.5f));

				draw_text_shadowed(default_font, health_text_string, health_text_position, font_size, font_spacing, WHITE, BLACK);
			}
		}

		//
		// Draw explosion rings
		//

		for (int ring_index = 0; ring_index < game_state->active_rings; ++ring_index) {

			Ring ring = game_state->rings[ring_index];

			Player *player = &game_state->players[ring.player_index];

			Color ring_color = player->params.color;

			float t1 = 1.0f - ring.t;
			float t2 = ring.t;
			t1 = 1.0f - t1*t1;

			float ring_radius = 5.0f*game_params->bullet_radius*view.scale;

			float outer_radius = t1*ring_radius;
			float inner_radius = t2*ring_radius;

			float start_angle = ring.angle - 60.0;
			float end_angle = ring.angle + 60.0f;

			ring.position = Vector2Scale(ring.position, view.scale);

			DrawRing(ring.position, inner_radius, outer_radius, start_angle, end_angle, 20, ring_color);
		}

		//
		// Draw player controls
		//

		for (int player_index = 0; player_index < game_params->num_players; ++player_index) {
			Player *player = game_state->players + player_index;
			if (player->health <= 0) continue;

			Player_Parameters *parameters = &player->params;

			Vector2 player_screen_position = Vector2Scale(player->position, view.scale);

			float player_speed = Vector2Length(player->velocity);

			float max_speed_while_drawing_control_text = 150.0f;

			if (player_speed <= max_speed_while_drawing_control_text) {
				if (player->controls_text_timeout < game_state->game_play_time) {

					float player_radius = calculate_player_radius(player, game_params)*view.scale;
					float font_size = player_radius*1.0f;
					float font_spacing = font_size*FONT_SPACING_FOR_SIZE;

					Color controls_color = parameters->color;

					const char *text = parameters->key_text;

					font_size = 30.0f*view.scale;
					font_spacing = font_size*FONT_SPACING_FOR_SIZE;

					Vector2 text_bounds = MeasureTextEx(default_font, text, font_size, font_spacing);

					Vector2 text_position = Vector2Add(player_screen_position, Vector2Scale(text_bounds, -0.5f));

					text_position.y -= player_radius + font_size;

					DrawTextEx(default_font, text, text_position, font_size, font_spacing, controls_color);
				}
			}
			else {
				player->controls_text_timeout = game_state->game_play_time + 5.0;
			}
		}

		int triumphant_player = game_state->triumphant_player;

		if (triumphant_player >= 0 || game_state->show_menu) {

			assert(triumphant_player < game_params->num_players);

			Color screen_overlay_color = (Color){255.0f, 255.0f, 255.0f, 192.0f};

			DrawRectangle(0, 0, screen.x, screen.y, screen_overlay_color);

			Vector2 screen_center = (Vector2){0.5f*screen.x, 0.5f*screen.y};

			if (!game_state->show_menu) {

				Color win_box_color = game_state->players[triumphant_player].params.color;
				win_box_color.a = 192;



				const char *win_text = (const char *[]){
					"Orange Player Wins",
					"Blue Player Wins",
					"Green Player Wins",
					"Purple Player Wins",
				}[triumphant_player];

				const char *reset_button_text = "Press [Esc] or [Menu] to Reset";

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
				DrawRectangleV(draw_position, (Vector2){screen.x, colored_background_height}, win_box_color);

				draw_position.x = screen_center.x - 0.5f*win_text_metrics.x;
				draw_position.y += padding;

				draw_text_shadowed(default_font, win_text, draw_position, win_text_font_size, win_text_font_spacing, WHITE, BLACK);

				draw_position.y += win_text_metrics.y;

				draw_position.x = screen_center.x - 0.5f*reset_button_text_metrics.x;
				draw_position.y += padding;

				draw_text_shadowed(default_font, reset_button_text, draw_position, reset_button_text_font_size, reset_button_text_font_spacing, WHITE, BLACK);
			}
			else {

				float font_size = 48*view.scale;
				float font_spacing = font_size*FONT_SPACING_FOR_SIZE;
				float line_advance = 70.0f*view.scale;
				float menu_height = menu->item_count * line_advance;

				Vector2 pos = Vector2Multiply(screen, (Vector2){0.5f, 0.5f});
				pos.y -= 0.5f*menu_height;

				for (unsigned int menu_item_index = 0;
					menu_item_index < menu->item_count;
					++menu_item_index
				) {
					Menu_Item *item = &menu->items[menu_item_index];

					char buffer[512];

					switch (item->type) {
						case MENU_ITEM_INT:
						case MENU_ITEM_INT_RANGE:
							snprintf(buffer, sizeof(buffer), "%s: %d", item->text, *item->u.int_ref);
							break;
						case MENU_ITEM_FLOAT:
						case MENU_ITEM_FLOAT_RANGE:
							snprintf(buffer, sizeof(buffer), "%s: %.2f", item->text, *item->u.float_ref);
							break;
						case MENU_ITEM_BOOL:
							snprintf(buffer, sizeof(buffer), "%s: %s", item->text, *item->u.bool_ref ? "On" : "Off");
							break;
						default:
							snprintf(buffer, sizeof(buffer), "%s", item->text);
					}

					Vector2 text_dim = MeasureTextEx(default_font, buffer, font_size, font_spacing);

					pos.x = screen_center.x - 0.5f*text_dim.x;


					bool selected = menu_item_index == menu->selected_index;

					if (selected) {
						Vector2 rect_pos = Vector2Subtract(pos, (Vector2){25.0f*view.scale, 10.0f*view.scale});
						Vector2 rect_size = Vector2Add(text_dim, (Vector2){50.0f*view.scale, 20.0f*view.scale});
						DrawRectangleV(rect_pos, rect_size, BLACK);
						DrawTextEx(default_font, buffer, pos, font_size, font_spacing, WHITE);
					}
					else {
						DrawTextEx(default_font, buffer, pos, font_size, font_spacing, BLACK);
					}

					pos.y += line_advance;
				}







				// DrawTextEx(default_font, menu_text, (Vector2){10.0f, 10.0f}, 30.0f, FONT_SPACING_FOR_SIZE*view.scale, BLACK);
			}
		}

		// DrawFPS(view_width-100, 10);

#if 0
		{
			float font_size = 20*view.scale;
			float font_spacing = font_size*FONT_SPACING_FOR_SIZE;

			for (int i = 0; i < 16; ++i) {
				// Virtual_Input_Device *dev = &input_devices[i];

				bool is_present = glfwJoystickPresent(i);
				bool is_joystick = glfwJoystickIsGamepad(i);

				Vector2 pos = (Vector2){25.0f*view.scale, (10.0f + i*20.0f)*view.scale};

				DrawTextEx(default_font, is_present ? "^" : "v", pos, font_size, font_spacing, BLACK);
				pos.x += 15.0f*view.scale;

				DrawTextEx(default_font, is_joystick ? "J" : "?", pos, font_size, font_spacing, BLACK);
				pos.x += 15.0f*view.scale;

				for (int j = 0; j < 16; ++j) {
					bool button_down = IsGamepadButtonDown(i, j);
					DrawTextEx(default_font, button_down ? "_" : "#", pos, font_size, font_spacing, BLACK);
					pos.x += 15.0f*view.scale;
				}
			}
		}
#endif

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


