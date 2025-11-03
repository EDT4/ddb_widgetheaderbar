#ifndef __DDB_WIDGETHEADERBAR_SUBTITLE_H
#define __DDB_WIDGETHEADERBAR_SUBTITLE_H

#include <gtk/gtk.h>

void subtitle_on_config_load(GtkHeaderBar *widget);
void subtitle_message(GtkHeaderBar *widget,uint32_t id,uintptr_t ctx,uint32_t p1,uint32_t p2);
int subtitle_start();
int subtitle_stop();

#endif
