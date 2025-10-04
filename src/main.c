#include <gtk/gtk.h>
#include <jansson.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>
#include <stdbool.h>
#include "deadbeef_util.h"
#include "gtk_util.h"

DB_functions_t *deadbeef;
ddb_gtkui_t *gtkui_plugin;

#define CUSTOMHEADERBAR_CONFIG_START_WIDGET "customheaderbar.layout.start"
#define CUSTOMHEADERBAR_CONFIG_END_WIDGET   "customheaderbar.layout.end"

struct headerbar_t{
	GtkHeaderBar *widget;
	ddb_gtkui_widget_t *start_container;
	ddb_gtkui_widget_t *end_container;
	guint on_config_load_callback_id;
	struct{
		bool menubar_button;
		bool window_buttons;
		bool decoration_layout_toggle;
		const char *decoration_layout;
	} options;
} headerbar;

static void customheaderbar_root_widget_init(ddb_gtkui_widget_t **container,const char *conf_field){
	*container = gtkui_plugin->w_create("box");
	gtk_widget_show((*container)->widget);

	ddb_gtkui_widget_t *w = NULL;
	deadbeef->conf_lock();
	const char *json = deadbeef->conf_get_str_fast(conf_field,NULL);
	if(json){
		json_t *layout = json_loads(json,0,NULL);
		if(layout != NULL){
			if(w_create_from_json(layout,&w) >= 0){
				gtkui_plugin->w_append(*container,w);
			}
			json_delete(layout);
		}
	}
	deadbeef->conf_unlock();

	if(!w){
		w = gtkui_plugin->w_create("placeholder");
		gtkui_plugin->w_append(*container,w);
	}
}

static void customheaderbar_root_widget_save(ddb_gtkui_widget_t *container,const char *conf_field){
	if(!container || !container->children) return;

	json_t *layout = w_save_widget_to_json(container->children);
	if(layout){
		char *layout_str = json_dumps(layout,JSON_COMPACT);
		if(layout_str){
			deadbeef->conf_set_str(conf_field,layout_str);
			free(layout_str);
		}
		json_delete(layout);
	}
}

static void on_notify_title(__attribute__((unused)) GtkWidget* widget,__attribute__((unused)) GParamSpec* property,__attribute__((unused)) gpointer data){
	GtkWidget *title_label = gtk_header_bar_get_custom_title(GTK_HEADER_BAR(headerbar.widget));
	if(title_label) gtk_label_set_text(GTK_LABEL(title_label),gtk_window_get_title(GTK_WINDOW(gtkui_plugin->get_mainwin())));
}

static gboolean on_config_load(__attribute__((unused)) gpointer user_data){
	#define CONFIG_IF_BEGIN(ty,conf_get,opt_var) \
		ty opt = conf_get;\
		if(opt_var != opt){\
			opt_var = opt;
	#define CONFIG_IF_END() }

	{
		CONFIG_IF_BEGIN(bool,deadbeef->conf_get_int("customheaderbar.decoration_layout_toggle",0),headerbar.options.decoration_layout_toggle)
			if(headerbar.options.decoration_layout_toggle){
				deadbeef->conf_lock();
				gtk_header_bar_set_decoration_layout(headerbar.widget,deadbeef->conf_get_str_fast("customheaderbar.decoration_layout",""));
				deadbeef->conf_unlock();
			}else{
				gtk_header_bar_set_decoration_layout(headerbar.widget,NULL);
			}
		CONFIG_IF_END()
	}{
		CONFIG_IF_BEGIN(bool,deadbeef->conf_get_int("customheaderbar.window_buttons",1),headerbar.options.window_buttons)
			gtk_header_bar_set_show_close_button(headerbar.widget,headerbar.options.window_buttons);
		CONFIG_IF_END()
	}
	return G_SOURCE_REMOVE;
}
static void on_config_load_callback_end(__attribute__((unused)) void *data){
	headerbar.on_config_load_callback_id = 0;
}

static void customheaderbar_window_init_hook(__attribute__((unused)) void *user_data){
	GtkWidget *window = gtkui_plugin->get_mainwin();
	g_assert_nonnull(window);

	headerbar.widget = GTK_HEADER_BAR(gtk_header_bar_new());
		on_config_load(NULL);

		//Widget at start.
		customheaderbar_root_widget_init(&headerbar.start_container,CUSTOMHEADERBAR_CONFIG_START_WIDGET);
		gtk_header_bar_pack_start(headerbar.widget,headerbar.start_container->widget);

		//Widget at end.
		customheaderbar_root_widget_init(&headerbar.end_container,CUSTOMHEADERBAR_CONFIG_END_WIDGET);
		gtk_header_bar_pack_end(headerbar.widget,headerbar.end_container->widget);

		gtk_widget_show(GTK_WIDGET(headerbar.widget));
	gtk_window_set_titlebar(GTK_WINDOW(window),GTK_WIDGET(headerbar.widget));

	g_signal_connect(
		G_OBJECT(window),
		"notify::title",
		G_CALLBACK(on_notify_title),
		NULL
	);
}

static int customheaderbar_connect(void){
	if(!(gtkui_plugin = (ddb_gtkui_t*) deadbeef->plug_get_for_id(DDB_GTKUI_PLUGIN_ID))){
		return -1;
	}
	gtkui_plugin->add_window_init_hook(customheaderbar_window_init_hook,NULL);
	return 0;
}

static int customheaderbar_disconnect(void){
	customheaderbar_root_widget_save(headerbar.start_container,CUSTOMHEADERBAR_CONFIG_START_WIDGET);
	customheaderbar_root_widget_save(headerbar.end_container  ,CUSTOMHEADERBAR_CONFIG_END_WIDGET);
	return 0;
}

static int customheaderbar_message(uint32_t id,__attribute__((unused)) uintptr_t ctx,__attribute__((unused)) uint32_t p1,__attribute__((unused)) uint32_t p2){
	switch(id){
		case DB_EV_CONFIGCHANGED:
			if(headerbar.on_config_load_callback_id == 0){
				headerbar.on_config_load_callback_id = g_idle_add_full(G_PRIORITY_LOW,on_config_load,NULL,on_config_load_callback_end);
			}
			break;
	}
	return 0;
}

static const char settings_dlg[] =
    "property \"Show window buttons\"             checkbox customheaderbar.window_buttons           1;\n"
    "property \"Custom window decoration layout\" checkbox customheaderbar.decoration_layout_toggle 0;\n"
    "property \"Window decoration layout\"        entry    customheaderbar.decoration_layout        \"menu:minimize,maximize,close\";\n"
;

static DB_misc_t plugin ={
	.plugin.api_vmajor = DB_API_VERSION_MAJOR,
	.plugin.api_vminor = DB_API_VERSION_MINOR,
	.plugin.version_major = 1,
	.plugin.version_minor = 0,
	.plugin.type = DB_PLUGIN_MISC,
	.plugin.id = "customheaderbar-gtk3",
	.plugin.name = "Customisable Header Bar for GTK3",
	.plugin.descr = "A customisable GTK3 header bar. Widgets can be added and modified in Design Mode.",
	.plugin.copyright =
		"MIT License\n"
		"\n"
		"Copyright 2025 EDT4\n"
		"\n"
		"Permission is hereby granted,free of charge,to any person obtaining a copy\n"
		"of this software and associated documentation files(the \"Software\"),to deal\n"
		"in the Software without restriction,including without limitation the rights\n"
		"to use,copy,modify,merge,publish,distribute,sublicense,and/or sell\n"
		"copies of the Software,and to permit persons to whom the Software is\n"
		"furnished to do so,subject to the following conditions:\n"
		"\n"
		"The above copyright notice and this permission notice shall be included in all\n"
		"copies or substantial portions of the Software.\n"
		"\n"
		"THE SOFTWARE IS PROVIDED \"AS IS\",WITHOUT WARRANTY OF ANY KIND,EXPRESS OR\n"
		"IMPLIED,INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
		"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
		"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,DAMAGES OR OTHER\n"
		"LIABILITY,WHETHER IN AN ACTION OF CONTRACT,TORT OR OTHERWISE,ARISING FROM,\n"
		"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
		"SOFTWARE.\n"
	,
	.plugin.website = "https://github.org/EDT4/ddb_customheaderbar",
	.plugin.connect = customheaderbar_connect,
	.plugin.disconnect = customheaderbar_disconnect,
    .plugin.configdialog = settings_dlg,
	.plugin.message = customheaderbar_message,
};

__attribute__((visibility("default")))
DB_plugin_t * customheaderbar_gtk3_load(DB_functions_t *api){
	deadbeef = api;
	return DB_PLUGIN(&plugin);
}
