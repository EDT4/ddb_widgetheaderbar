#ifndef __DDB_CUSTOMHEADERBAR_DEADBEEF_UTIL_H
#define __DDB_CUSTOMHEADERBAR_DEADBEEF_UTIL_H

#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

void gtkui_exec_action_14(DB_plugin_action_t *action,int cursor);
GActionGroup *deadbeef_action_group(GHashTable *db_action_map);
GtkWidget* gtkui_lookup_widget(GtkWidget *widget,const gchar *widget_name);

#endif
