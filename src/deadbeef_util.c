#include <gtk/gtk.h>
#include <jansson.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

extern DB_functions_t *deadbeef;
extern ddb_gtkui_t *gtkui_plugin;

//Copied (with modifications) from deadbeef/plugins/gtkui/plugins.c due to it not being exported.
//Hopefully, this does not change upstream.
int w_create_from_json(json_t *node,ddb_gtkui_widget_t **parent){
	json_t *node_type = NULL;
	json_t *node_legacy_params = NULL;
	json_t *node_settings = NULL;
	json_t *node_children = NULL;

	int err = -1;

	node_type = json_object_get(node,"type");
	if(node_type == NULL || !json_is_string(node_type)){
		goto error;
	}

	node_legacy_params = json_object_get(node,"legacy_params");
	if(node_legacy_params != NULL && !json_is_string(node_legacy_params)){
		goto error;
	}

	node_settings = json_object_get(node,"settings");
	if(node_settings != NULL && !json_is_object(node_settings)){
		goto error;
	}

	node_children = json_object_get(node,"children");
	if(node_children != NULL && !json_is_array(node_children)){
		goto error;
	}

	const char *type = json_string_value(node_type);
	const char *legacy_params = node_legacy_params ? json_string_value(node_legacy_params) : "";

	ddb_gtkui_widget_t *w = gtkui_plugin->w_create(type);
	if(!w){
		w = gtkui_plugin->w_create("placeholder");
	}else{
		// nuke all default children
		while(w->children){
			ddb_gtkui_widget_t *c = w->children;
			gtkui_plugin->w_remove(w,w->children);
			gtkui_plugin->w_destroy(c);
		}

		uint32_t flags = gtkui_plugin->w_get_type_flags(type);

		if((flags & DDB_WF_SUPPORTS_EXTENDED_API) && node_settings != NULL){
			ddb_gtkui_widget_extended_api_t *api = (ddb_gtkui_widget_extended_api_t *)(w + 1);
			if(api->_size >= sizeof(ddb_gtkui_widget_extended_api_t)){
				size_t count = json_object_size(node_settings);
				if(count != 0){
					char const **keyvalues = calloc(count * 2 + 1,sizeof(char *));

					const char *key;
					json_t *value;
					int index = 0;
					json_object_foreach(node_settings,key,value){
						keyvalues[index * 2 + 0] = key;
						keyvalues[index * 2 + 1] = json_string_value(value);
						index += 1;
					}

					api->deserialize_from_keyvalues(w,keyvalues);

					free(keyvalues);
				}
			}
		}else if(w->load != NULL && legacy_params != NULL){
			// load from legacy params
			w->load(w,type,legacy_params);
		}

		size_t children_count = json_array_size(node_children);
		for(size_t i = 0; i < children_count; i++){
			json_t *child = json_array_get(node_children,i);

			if(child == NULL || !json_is_object(child)){
				goto error;
			}

			int res = w_create_from_json(child,&w);
			if(res < 0){
				goto error;
			}
		}
	}

	if(*parent){
		gtkui_plugin->w_append(*parent,w);
	}else{
		*parent = w;
	}

	err = 0;

error:

	return err;
}

//Copied (with modifications) from deadbeef/plugins/gtkui/plugins.c due to it not being exported.
json_t *w_save_widget_to_json(ddb_gtkui_widget_t *w){
    json_t *node = json_object();

    json_object_set(node,"type",json_string(w->type));

    uint32_t flags = gtkui_plugin->w_get_type_flags(w->type);

    if(flags & DDB_WF_SUPPORTS_EXTENDED_API){
        ddb_gtkui_widget_extended_api_t *api = (ddb_gtkui_widget_extended_api_t *)(w + 1);
        if(api->_size >= sizeof(ddb_gtkui_widget_extended_api_t)){
            char const **keyvalues = api->serialize_to_keyvalues(w);

            if(keyvalues != NULL){
                json_t *settings = json_object();
                for(int i = 0; keyvalues[i]; i += 2){
                    json_t *value = json_string(keyvalues[i + 1]);
                    json_object_set(settings,keyvalues[i],value);
                    json_decref(value);
                }
                json_object_set(node,"settings",settings);
                json_decref(settings);
            }
        }
    }else if(w->save){
        char params[1000] = "";
        w->save(w,params,sizeof(params));
        json_object_set(node,"legacy_params",json_string(params));
    }

    if(w->children != NULL){
        json_t *children = json_array();
        for(ddb_gtkui_widget_t *c = w->children; c; c = c->next){
            json_t *child = w_save_widget_to_json(c);
            json_array_append(children,child);
        }
        json_object_set(node,"children",children);
    }

    return node;
}
