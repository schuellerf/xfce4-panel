/* $Id$
 *
 * Copyright (c) 2005-2007 Jasper Huijsmans <jasper@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __XFCE_PANEL_PLUGIN_MESSAGES_H__
#define __XFCE_PANEL_PLUGIN_MESSAGES_H__

#include <gdk/gdkx.h>

G_BEGIN_DECLS

#define XFCE_PANEL_PLUGIN_ATOM "XFCE4_XFCE_PANEL_PLUGIN"

typedef enum
{
    XFCE_PANEL_PLUGIN_CONSTRUCT,
    XFCE_PANEL_PLUGIN_FREE_DATA,
    XFCE_PANEL_PLUGIN_SAVE,
    XFCE_PANEL_PLUGIN_SIZE,
    XFCE_PANEL_PLUGIN_SCREEN_POSITION,
    XFCE_PANEL_PLUGIN_REMOVE,
    XFCE_PANEL_PLUGIN_EXPAND,
    XFCE_PANEL_PLUGIN_CUSTOMIZE,
    XFCE_PANEL_PLUGIN_MENU_DEACTIVATED,
    XFCE_PANEL_PLUGIN_POPUP_MENU,
    XFCE_PANEL_PLUGIN_CUSTOMIZE_ITEMS,
    XFCE_PANEL_PLUGIN_SENSITIVE,
    XFCE_PANEL_PLUGIN_MOVE,
    XFCE_PANEL_PLUGIN_FOCUS,
    XFCE_PANEL_PLUGIN_SET_HIDDEN
}
XfcePanelPluginMessage;

void  xfce_panel_plugin_message_send  (GdkWindow              *from,
                                       GdkNativeWindow         xid,
                                       XfcePanelPluginMessage  message,
                                       gint                    value);

G_END_DECLS

#endif /* !__XFCE_PANEL_PLUGIN_MESSAGES_H__ */