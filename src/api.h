#ifndef __DDB_CUSTOMHEADERBAR_API_H
#define __DDB_CUSTOMHEADERBAR_API_H

typedef struct ddb_customheaderbar_s{
	DB_misc_t misc;
    GtkHeaderBar       *(*get_headerbar)();
    ddb_gtkui_widget_t *(*get_rootwidget_start)();
    ddb_gtkui_widget_t *(*get_rootwidget_end)();
} ddb_customheaderbar_t;

#endif
