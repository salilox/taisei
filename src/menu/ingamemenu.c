/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2019, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2019, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include "menu.h"
#include "common.h"
#include "ingamemenu.h"
#include "submenus.h"
#include "global.h"
#include "stagedraw.h"
#include "video.h"
#include "options.h"
#include "renderer/api.h"

static void return_to_title(MenuData *m, void *arg) {
	global.gameover = GAMEOVER_ABORT;
	menu_commonaction_close(m, arg);
}

void restart_game(MenuData *m, void *arg) {
	global.gameover = GAMEOVER_RESTART;
	menu_commonaction_close(m, arg);
}

static void ingame_menu_do(MenuData *m, MenuAction action) {
	m->selected = -1;

	for(int i = 0; i < m->ecount; ++i) {
		if(m->entries[i].action == action) {
			m->selected = i;
		}
	}

	assert(m->selected >= 0);
	close_menu(m);
}

static bool check_shortcut(SDL_Scancode scan, SDL_Scancode expectscan) {
	if(scan != expectscan) {
		return false;
	}

	KeyIndex k = config_key_from_scancode(scan);

	switch(k) {
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_SHOT:
		case KEY_BOMB:
			return false;

		default:
			return true;
	}
}

static bool ingame_menu_input_handler(SDL_Event *event, void *arg) {
	MenuData *menu = arg;

	switch(event->type) {
		case SDL_KEYDOWN: {
			SDL_Scancode scan = event->key.keysym.scancode;

			if(check_shortcut(scan, SDL_SCANCODE_R)) {
				ingame_menu_do(menu, restart_game);
				return true;
			}

			if(check_shortcut(scan, SDL_SCANCODE_Q)) {
				ingame_menu_do(menu, return_to_title);
				return true;
			}

			return false;
		}

		default: {
			return false;
		}
	}
}

static bool ingame_menu_input_filter(SDL_Event *event, void *arg) {
	MenuData *menu = arg;

	if(menu->state == MS_Normal) {
		return false;
	}

	switch(TAISEI_EVENT(event->type)) {
		// workaround for #136
		case TE_MENU_ACCEPT:
			return true;
	}

	return false;
}

static void ingame_menu_input(MenuData *m) {
	events_poll((EventHandler[]) {
		{ .proc = ingame_menu_input_filter, .arg = m },
		{ .proc = menu_input_handler, .arg = m },
		{ .proc = ingame_menu_input_handler, .arg = m },
		{ NULL }
	}, EFLAG_MENU);
}

void create_ingame_menu(MenuData *m) {
	create_menu(m);
	m->draw = draw_ingame_menu;
	m->logic = update_ingame_menu;
	m->input = ingame_menu_input;
	m->flags = MF_Abortable | MF_AlwaysProcessInput;
	m->transition = TransEmpty;
	m->cursor = 1;
	m->context = "Game Paused";
	add_menu_entry(m, "Options", enter_options, NULL)->transition = TransMenuDark;
	add_menu_entry(m, "Return to Game", menu_commonaction_close, NULL);
	add_menu_entry(m, "Restart the Game", restart_game, NULL)->transition = TransFadeBlack;
	add_menu_entry(m, "Stop the Game", return_to_title, NULL)->transition = TransFadeBlack;
	set_transition(TransEmpty, 0, m->transition_out_time);
}

static void skip_stage(MenuData *m, void *arg) {
	global.gameover = GAMEOVER_WIN;
	menu_commonaction_close(m, arg);
}

void create_ingame_menu_replay(MenuData *m) {
	create_menu(m);
	m->draw = draw_ingame_menu;
	m->logic = update_ingame_menu;
	m->flags = MF_Abortable | MF_AlwaysProcessInput;
	m->transition = TransEmpty;
	m->cursor = 1;
	m->context = "Replay Paused";
	add_menu_entry(m, "Options", enter_options, NULL)->transition = TransMenuDark;
	add_menu_entry(m, "Continue Watching", menu_commonaction_close, NULL);
	add_menu_entry(m, "Restart the Stage", restart_game, NULL)->transition = TransFadeBlack;
	add_menu_entry(m, "Skip the Stage", skip_stage, NULL)->transition = TransFadeBlack;
	add_menu_entry(m, "Stop Watching", return_to_title, NULL)->transition = TransFadeBlack;
	m->cursor = 1;
	set_transition(TransEmpty, 0, m->transition_out_time);
}

void draw_ingame_menu_bg(MenuData *menu, float f) {
	float rad = f*IMENU_BLUR;

	r_shader("ingame_menu");
	r_uniform_float("rad", rad);
	r_uniform_float("phase", menu->frames / 100.0);
	stage_draw_foreground();
	r_shader_standard();
}

void update_ingame_menu(MenuData *menu) {
	menu->drawdata[0] += (menu->cursor*35 - menu->drawdata[0])/7.0;
	menu->drawdata[1] += (text_width(get_font("standard"), menu->entries[menu->cursor].name, 0) - menu->drawdata[1])/10.0;
}

void draw_ingame_menu(MenuData *menu) {
	set_ortho(SCREEN_W, SCREEN_H);

	r_mat_push();

	draw_ingame_menu_bg(menu, 1.0-menu_fade(menu));

	r_mat_push();
	r_mat_translate(VIEWPORT_X, VIEWPORT_Y, 0);
	r_mat_translate(VIEWPORT_W/2, VIEWPORT_H/4, 0);

	draw_menu_selector(0, menu->drawdata[0], menu->drawdata[1]*2, 41, menu->frames);

	ShaderProgram *sh_prev = r_shader_current();
	r_shader("text_default");
	if(menu->context) {
		float s = 0.3 + 0.2 * sin(menu->frames/10.0);
		r_color(RGBA_MUL_ALPHA(1-s/2, 1-s/2, 1-s, 1-menu_fade(menu)));
		text_draw(menu->context, &(TextParams) {
			.align = ALIGN_CENTER,
			.pos = { 0, -2 * 35 },
		});
	}

	for(int i = 0; i < menu->ecount; i++) {
		if(menu->entries[i].action) {
			float s = 0, t = 0.7;
			if(i == menu->cursor) {
				t = 1;
				s = 0.3 + 0.2*sin(menu->frames/7.0);
			}

			r_color(RGBA_MUL_ALPHA(t-s, t-s, t-s/2, 1-menu_fade(menu)));
		} else {
			r_color(RGBA_MUL_ALPHA(0.5, 0.5, 0.5, 0.5 * (1-menu_fade(menu))));
		}

		text_draw(menu->entries[i].name, &(TextParams) {
			.align = ALIGN_CENTER,
			.pos = { 0, i * 35 },
		});
	}

	r_color4(1,1,1,1);
	r_mat_pop();
	r_mat_pop();
	r_shader_ptr(sh_prev);

	stage_draw_hud();
}
