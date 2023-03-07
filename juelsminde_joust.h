#ifndef JUELSMINDE_JOUST_H
#define JUELSMINDE_JOUST_H

typedef struct {
	int x;
} JJ_Input;

typedef struct {
	int x;
} JJ_Render_Context;

void jj_update_and_render(JJ_Input input, JJ_Render_Context render);


#endif