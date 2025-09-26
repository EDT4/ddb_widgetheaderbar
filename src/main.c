#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>
#include <stdbool.h>

#include "deadbeef_util.h"

DB_functions_t *deadbeef;
static ddb_gtkui_t *gtkui_plugin;

enum{
	BUTTON_PREV,
	BUTTON_PLAYPAUSE,
	BUTTON_STOP,
	BUTTON_NEXT,
	BUTTON_NEXT_SHUFFLE,
	BUTTON_JUMP_TO_CURRENT,

	BUTTON_COUNT
};

struct{
	GtkHeaderBar *headerbar;
	GtkWidget *buttons[BUTTON_COUNT];
	GtkWidget *volume_scale;
	GtkWidget *dsp_combo;
	guint ui_callback_id;
	guint volume_change_callback_id;
	GHashTable *db_action_map;
} customheaderbar_data;

static void on_notify_title(__attribute__((unused)) GtkWidget* widget,__attribute__((unused)) GParamSpec* property,__attribute__((unused)) gpointer data){
	GtkWidget *title_label = gtk_header_bar_get_custom_title(GTK_HEADER_BAR(customheaderbar_data.headerbar));
	if(title_label) gtk_label_set_text(GTK_LABEL(title_label),gtk_window_get_title(GTK_WINDOW(gtkui_plugin->get_mainwin())));
}

static void on_menubar_toggle(__attribute__((unused)) GtkButton* self,gpointer user_data){
	int val = 1 - deadbeef->conf_get_int("gtkui.show_menu",1);
	gtk_widget_set_visible(gtkui_lookup_widget(GTK_WIDGET(user_data),"menubar"),val);
	deadbeef->conf_set_int("gtkui.show_menu",val);
}

static void on_volumebar_change(GtkRange* self,__attribute__((unused)) gpointer user_data){
	deadbeef->volume_set_amp(gtk_range_get_value(self));
}

static gboolean volumebar_change(__attribute__((unused)) gpointer user_data){
	const GSignalMatchType mask = (GSignalMatchType)(G_SIGNAL_MATCH_FUNC);
	g_signal_handlers_block_matched(G_OBJECT(customheaderbar_data.volume_scale),mask,0,0,NULL,(gpointer)on_volumebar_change,NULL);
	gtk_range_set_value(GTK_RANGE(customheaderbar_data.volume_scale),deadbeef->volume_get_amp());
	g_signal_handlers_unblock_matched(G_OBJECT(customheaderbar_data.volume_scale),mask,0,0,NULL,(gpointer)on_volumebar_change,NULL);
	return false;
}
static void volume_change_on_callback_end(__attribute__((unused)) void *data){
	customheaderbar_data.volume_change_callback_id = 0;
}

static int dsp_preset_scandir_filter(const struct dirent *ent){
    char *ext = strrchr(ent->d_name,'.');
    if(ext && !strcasecmp(ext,".txt")){
        return 1;
    }
    return 0;
}
static gboolean dspcombo_init(__attribute__((unused)) gpointer user_data){
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(customheaderbar_data.dsp_combo));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(customheaderbar_data.dsp_combo),"");

	char path[PATH_MAX];
	if(snprintf(path,sizeof(path),"%s/presets/dsp",deadbeef->get_system_dir(DDB_SYS_DIR_CONFIG)) >= 0){
		struct dirent **namelist = NULL;
		int n = scandir(path,&namelist,dsp_preset_scandir_filter,alphasort);
		for(int i=0; i<n; i++){
			{
				char *c;
				for(c=namelist[i]->d_name; *c!='\0' && c<namelist[i]->d_name+PATH_MAX; c+=1);
				if(namelist[i]->d_name+4 < c) *(c-4) = '\0';
			}

			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(customheaderbar_data.dsp_combo),namelist[i]->d_name);
			free(namelist[i]);
		}
		free(namelist);
	}
	return false;
}
//static gboolean dspcombo_change(__attribute__((unused)) gpointer user_data){
//	gtk_combo_box_set_active(GTK_COMBO_BOX(customheaderbar_data.dsp_combo),0);
//}
static void on_dspcombo_change(GtkComboBox* self,__attribute__((unused)) gpointer user_data){
	if(gtk_combo_box_get_active(self) == 0) return;

	char path[PATH_MAX];
	if(snprintf(path,sizeof(path),"%s/presets/dsp/%s.txt",deadbeef->get_system_dir(DDB_SYS_DIR_CONFIG),gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(self))) < 0) return;
	ddb_dsp_context_t *chain = NULL;
	if(deadbeef->dsp_preset_load(path,&chain)) return;
	deadbeef->streamer_set_dsp_chain(chain);
}

#define PLAY_IMAGE_NEW  gtk_image_new_from_icon_name("media-playback-start-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR)
#define PAUSE_IMAGE_NEW gtk_image_new_from_icon_name("media-playback-pause-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR)

#define BUTTON_ACTION_INIT(button,name) \
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),"db." name);\
	{\
		DB_plugin_action_t *db_action = g_hash_table_lookup(customheaderbar_data.db_action_map,name);\
		if(db_action) gtk_widget_set_tooltip_text(button,db_action->title);\
	}

static void customheaderbar_window_init_hook(__attribute__((unused)) void *user_data){
	customheaderbar_data.db_action_map = g_hash_table_new_full(g_str_hash,g_str_equal,free,NULL); //TODO: free
	customheaderbar_data.ui_callback_id = 0;

	GtkWidget *window = gtkui_plugin->get_mainwin();
	g_assert_nonnull(window);

	GActionGroup *action_group = deadbeef_action_group(customheaderbar_data.db_action_map);
	customheaderbar_data.headerbar = GTK_HEADER_BAR(gtk_header_bar_new());
		gtk_header_bar_set_show_close_button(customheaderbar_data.headerbar,true);
			GtkWidget *button_box;
			button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
				#define button customheaderbar_data.buttons[BUTTON_PREV]
				button = gtk_button_new_from_icon_name("media-skip-backward-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR);
					BUTTON_ACTION_INIT(button,"prev_or_restart");
					gtk_widget_show(button);
				gtk_box_pack_start(GTK_BOX(button_box),button,false,false,0);
				#undef button

				#define button customheaderbar_data.buttons[BUTTON_PLAYPAUSE]
				button = gtk_button_new();
					gtk_button_set_image(GTK_BUTTON(button),PLAY_IMAGE_NEW);
					BUTTON_ACTION_INIT(button,"play_pause");
					gtk_widget_show(button);
				gtk_box_pack_start(GTK_BOX(button_box),button,false,false,0);
				#undef button

				#define button customheaderbar_data.buttons[BUTTON_STOP]
				button = gtk_button_new_from_icon_name("media-playback-stop-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR);
					BUTTON_ACTION_INIT(button,"stop");
					gtk_widget_show(button);
				gtk_box_pack_start(GTK_BOX(button_box),button,false,false,0);
				#undef button

				#define button customheaderbar_data.buttons[BUTTON_NEXT]
				button = gtk_button_new_from_icon_name("media-skip-forward-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR);
					BUTTON_ACTION_INIT(button,"next");
					gtk_widget_show(button);
				gtk_box_pack_start(GTK_BOX(button_box),button,false,false,0);
				#undef button

				#define button customheaderbar_data.buttons[BUTTON_NEXT_SHUFFLE]
				button = gtk_button_new_from_icon_name("media-playlist-shuffle-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR);
					BUTTON_ACTION_INIT(button,"playback_random");
					gtk_widget_show(button);
				gtk_box_pack_start(GTK_BOX(button_box),button,false,false,0);
				#undef button

				gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box),GTK_BUTTONBOX_EXPAND);
				gtk_widget_show(button_box);
			gtk_header_bar_pack_start(customheaderbar_data.headerbar,button_box);

			button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
				#define button customheaderbar_data.buttons[BUTTON_JUMP_TO_CURRENT]
				button = gtk_button_new_from_icon_name("edit-select-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR);
					BUTTON_ACTION_INIT(button,"jump_to_current_track");
					gtk_widget_show(button);
				gtk_box_pack_start(GTK_BOX(button_box),button,false,false,0);
				#undef button

				gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box),GTK_BUTTONBOX_EXPAND);
				gtk_widget_show(button_box);
			gtk_header_bar_pack_start(customheaderbar_data.headerbar,button_box);

			customheaderbar_data.dsp_combo = gtk_combo_box_text_new();
				dspcombo_init(NULL);
				{
					GtkLabel *dsp_combo_label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(customheaderbar_data.dsp_combo)));
					gtk_label_set_max_width_chars(dsp_combo_label,16);
					gtk_label_set_ellipsize(dsp_combo_label,PANGO_ELLIPSIZE_END);
				}
				g_signal_connect(customheaderbar_data.dsp_combo,"changed",G_CALLBACK(on_dspcombo_change),NULL);
				gtk_widget_show(customheaderbar_data.dsp_combo);
			gtk_header_bar_pack_start(customheaderbar_data.headerbar,customheaderbar_data.dsp_combo);

			button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
				GtkWidget *button;

				button = gtk_button_new_from_icon_name("preferences-other-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR);
					BUTTON_ACTION_INIT(button,"preferences");
					gtk_style_context_add_class(gtk_widget_get_style_context(button),"flat");
					gtk_widget_show(button);
				gtk_box_pack_start(GTK_BOX(button_box),button,false,false,0);

				button = gtk_button_new_from_icon_name("open-menu-symbolic",GTK_ICON_SIZE_SMALL_TOOLBAR);
					g_signal_connect(button,"clicked",G_CALLBACK(on_menubar_toggle),window);
					gtk_widget_set_tooltip_text(button,"Toggle menu bar");
					gtk_style_context_add_class(gtk_widget_get_style_context(button),"flat");
					gtk_widget_show(button);
				gtk_box_pack_start(GTK_BOX(button_box),button,false,false,0);

				gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box),GTK_BUTTONBOX_EXPAND);
				gtk_widget_show(button_box);
			gtk_header_bar_pack_end(customheaderbar_data.headerbar,button_box);

			customheaderbar_data.volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0,1.0,0.05);
				volumebar_change(NULL);
				g_signal_connect(customheaderbar_data.volume_scale,"value-changed",G_CALLBACK(on_volumebar_change),NULL);
				gtk_scale_set_draw_value(GTK_SCALE(customheaderbar_data.volume_scale),false);
				gtk_widget_set_size_request(customheaderbar_data.volume_scale,100,-1);
				gtk_widget_show(customheaderbar_data.volume_scale);
			gtk_header_bar_pack_end(customheaderbar_data.headerbar,customheaderbar_data.volume_scale);

		gtk_widget_show(GTK_WIDGET(customheaderbar_data.headerbar));
	gtk_window_set_titlebar(GTK_WINDOW(window),GTK_WIDGET(customheaderbar_data.headerbar));
	gtk_widget_insert_action_group(GTK_WIDGET(customheaderbar_data.headerbar),"db",action_group);

	g_signal_connect(
		G_OBJECT(window),
		"notify::title",
		G_CALLBACK(on_notify_title),
		NULL
	);
}

static gboolean ui_on_play(__attribute__((unused)) gpointer user_data){
	gtk_widget_set_sensitive(customheaderbar_data.buttons[BUTTON_JUMP_TO_CURRENT],true);
	gtk_widget_set_sensitive(customheaderbar_data.buttons[BUTTON_STOP],true);
	gtk_button_set_image(GTK_BUTTON(customheaderbar_data.buttons[BUTTON_PLAYPAUSE]),PAUSE_IMAGE_NEW);
	return false;
}
static gboolean ui_on_stop(__attribute__((unused)) gpointer user_data){
	gtk_widget_set_sensitive(customheaderbar_data.buttons[BUTTON_JUMP_TO_CURRENT],false);
	gtk_widget_set_sensitive(customheaderbar_data.buttons[BUTTON_STOP],false);
	gtk_button_set_image(GTK_BUTTON(customheaderbar_data.buttons[BUTTON_PLAYPAUSE]),PLAY_IMAGE_NEW);
	return false;
}
static gboolean ui_on_unpause(__attribute__((unused)) gpointer user_data){
	gtk_button_set_image(GTK_BUTTON(customheaderbar_data.buttons[BUTTON_PLAYPAUSE]),PAUSE_IMAGE_NEW);
	return false;
}
static gboolean ui_on_pause(__attribute__((unused)) gpointer user_data){
	gtk_button_set_image(GTK_BUTTON(customheaderbar_data.buttons[BUTTON_PLAYPAUSE]),PLAY_IMAGE_NEW);
	return false;
}
static void ui_on_callback_end(__attribute__((unused)) void *data){
	customheaderbar_data.ui_callback_id = 0;
}
static int customheaderbar_message(uint32_t id,__attribute__((unused)) uintptr_t ctx,uint32_t p1,__attribute__((unused)) uint32_t p2){
	//TODO: MAybe instead of sentivitiy, set enabled on actions.
	switch(id){
		case DB_EV_SONGFINISHED:
			if(customheaderbar_data.ui_callback_id != 0) g_source_remove(customheaderbar_data.ui_callback_id);
			customheaderbar_data.ui_callback_id = g_idle_add_full(G_PRIORITY_LOW,ui_on_stop,NULL,ui_on_callback_end);
			break;
		case DB_EV_SONGSTARTED:
			if(customheaderbar_data.ui_callback_id != 0) g_source_remove(customheaderbar_data.ui_callback_id);
			customheaderbar_data.ui_callback_id = g_idle_add_full(G_PRIORITY_LOW,ui_on_play,NULL,ui_on_callback_end);
			break;
		case DB_EV_PAUSED:
			if(customheaderbar_data.ui_callback_id != 0) g_source_remove(customheaderbar_data.ui_callback_id);
			if(p1){
				customheaderbar_data.ui_callback_id = g_idle_add_full(G_PRIORITY_LOW,ui_on_pause,NULL,ui_on_callback_end);
			}else{
				customheaderbar_data.ui_callback_id = g_idle_add_full(G_PRIORITY_LOW,ui_on_unpause,NULL,ui_on_callback_end);
			}
			break;
		case DB_EV_VOLUMECHANGED:
			if(customheaderbar_data.volume_change_callback_id == 0){
				customheaderbar_data.volume_change_callback_id = g_idle_add_full(G_PRIORITY_LOW,volumebar_change,NULL,volume_change_on_callback_end);
			}
			break;
		//case DB_EV_DSPCHAINCHANGED:
		//	g_idle_add(dspcombo_change,NULL);
		//	break;
	}
	return 0;
}

static int customheaderbar_connect(){
	gtkui_plugin = (ddb_gtkui_t*) deadbeef->plug_get_for_id(DDB_GTKUI_PLUGIN_ID);
	if(!gtkui_plugin) return -1;
	gtkui_plugin->add_window_init_hook(customheaderbar_window_init_hook,NULL);
	return 0;
}

static DB_misc_t plugin = {
	.plugin.api_vmajor = DB_API_VERSION_MAJOR,
	.plugin.api_vminor = DB_API_VERSION_MINOR,
	.plugin.version_major = 1,
	.plugin.version_minor = 0,
	.plugin.type = DB_PLUGIN_MISC,
	.plugin.id = "customheaderbar-gtk3",
	.plugin.name = "Custom Header bar for GTK3 UI",
	.plugin.descr = "A customisable header bar for GTK3.",
	.plugin.copyright =
		"Written by EDT4 for private use. No guarantees.\n"
		"The formal permissions/requirements are stated below:\n"
		"\n"
		"MIT License\n"
		"\n"
		"Copyright 2025 EDT4\n"
		"\n"
		"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
		"of this software and associated documentation files (the \"Software\"), to deal\n"
		"in the Software without restriction, including without limitation the rights\n"
		"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
		"copies of the Software, and to permit persons to whom the Software is\n"
		"furnished to do so, subject to the following conditions:\n"
		"\n"
		"The above copyright notice and this permission notice shall be included in all\n"
		"copies or substantial portions of the Software.\n"
		"\n"
		"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
		"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
		"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
		"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
		"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
		"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
		"SOFTWARE.\n"
	,
	.plugin.website = "https://example.org",
	.plugin.connect = customheaderbar_connect,
	.plugin.message = customheaderbar_message,
};

__attribute__ ((visibility ("default")))
DB_plugin_t *customheaderbar_gtk3_load(DB_functions_t *api){
	deadbeef = api;
	return DB_PLUGIN(&plugin);
}
