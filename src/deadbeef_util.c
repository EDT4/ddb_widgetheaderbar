#include "deadbeef_util.h"

extern DB_functions_t *deadbeef;

//Copied from deadbeef/plugins/gtkui/actions.c:gtkui_exec_action_14.
void gtkui_exec_action_14(DB_plugin_action_t *action,int cursor){
	// Plugin can handle all tracks by itself
	if (action->flags & DB_ACTION_CAN_MULTIPLE_TRACKS)
	{
		action->callback (action, NULL);
		return;
	}

	// For single-track actions just invoke it with first selected track
	if (!(action->flags & DB_ACTION_MULTIPLE_TRACKS))
	{
		if (cursor == -1) {
			cursor = deadbeef->pl_get_cursor (PL_MAIN);
		}
		if (cursor == -1) 
		{
			return;
		}
		DB_playItem_t *it = deadbeef->pl_get_for_idx_and_iter (cursor, PL_MAIN);
		action->callback (action, it);
		deadbeef->pl_item_unref (it);
		return;
	}

	//We end up here if plugin won't traverse tracks and we have to do it ourselves
	DB_playItem_t *it = deadbeef->pl_get_first (PL_MAIN);
	while (it) {
		if (deadbeef->pl_is_selected (it)) {
			action->callback (action, it);
		}
		DB_playItem_t *next = deadbeef->pl_get_next (it, PL_MAIN);
		deadbeef->pl_item_unref (it);
		it = next;
	}
}

//Copied with modifications from deadbeef/plugins/gtkui/actions.c:menu_action_cb.
static void on_action_activate(__attribute__((unused)) GSimpleAction *act,GVariant *parameter,gpointer user_data){
	DB_plugin_action_t *db_action = (DB_plugin_action_t*) user_data;
	if(db_action->callback){
		gtkui_exec_action_14(db_action,-1);
	}else if(db_action->callback2){
		if(parameter && g_variant_is_of_type(parameter,G_VARIANT_TYPE_INT32)){
			db_action->callback2(db_action,g_variant_get_int32(parameter)); //parameter is action_ctx.
		}else{
			db_action->callback2(db_action,DDB_ACTION_CTX_MAIN);
		}
	}
}

GActionGroup *deadbeef_action_group(GHashTable *db_action_map){
	GSimpleActionGroup *group = g_simple_action_group_new();
	for(DB_plugin_t **plugin = deadbeef->plug_get_list() ; *plugin ; plugin++){
		if(!(*plugin)->get_actions) continue;
		for(DB_plugin_action_t *db_action = (*plugin)->get_actions(NULL) ; db_action; db_action = db_action->next){
			if(db_action->callback2 && db_action->flags & (DB_ACTION_COMMON | DB_ACTION_MULTIPLE_TRACKS)){
				GSimpleAction *action = g_simple_action_new(db_action->name,(db_action->flags & DB_ACTION_MULTIPLE_TRACKS) ? G_VARIANT_TYPE_INT32 : NULL);
				g_hash_table_replace(db_action_map,strdup(db_action->name),db_action);
				g_signal_connect(action,"activate",G_CALLBACK(on_action_activate),db_action);
				g_action_map_add_action(G_ACTION_MAP(group),G_ACTION(action));
			}
		}
	}

	return G_ACTION_GROUP(group);
}

//Copied from deadbeef/plugins/gtkui/support:lookup_widget.
GtkWidget* gtkui_lookup_widget(GtkWidget *widget,const gchar *widget_name){
  GtkWidget *parent, *found_widget;

  for (;;)
    {
      if (GTK_IS_MENU (widget))
        parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
        parent = gtk_widget_get_parent (widget);
      if (!parent)
        parent = (GtkWidget*) g_object_get_data (G_OBJECT (widget), "GladeParentKey");
      if (parent == NULL)
        break;
      widget = parent;
    }

  found_widget = (GtkWidget*) g_object_get_data (G_OBJECT (widget),
                                                 widget_name);
  if (!found_widget)
    g_warning ("Widget not found: %s", widget_name);
  return found_widget;
}
