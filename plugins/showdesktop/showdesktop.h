/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __XFCE_SHOW_DESKTOP_PLUGIN_H__
#define __XFCE_SHOW_DESKTOP_PLUGIN_H__

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libwnck/libwnck.h>

G_BEGIN_DECLS

typedef struct _ShowDesktopPluginClass ShowDesktopPluginClass;
typedef struct _ShowDesktopPlugin      ShowDesktopPlugin;

#define XFCE_TYPE_SHOW_DESKTOP_PLUGIN            (show_desktop_plugin_get_type ())
#define XFCE_SHOW_DESKTOP_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SHOW_DESKTOP_PLUGIN, ShowDesktopPlugin))
#define XFCE_SHOW_DESKTOP_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SHOW_DESKTOP_PLUGIN, ShowDesktopPluginClass))
#define XFCE_IS_SHOW_DESKTOP_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SHOW_DESKTOP_PLUGIN))
#define XFCE_IS_SHOW_DESKTOP_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SHOW_DESKTOP_PLUGIN))
#define XFCE_SHOW_DESKTOP_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SHOW_DESKTOP_PLUGIN, ShowDesktopPluginClass))

GType show_desktop_plugin_get_type      (void) G_GNUC_CONST;

void  show_desktop_plugin_register_type (XfcePanelTypeModule *type_module);

G_END_DECLS

#endif /* !__XFCE_SHOW_DESKTOP_PLUGIN_H__ */