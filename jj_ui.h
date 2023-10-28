#ifndef UI_H
#define UI_H

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
	MENU_ACTION_CONNECT_TO_HOST,
	MENU_ACTION_FULLSCREEN_TOGGLE,
};

typedef union Value_Range {
	struct {int *value, min, max;} int_range;
	struct {float *value, min, max;} float_range;
} Value_Range;

typedef struct Menu_Item {
	enum Menu_Item_Type type;
	const char *text;
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

#define MENU_DEF(MMM, ...) \
	MMM.items = (Menu_Item []){__VA_ARGS__}; \
	MMM.item_count = sizeof((Menu_Item []){__VA_ARGS__})/sizeof(Menu_Item);

#endif // UI_H
