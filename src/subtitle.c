#include "subtitle.h"

#include <deadbeef/deadbeef.h>
#include <stdbool.h>

extern DB_functions_t *deadbeef;

enum option_subtitle{
	OPTION_SUBTITLE_NONE,
	OPTION_SUBTITLE_STATIC,
	OPTION_SUBTITLE_SWITCH_WHEN_PLAYING,
	//OPTION_SUBTITLE_POLL_WHILE_PLAYING, //TODO
	//OPTION_SUBTITLE_POLL_ALWAYS, //TODO
};

struct{
	guint callback_id;
	char *playing_tf_bytecode;
	char *stopped_tf_bytecode;
	char buffer[500];
	struct{
		enum option_subtitle mode;
		char playing[300];
		char stopped[300];
	} options;
} subtitle;

static gboolean subtitle_on_playing(gpointer user_data){
	GtkHeaderBar *widget = (GtkHeaderBar*)user_data;
	if(subtitle.playing_tf_bytecode){
		ddb_tf_context_t ctx = {
			._size = sizeof(ddb_tf_context_t),
			.flags = DDB_TF_CONTEXT_NO_DYNAMIC,
			.plt = NULL,
			.iter = PL_MAIN,
			.it = deadbeef->streamer_get_playing_track_safe(),
		};
		if(deadbeef->tf_eval(&ctx,subtitle.playing_tf_bytecode,subtitle.buffer,sizeof(subtitle.buffer)) >= 0){
			if(ctx.it) deadbeef->pl_item_unref(ctx.it);
			gtk_header_bar_set_subtitle(widget,subtitle.buffer);
			return G_SOURCE_REMOVE;
		}
		if(ctx.it) deadbeef->pl_item_unref(ctx.it);
	}
	gtk_header_bar_set_subtitle(widget,subtitle.options.playing);
	return G_SOURCE_REMOVE;
}
static gboolean subtitle_on_stopped(gpointer user_data){
	GtkHeaderBar *widget = (GtkHeaderBar*)user_data;
	if(subtitle.stopped_tf_bytecode){
		ddb_tf_context_t ctx = {
			._size = sizeof(ddb_tf_context_t),
			.flags = DDB_TF_CONTEXT_NO_DYNAMIC,
			.plt = NULL,
			.iter = PL_MAIN,
			.it = NULL,
		};
		if(deadbeef->tf_eval(&ctx,subtitle.stopped_tf_bytecode,subtitle.buffer,sizeof(subtitle.buffer)) >= 0){
			gtk_header_bar_set_subtitle(widget,subtitle.buffer);
			return G_SOURCE_REMOVE;
		}
	}
	gtk_header_bar_set_subtitle(widget,subtitle.options.stopped);
	return G_SOURCE_REMOVE;
}
static void subtitle_on_callback_end(__attribute__((unused)) void *data){
	subtitle.callback_id = 0;
}

void subtitle_on_config_load(GtkHeaderBar *widget){
	enum option_subtitle old_subtitle = subtitle.options.mode;
	subtitle.options.mode = deadbeef->conf_get_int("customheaderbar.subtitlebar_mode",0);

	switch(subtitle.options.mode){
		case OPTION_SUBTITLE_STATIC:{
			deadbeef->conf_lock();
				const char *s = deadbeef->conf_get_str_fast("customheaderbar.subtitlebar_stopped",NULL);
				int changed = s && strcmp(s,subtitle.options.stopped) != 0;
				if(changed){
					strncpy(subtitle.options.stopped,s,sizeof(subtitle.options.stopped));

					deadbeef->tf_free(subtitle.stopped_tf_bytecode);
					subtitle.stopped_tf_bytecode = NULL;
				}
			deadbeef->conf_unlock();

			//Update current subtitle.
			if(changed || old_subtitle != OPTION_SUBTITLE_STATIC) subtitle_on_stopped(widget);
		}	break;

		case OPTION_SUBTITLE_SWITCH_WHEN_PLAYING:{
			//TODO: This does not change the subtitle, though it will be changed later on anyway on a state change.
			const char *s;
			deadbeef->conf_lock();
				s = deadbeef->conf_get_str_fast("customheaderbar.subtitlebar_stopped",NULL);
				bool changed_stopped;
				if((changed_stopped = s && strcmp(s,subtitle.options.stopped) != 0)){
					strncpy(subtitle.options.stopped,s,sizeof(subtitle.options.stopped));
				}

				s = deadbeef->conf_get_str_fast("customheaderbar.subtitlebar_playing",NULL);
				bool changed_playing;
				if((changed_playing = s && strcmp(s,subtitle.options.playing) != 0)){
					strncpy(subtitle.options.playing,s,sizeof(subtitle.options.playing));
				}
			deadbeef->conf_unlock();

			//Update title formatting.
			if(changed_stopped){
				deadbeef->tf_free(subtitle.stopped_tf_bytecode);
				subtitle.stopped_tf_bytecode = deadbeef->tf_compile(subtitle.options.stopped);
			}
			if(changed_playing){
				deadbeef->tf_free(subtitle.playing_tf_bytecode);
				subtitle.playing_tf_bytecode = deadbeef->tf_compile(subtitle.options.playing);
			}

			//Update current subtitle.
			if(changed_stopped || changed_playing || old_subtitle != OPTION_SUBTITLE_SWITCH_WHEN_PLAYING){
				struct DB_output_s* output = deadbeef->get_output();
				if(output){
					switch(output->state()){
						case DDB_PLAYBACK_STATE_STOPPED:
							subtitle_on_stopped(widget);
							break;
						case DDB_PLAYBACK_STATE_PLAYING:
						case DDB_PLAYBACK_STATE_PAUSED:
							subtitle_on_playing(widget);
							break;
					}
				}
			}
		}	break;

		case OPTION_SUBTITLE_NONE:
			if(old_subtitle){ //If turned off.
				gtk_header_bar_set_subtitle(widget,NULL);
			}
			break;
	}
}

void subtitle_message(GtkHeaderBar *widget,uint32_t id,__attribute__((unused)) uintptr_t ctx,__attribute__((unused)) uint32_t p1,__attribute__((unused)) uint32_t p2){
	switch(id){
		case DB_EV_SONGSTARTED:
			if(subtitle.options.mode == OPTION_SUBTITLE_SWITCH_WHEN_PLAYING){
				if(subtitle.callback_id != 0) g_source_remove(subtitle.callback_id);
				subtitle.callback_id = g_idle_add_full(G_PRIORITY_LOW,subtitle_on_playing,widget,subtitle_on_callback_end);
			}
			break;
		case DB_EV_SONGFINISHED:
			if(subtitle.options.mode == OPTION_SUBTITLE_SWITCH_WHEN_PLAYING){
				if(subtitle.callback_id != 0) g_source_remove(subtitle.callback_id);
				subtitle.callback_id = g_idle_add_full(G_PRIORITY_LOW,subtitle_on_stopped,widget,subtitle_on_callback_end);
			}
			break;
	}
}

int subtitle_start(){
	subtitle.callback_id = 0;
	subtitle.stopped_tf_bytecode = NULL;
	subtitle.playing_tf_bytecode = NULL;
	subtitle.options.stopped[0] = '\0';
	subtitle.options.playing[0] = '\0';
	subtitle.options.mode = OPTION_SUBTITLE_NONE;
	return 0;
}

int subtitle_stop(){
	deadbeef->tf_free(subtitle.playing_tf_bytecode);
	deadbeef->tf_free(subtitle.stopped_tf_bytecode);
	return 0;
}
