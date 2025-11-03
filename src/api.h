#ifndef __DDB_WIDGETHEADERBAR_API_H
#define __DDB_WIDGETHEADERBAR_API_H

typedef struct ddb_widgetheaderbar_s{
	DB_misc_t misc;
    GtkHeaderBar       *(*get_headerbar)();
    ddb_gtkui_widget_t *(*get_rootwidget_start)();
    ddb_gtkui_widget_t *(*get_rootwidget_end)();
} ddb_widgetheaderbar_t;

#endif
