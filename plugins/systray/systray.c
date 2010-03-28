/*
 * Copyright (c) 2005-2007 Jasper Huijsmans <jasper@xfce.org>
 * Copyright (c) 2007-2009 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <common/panel-private.h>
#include <common/panel-xfconf.h>
#include <common/panel-utils.h>
#include <exo/exo.h>

#include "systray.h"
#include "systray-box.h"
#include "systray-socket.h"
#include "systray-manager.h"
#include "systray-dialog_ui.h"

#define ICON_SIZE (22)



static void     systray_plugin_get_property                 (GObject               *object,
                                                             guint                  prop_id,
                                                             GValue                *value,
                                                             GParamSpec            *pspec);
static void     systray_plugin_set_property                 (GObject               *object,
                                                             guint                  prop_id,
                                                             const GValue          *value,
                                                             GParamSpec            *pspec);
static void     systray_plugin_construct                    (XfcePanelPlugin       *panel_plugin);
static void     systray_plugin_free_data                    (XfcePanelPlugin       *panel_plugin);
static void     systray_plugin_screen_position_changed      (XfcePanelPlugin       *panel_plugin,
                                                             XfceScreenPosition     screen_position);
static void     systray_plugin_orientation_changed          (XfcePanelPlugin       *panel_plugin,
                                                             GtkOrientation         orientation);
static gboolean systray_plugin_size_changed                 (XfcePanelPlugin       *panel_plugin,
                                                             gint                   size);
static void     systray_plugin_configure_plugin             (XfcePanelPlugin       *panel_plugin);
static void     systray_plugin_icon_added                   (SystrayManager        *manager,
                                                             GtkWidget             *icon,
                                                             SystrayPlugin         *plugin);
static void     systray_plugin_icon_removed                 (SystrayManager        *manager,
                                                             GtkWidget             *icon,
                                                             SystrayPlugin         *plugin);
static void     systray_plugin_lost_selection               (SystrayManager        *manager,
                                                             SystrayPlugin         *plugin);
static void     systray_plugin_dialog_add_application_names (SystrayPlugin         *plugin,
                                                             GtkListStore          *store);
static void     systray_plugin_dialog_hidden_toggled        (GtkCellRendererToggle *renderer,
                                                             const gchar           *path_string,
                                                             SystrayPlugin         *plugin);
static void     systray_plugin_dialog_clear_clicked         (GtkWidget             *button,
                                                             SystrayPlugin         *plugin);



struct _SystrayPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _SystrayPlugin
{
  XfcePanelPlugin __parent__;

  /* systray manager */
  SystrayManager *manager;

  /* widgets */
  GtkWidget      *frame;
  GtkWidget      *box;

  /* settings */
  guint           show_frame : 1;
};

enum
{
  PROP_0,
  PROP_ROWS,
  PROP_SHOW_FRAME
};

enum
{
  COLUMN_PIXBUF,
  COLUMN_TITLE,
  COLUMN_HIDDEN,
  COLUMN_INTERNAL_NAME
};



/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (SystrayPlugin, systray_plugin,
    systray_box_register_type,
    systray_manager_register_type,
    systray_socket_register_type)



/* known applications to improve the icon and name */
static const gchar *known_applications[][3] =
{
  /* application name, icon-name, understandable name */
  { "networkmanager applet", "network-workgroup", "Network Manager Applet" },
  { "thunar", "Thunar", "Thunar Progress Dialog" },
};



static void
systray_plugin_class_init (SystrayPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = systray_plugin_get_property;
  gobject_class->set_property = systray_plugin_set_property;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = systray_plugin_construct;
  plugin_class->free_data = systray_plugin_free_data;
  plugin_class->size_changed = systray_plugin_size_changed;
  plugin_class->screen_position_changed = systray_plugin_screen_position_changed;
  plugin_class->configure_plugin = systray_plugin_configure_plugin;
  plugin_class->orientation_changed = systray_plugin_orientation_changed;

  g_object_class_install_property (gobject_class,
                                   PROP_ROWS,
                                   g_param_spec_uint ("rows",
                                                      NULL, NULL,
                                                      1, 10, 1,
                                                      EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_FRAME,
                                   g_param_spec_boolean ("show-frame",
                                                         NULL, NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));
}



static void
systray_plugin_init (SystrayPlugin *plugin)
{
  plugin->manager = NULL;
  plugin->show_frame = TRUE;

  plugin->frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (plugin), plugin->frame);
  gtk_frame_set_shadow_type (GTK_FRAME (plugin->frame), GTK_SHADOW_ETCHED_IN);
  gtk_widget_show (plugin->frame);

  plugin->box = systray_box_new ();
  gtk_container_add (GTK_CONTAINER (plugin->frame), plugin->box);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->box);
  gtk_widget_show (plugin->box);
}



static void
systray_plugin_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  SystrayPlugin *plugin = XFCE_SYSTRAY_PLUGIN (object);
  guint          rows;

  switch (prop_id)
    {
    case PROP_ROWS:
      rows = systray_box_get_rows (XFCE_SYSTRAY_BOX (plugin->box));
      g_value_set_uint (value, rows);
      break;

    case PROP_SHOW_FRAME:
      g_value_set_boolean (value, plugin->show_frame);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
systray_plugin_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  SystrayPlugin *plugin = XFCE_SYSTRAY_PLUGIN (object);
  gboolean       show_frame;

  switch (prop_id)
    {
    case PROP_ROWS:
      systray_box_set_rows (XFCE_SYSTRAY_BOX (plugin->box),
                            g_value_get_uint (value));
      break;

    case PROP_SHOW_FRAME:
      show_frame = g_value_get_boolean (value);
      if (plugin->show_frame != show_frame)
        {
          plugin->show_frame = show_frame;
          gtk_frame_set_shadow_type (GTK_FRAME (plugin->frame),
              show_frame ? GTK_SHADOW_ETCHED_IN : GTK_SHADOW_NONE);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
systray_plugin_screen_changed (GtkWidget *widget,
                               GdkScreen *previous_screen)
{
  SystrayPlugin *plugin = XFCE_SYSTRAY_PLUGIN (widget);
  GdkScreen     *screen;
  GError        *error = NULL;

  if (G_UNLIKELY (plugin->manager != NULL))
    {
      /* unregister from this screen */
      systray_manager_unregister (plugin->manager);
      g_object_unref (G_OBJECT (plugin->manager));
      plugin->manager = NULL;
    }

  /* check if not another systray is running on this screen */
  screen = gtk_widget_get_screen (widget);
  if (G_LIKELY (!systray_manager_check_running (screen)))
    {
      /* create a new manager and register this screen */
      plugin->manager = systray_manager_new ();

      /* hookup signals */
      g_signal_connect (G_OBJECT (plugin->manager), "icon-added",
          G_CALLBACK (systray_plugin_icon_added), plugin);
      g_signal_connect (G_OBJECT (plugin->manager), "icon-removed",
          G_CALLBACK (systray_plugin_icon_removed), plugin);
      g_signal_connect (G_OBJECT (plugin->manager), "lost-selection",
          G_CALLBACK (systray_plugin_lost_selection), plugin);

      if (systray_manager_register (plugin->manager, screen, &error))
        {
          /* send the plugin orientation */
          systray_plugin_orientation_changed (XFCE_PANEL_PLUGIN (plugin),
              xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)));
        }
      else
        {
          /* TODO handle error and leave the plugin */
          g_message ("Failed to register the systray manager %s", error->message);
          g_error_free (error);
        }
    }
  else
    {
      /* TODO, error and leave the plugin */
      g_message ("already a notification area running");
    }
}


#define LAUNCHER_TYPE_PTR_ARRAY        (dbus_g_type_get_collection("GPtrArray", G_TYPE_VALUE))

static void
systray_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  SystrayPlugin       *plugin = XFCE_SYSTRAY_PLUGIN (panel_plugin);
  const PanelProperty  properties[] =
  {
    { "rows", G_TYPE_UINT },
    { "show-frame", G_TYPE_BOOLEAN },
    { NULL }
  };
  const PanelProperty  box_properties[] =
  {
    { "names-visible", PANEL_PROPERTIES_TYPE_VALUE_ARRAY },
    { "names-hidden", PANEL_PROPERTIES_TYPE_VALUE_ARRAY },
    { NULL }
  };

  xfce_panel_plugin_menu_show_configure (XFCE_PANEL_PLUGIN (plugin));

  /* bind all properties */
  panel_properties_bind (NULL, G_OBJECT (plugin),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);
  panel_properties_bind (NULL, G_OBJECT (plugin->box),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         box_properties, FALSE);

  /* monitor screen changes */
  g_signal_connect (G_OBJECT (plugin), "screen-changed",
      G_CALLBACK (systray_plugin_screen_changed), NULL);
  systray_plugin_screen_changed (GTK_WIDGET (plugin), NULL);
}



static void
systray_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  SystrayPlugin *plugin = XFCE_SYSTRAY_PLUGIN (panel_plugin);

  /* disconnect screen changed signal */
  g_signal_handlers_disconnect_by_func (G_OBJECT (plugin),
      systray_plugin_screen_changed, NULL);

  if (G_LIKELY (plugin->manager != NULL))
    {
      systray_manager_unregister (plugin->manager);
      g_object_unref (G_OBJECT (plugin->manager));
    }
}



static void
systray_plugin_screen_position_changed (XfcePanelPlugin    *panel_plugin,
                                        XfceScreenPosition  screen_position)
{
  SystrayPlugin      *plugin = XFCE_SYSTRAY_PLUGIN (panel_plugin);
  GdkScreen          *screen;
  GdkRectangle        geom;
  gint                mon, x, y;
  GtkArrowType        arrow_type;

  panel_return_if_fail (GTK_WIDGET_REALIZED (panel_plugin));

  /* get the button position */
  switch (screen_position)
    {
    /*    horizontal west */
    case XFCE_SCREEN_POSITION_NW_H:
    case XFCE_SCREEN_POSITION_SW_H:
      arrow_type = GTK_ARROW_RIGHT;
      break;

    /* horizontal east */
    case XFCE_SCREEN_POSITION_N:
    case XFCE_SCREEN_POSITION_NE_H:
    case XFCE_SCREEN_POSITION_S:
    case XFCE_SCREEN_POSITION_SE_H:
      arrow_type = GTK_ARROW_LEFT;
      break;

    /* vertical north */
    case XFCE_SCREEN_POSITION_NW_V:
    case XFCE_SCREEN_POSITION_NE_V:
      arrow_type = GTK_ARROW_DOWN;
      break;

    /* vertical south */
    case XFCE_SCREEN_POSITION_W:
    case XFCE_SCREEN_POSITION_SW_V:
    case XFCE_SCREEN_POSITION_E:
    case XFCE_SCREEN_POSITION_SE_V:
      arrow_type = GTK_ARROW_UP;
      break;

    /* floating */
    default:
      /* get the screen information */
      screen = gtk_widget_get_screen (GTK_WIDGET (panel_plugin));
      mon = gdk_screen_get_monitor_at_window (screen, GTK_WIDGET (panel_plugin)->window);
      gdk_screen_get_monitor_geometry (screen, mon, &geom);
      gdk_window_get_root_origin (GTK_WIDGET (panel_plugin)->window, &x, &y);

      /* get the position based on the screen position */
      if (screen_position == XFCE_SCREEN_POSITION_FLOATING_H)
          arrow_type = ((x < (geom.x + geom.width / 2)) ? GTK_ARROW_RIGHT : GTK_ARROW_LEFT);
      else
          arrow_type = ((y < (geom.y + geom.height / 2)) ? GTK_ARROW_DOWN : GTK_ARROW_UP);
      break;
    }

  /* set the arrow type of the tray widget */
  systray_box_set_arrow_type (XFCE_SYSTRAY_BOX (plugin->box), arrow_type);

  /* update the manager orientation */
  systray_plugin_orientation_changed (panel_plugin,
      xfce_panel_plugin_get_orientation (panel_plugin));
}



static void
systray_plugin_orientation_changed (XfcePanelPlugin *panel_plugin,
                                    GtkOrientation   orientation)
{
  SystrayPlugin *plugin = XFCE_SYSTRAY_PLUGIN (panel_plugin);

  /* send the new orientation to the manager */
  if (G_LIKELY (plugin->manager != NULL))
    systray_manager_set_orientation (plugin->manager, orientation);
}



static gboolean
systray_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                             gint             size)
{
  SystrayPlugin *plugin = XFCE_SYSTRAY_PLUGIN (panel_plugin);
  GtkWidget     *frame = plugin->frame;
  gint           border = 0;

  /* set the frame border */
  if (plugin->show_frame && size > 26)
    border = 1;
  gtk_container_set_border_width (GTK_CONTAINER (frame), border);

  /* set the guess size this is used to get the initial icon size request
   * correct to avoid flickering in the system tray for new applications */
  border += MAX (frame->style->xthickness, frame->style->ythickness);
  systray_box_set_guess_size (XFCE_SYSTRAY_BOX (plugin->box), size - 2 * border);

  return TRUE;
}



static void
systray_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  SystrayPlugin *plugin = XFCE_SYSTRAY_PLUGIN (panel_plugin);
  GtkBuilder    *builder;
  GObject       *dialog, *object, *store;

  /* setup the dialog */
  PANEL_UTILS_LINK_4UI
  builder = panel_utils_builder_new (panel_plugin, systray_dialog_ui,
                                     systray_dialog_ui_length, &dialog);
  if (G_UNLIKELY (builder == NULL))
    return;

  object = gtk_builder_get_object (builder, "rows");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  exo_mutual_binding_new (G_OBJECT (plugin), "rows",
                          G_OBJECT (object), "value");

  object = gtk_builder_get_object (builder, "show-frame");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  exo_mutual_binding_new (G_OBJECT (plugin), "show-frame",
                          G_OBJECT (object), "active");

  store = gtk_builder_get_object (builder, "applications-store");
  panel_return_if_fail (GTK_IS_LIST_STORE (store));
  systray_plugin_dialog_add_application_names (plugin, GTK_LIST_STORE (store));
  g_object_set_data (G_OBJECT (plugin), "applications-store", store);

  object = gtk_builder_get_object (builder, "hidden-toggle");
  panel_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (object));
  g_signal_connect (G_OBJECT (object), "toggled",
      G_CALLBACK (systray_plugin_dialog_hidden_toggled), plugin);

  object = gtk_builder_get_object (builder, "applications-clear");
  panel_return_if_fail (GTK_IS_BUTTON (object));
  g_signal_connect (G_OBJECT (object), "clicked",
      G_CALLBACK (systray_plugin_dialog_clear_clicked), plugin);

  gtk_widget_show (GTK_WIDGET (dialog));
}



static void
systray_plugin_icon_added (SystrayManager *manager,
                           GtkWidget      *icon,
                           SystrayPlugin  *plugin)
{
  gchar *name;

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (XFCE_IS_SYSTRAY_PLUGIN (plugin));
  panel_return_if_fail (plugin->manager == manager);
  panel_return_if_fail (GTK_IS_WIDGET (icon));

  name = systray_socket_get_title (XFCE_SYSTRAY_SOCKET (icon));
  systray_box_add_with_name (XFCE_SYSTRAY_BOX (plugin->box), icon, name);
  g_free (name);

  gtk_widget_show (icon);
}



static void
systray_plugin_icon_removed (SystrayManager *manager,
                             GtkWidget      *icon,
                             SystrayPlugin  *plugin)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (XFCE_IS_SYSTRAY_PLUGIN (plugin));
  panel_return_if_fail (plugin->manager == manager);
  panel_return_if_fail (GTK_IS_WIDGET (icon));

  /* remove the icon from the box */
  gtk_container_remove (GTK_CONTAINER (plugin->box), icon);
}



static void
systray_plugin_lost_selection (SystrayManager *manager,
                               SystrayPlugin  *plugin)
{
  GError error;

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (XFCE_IS_SYSTRAY_PLUGIN (plugin));
  panel_return_if_fail (plugin->manager == manager);

  /* create fake error and show it */
  error.message = _("Most likely another widget took over the function "
                    "of a notification area. This plugin will close.");
  xfce_dialog_show_error (NULL, &error,
                          _("The notification area lost selection"));
}



static gchar *
systray_plugin_dialog_camel_case (const gchar *text)
{
  const gchar *t;
  gboolean     upper = TRUE;
  gunichar     c;
  GString     *result;

  panel_return_val_if_fail (!exo_str_is_empty (text), NULL);

  /* allocate a new string for the result */
  result = g_string_sized_new (32);

  /* convert the input text */
  for (t = text; *t != '\0'; t = g_utf8_next_char (t))
    {
      /* check the next char */
      c = g_utf8_get_char (t);
      if (g_unichar_isspace (c))
        {
          upper = TRUE;
        }
      else if (upper)
        {
          c = g_unichar_toupper (c);
          upper = FALSE;
        }
      else
        {
          c = g_unichar_tolower (c);
        }

      /* append the char to the result */
      g_string_append_unichar (result, c);
    }

  return g_string_free (result, FALSE);
}



static GdkPixbuf *
systray_plugin_dialog_icon (GtkIconTheme *icon_theme,
                            const gchar  *icon_name)
{
  GdkPixbuf   *icon = NULL;
  gchar       *first_occ;
  const gchar *p;

  panel_return_val_if_fail (!exo_str_is_empty (icon_name), NULL);
  panel_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* try to load the icon from the theme */
  icon = gtk_icon_theme_load_icon (icon_theme, icon_name, ICON_SIZE, 0, NULL);
  if (icon != NULL)
    return icon;

  /* try the first part when the name contains a space */
  p = g_utf8_strchr (icon_name, -1, ' ');
  if (p != NULL)
    {
      first_occ = g_strndup (icon_name, p - icon_name);
      icon = gtk_icon_theme_load_icon (icon_theme, first_occ, ICON_SIZE, 0, NULL);
      g_free (first_occ);
    }

  return icon;
}



static void
systray_plugin_dialog_add_application_names (SystrayPlugin *plugin,
                                             GtkListStore  *store)
{
  GList        *names, *li;
  const gchar  *name;
  guint         i;
  gboolean      hidden;
  const gchar  *title, *icon_name;
  gchar        *camelcase;
  GdkPixbuf    *icon;
  GtkIconTheme *icon_theme;
  GtkTreeIter   iter;

  panel_return_if_fail (XFCE_IS_SYSTRAY_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (plugin->box));
  panel_return_if_fail (GTK_IS_LIST_STORE (store));

  icon_theme = gtk_icon_theme_get_default ();

  /* get the know application names and insert them in the store */
  names = systray_box_name_list (XFCE_SYSTRAY_BOX (plugin->box));
  for (li = names; li != NULL; li = li->next)
    {
      name = li->data;

      /* skip invalid names */
      if (exo_str_is_empty (name))
        continue;

      title = NULL;
      icon_name = name;
      camelcase = NULL;
      hidden = systray_box_name_get_hidden (XFCE_SYSTRAY_BOX (plugin->box), name);

      /* check if we have a better name for the application */
      for (i = 0; i < G_N_ELEMENTS (known_applications); i++)
        {
          if (strcmp (name, known_applications[i][0]) == 0)
            {
              icon_name = known_applications[i][1];
              title = _(known_applications[i][2]);
              break;
            }
        }

      /* create fallback title if the application was not found */
      if (title == NULL)
        {
          camelcase = systray_plugin_dialog_camel_case (name);
          title = camelcase;
        }

      /* try to load the icon name */
      icon = systray_plugin_dialog_icon (icon_theme, icon_name);

      /* insert in the store */
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_PIXBUF, icon,
                          COLUMN_TITLE, title,
                          COLUMN_HIDDEN, hidden,
                          COLUMN_INTERNAL_NAME, name,
                          -1);

      g_free (camelcase);
      if (icon != NULL)
        g_object_unref (G_OBJECT (icon));
    }

  g_list_free (names);
}



static void
systray_plugin_dialog_hidden_toggled (GtkCellRendererToggle *renderer,
                                      const gchar           *path_string,
                                      SystrayPlugin         *plugin)
{
  GtkTreeIter   iter;
  gboolean      hidden;
  GtkTreeModel *model;
  gchar        *name;

  panel_return_if_fail (XFCE_IS_SYSTRAY_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (plugin->box));

  model = g_object_get_data (G_OBJECT (plugin), "applications-store");
  panel_return_if_fail (GTK_IS_LIST_STORE (model));
  if (gtk_tree_model_get_iter_from_string (model, &iter, path_string))
    {
      gtk_tree_model_get (model, &iter,
                          COLUMN_HIDDEN, &hidden,
                          COLUMN_INTERNAL_NAME, &name, -1);

      /* update box and store with new state */
      systray_box_name_set_hidden (XFCE_SYSTRAY_BOX (plugin->box), name, !hidden);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, 2, !hidden, -1);

      g_free (name);
    }
}



static void
systray_plugin_dialog_clear_clicked (GtkWidget     *button,
                                     SystrayPlugin *plugin)
{
  GtkListStore *store;

  panel_return_if_fail (XFCE_IS_SYSTRAY_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (plugin->box));

  if (xfce_dialog_confirm (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                           GTK_STOCK_CLEAR, NULL, NULL,
                           _("Are you sure you want to clear the list of "
                             "known applications?")))
    {
      store = g_object_get_data (G_OBJECT (plugin), "applications-store");
      panel_return_if_fail (GTK_IS_LIST_STORE (store));
      gtk_list_store_clear (store);

      systray_box_name_clear (XFCE_SYSTRAY_BOX (plugin->box));
    }
}