#ifndef __DDB_CUSTOMHEADERBAR_DEADBEEF_UTIL_H
#define __DDB_CUSTOMHEADERBAR_DEADBEEF_UTIL_H

struct ddb_gtkui_widget_s;
struct json_t;

int w_create_from_json(struct json_t *node,struct ddb_gtkui_widget_s **parent);
struct json_t *w_save_widget_to_json(struct ddb_gtkui_widget_s *w);
void send_messages_to_widgets(struct ddb_gtkui_widget_s *w,uint32_t id,uintptr_t ctx,uint32_t p1,uint32_t p2);

#endif
