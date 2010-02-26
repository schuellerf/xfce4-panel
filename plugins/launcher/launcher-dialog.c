/* $Id$ */
/*
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo/exo.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4menu/libxfce4menu.h>
#include <libxfce4util/libxfce4util.h>
#include <common/panel-private.h>

#include "launcher.h"
#include "launcher-dialog.h"
#include "launcher-dialog_glade.h"



typedef struct
{
  LauncherPlugin *plugin;
  GtkBuilder     *builder;
  guint           idle_populate_id;
  XfconfChannel  *channel;
}
LauncherPluginDialog;

enum
{
  COL_ICON,
  COL_NAME,
  COL_FILENAME,
  COL_SEARCH
};



static void launcher_dialog_entries_insert_item (GtkListStore *store, GtkTreeIter *iter, const gchar *filename);
static void launcher_dialog_entries_changed (XfconfChannel *channel, const gchar *property_name, const GValue *value, LauncherPluginDialog *dialog);



static gboolean
launcher_dialog_add_visible_function (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      gpointer      user_data)
{
  gchar       *string, *escaped;
  gboolean     visible = FALSE;
  const gchar *text;
  gchar       *normalized;
  gchar       *text_casefolded;
  gchar       *name_casefolded;

  /* get the search string from the entry */
  text = gtk_entry_get_text (GTK_ENTRY (user_data));
  if (G_UNLIKELY (!IS_STRING (text)))
    return TRUE;

  /* casefold the search text */
  normalized = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
  text_casefolded = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  /* try the pre-build search string first */
  gtk_tree_model_get (model, iter, COL_SEARCH, &string, -1);
  if (IS_STRING (string))
    {
      /* search */
      visible = (strstr (string, text_casefolded) != NULL);
    }
  else
    {
      /* get the name */
      gtk_tree_model_get (model, iter, COL_NAME, &string, -1);
      if (IS_STRING (string))
        {
          /* escape and casefold the name */
          escaped = g_markup_escape_text (string, -1);
          normalized = g_utf8_normalize (escaped, -1, G_NORMALIZE_ALL);
          name_casefolded = g_utf8_casefold (normalized, -1);
          g_free (normalized);
          g_free (escaped);

          /* search */
          visible = (strstr (name_casefolded, text_casefolded) != NULL);

          /* store the generated search string */
          gtk_list_store_set (GTK_LIST_STORE (model), iter, COL_SEARCH,
                              name_casefolded, -1);

          /* cleanup */
          g_free (name_casefolded);
        }
    }

  /* cleanup */
  g_free (text_casefolded);
  g_free (string);

  return visible;
}



static void
launcher_dialog_add_store_insert (gpointer filename,
                                  gpointer item,
                                  gpointer user_data)
{
  GtkListStore *store = GTK_LIST_STORE (user_data);
  GtkTreeIter   iter;
  const gchar  *icon_name, *name, *comment;
  gchar        *markup;

  panel_return_if_fail (XFCE_IS_MENU_ITEM (item));
  panel_return_if_fail (GTK_IS_LIST_STORE (user_data));
  panel_return_if_fail (exo_str_is_equal (xfce_menu_item_get_filename (item),
                        (gchar *) filename));

  /* TODO get rid of this and support absolute paths too */
  icon_name = xfce_menu_item_get_icon_name (item);
  if (icon_name != NULL
      && (g_path_is_absolute (icon_name)
          || !gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), icon_name)))
    icon_name = NULL;

  /* create a good looking name */
  comment = xfce_menu_item_get_comment (item);
  name = xfce_menu_item_get_name (item);
  if (IS_STRING (comment))
    markup = g_strdup_printf ("<b>%s</b>\n%s", name, comment);
  else
    markup = g_strdup_printf ("<b>%s</b>", name);

  /* insert the item */
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      COL_ICON, icon_name,
                      COL_NAME, markup,
                      COL_FILENAME, (gchar *) filename,
                      -1);

  g_free (markup);
}



static gboolean
launcher_dialog_add_populate_model_idle (gpointer user_data)
{
  LauncherPluginDialog *dialog = user_data;
  XfceMenu             *root;
  GError               *error = NULL;
  GObject              *store;
  XfceMenuItemCache    *cache;

  panel_return_val_if_fail (GTK_IS_BUILDER (dialog->builder), FALSE);

  /* initialize the menu library */
  xfce_menu_init (NULL);

  GDK_THREADS_ENTER ();

  /* get the root menu */
  root = xfce_menu_get_root (&error);
  if (G_LIKELY (root != NULL))
    {
      /* start appending items in the store */
      store = gtk_builder_get_object (dialog->builder, "add-store");

      /* get the item cache and insert everything in the store */
      cache = xfce_menu_item_cache_get_default ();
      xfce_menu_item_cache_foreach (cache,
          launcher_dialog_add_store_insert, store);

      /* release the root menu and cache */
      g_object_unref (G_OBJECT (root));
      g_object_unref (G_OBJECT (cache));
    }
  else
    {
      /* TODO make this a warning dialog or something */
      g_message ("Failed to load the root menu: %s", error->message);
      g_error_free (error);
    }

  GDK_THREADS_LEAVE ();

  /* shutdown menu library */
  xfce_menu_shutdown ();

  return FALSE;
}



static void
launcher_dialog_add_populate_model_idle_destroyed (gpointer user_data)
{
  ((LauncherPluginDialog *) user_data)->idle_populate_id = 0;
}



static void
launcher_dialog_add_populate_model (LauncherPluginDialog *dialog)
{
  GObject *store;

  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  /* get the store and make sure it's empty */
  store = gtk_builder_get_object (dialog->builder, "add-store");
  gtk_list_store_clear (GTK_LIST_STORE (store));

  /* fire an idle menu system load */
  if (G_LIKELY (dialog->idle_populate_id == 0))
    dialog->idle_populate_id = g_idle_add_full (
        G_PRIORITY_DEFAULT_IDLE,
        launcher_dialog_add_populate_model_idle,
        dialog, launcher_dialog_add_populate_model_idle_destroyed);
}



static gboolean
launcher_dialog_tree_save_foreach (GtkTreeModel *model,
                                   GtkTreePath  *path,
                                   GtkTreeIter  *iter,
                                   gpointer      user_data)
{
  GPtrArray *array = user_data;
  GValue    *value;
  gchar     *filename;

  /* get the filename of the entry from the store */
  gtk_tree_model_get (model, iter, COL_FILENAME, &filename, -1);

  /* create a value with the filename */
  value = g_new0 (GValue, 1);
  g_value_init (value, G_TYPE_STRING);
  g_value_take_string (value, filename);

  /* put it in the array */
  g_ptr_array_add (array, value);

  return FALSE;
}



static void
launcher_dialog_tree_save (LauncherPluginDialog *dialog)
{
  GObject   *store;
  GPtrArray *array;

  store = gtk_builder_get_object (dialog->builder, "entry-store");

  array = g_ptr_array_new ();
  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          launcher_dialog_tree_save_foreach, array);
  xfconf_channel_set_arrayv (dialog->channel, "/entries", array);
  xfconf_array_free (array);
}



static void
launcher_dialog_tree_selection_changed (GtkTreeSelection     *selection,
                                        LauncherPluginDialog *dialog)
{
  GObject      *object;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gint          n_children = -1, position = 0;
  GtkTreePath  *path;
  gboolean      sensitive;

  panel_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* get the number of launchers in the tree */
      n_children = gtk_tree_model_iter_n_children (model, NULL);

      /* get the position of the selected item in the tree */
      path = gtk_tree_model_get_path (model, &iter);
      position = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);
    }

  /* update the sensitivity of the buttons */
  object = gtk_builder_get_object (dialog->builder, "entry-remove");
  gtk_widget_set_sensitive (GTK_WIDGET (object), !!(n_children > 0));

  object = gtk_builder_get_object (dialog->builder, "entry-move-up");
  sensitive = !!(position > 0 && position <= n_children);
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);

  object = gtk_builder_get_object (dialog->builder, "entry-move-down");
  sensitive = !!(position >= 0 && position < n_children - 1);
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);

  object = gtk_builder_get_object (dialog->builder, "entry-edit");
  sensitive = !!(position >= 0 && n_children > 0); /* TODO custom entries only */
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);
}



static void
launcher_dialog_entry_button_clicked (GtkWidget            *button,
                                      LauncherPluginDialog *dialog)
{
  const gchar      *name;
  GObject          *object;
  GtkWidget        *window;
  GObject          *treeview;
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter_a, iter_b;
  GtkTreePath      *path;

  panel_return_if_fail (GTK_IS_BUILDABLE (button));
  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  /* name of the button */
  name = gtk_buildable_get_name (GTK_BUILDABLE (button));
  if (exo_str_is_equal (name, "entry-add"))
    {
      object = gtk_builder_get_object (dialog->builder, "dialog-add");
      launcher_dialog_add_populate_model (dialog);
      gtk_widget_show (GTK_WIDGET (object));
    }
  else
    {
      /* get the selected item in the tree, leave if none is found */
      treeview = gtk_builder_get_object (dialog->builder, "entry-treeview");
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
      if (!gtk_tree_selection_get_selected (selection, &model, &iter_a))
        return;

      if (exo_str_is_equal (name, "entry-remove"))
        {
          /* create question dialog */
          window = gtk_message_dialog_new (
              GTK_WINDOW (gtk_widget_get_toplevel (button)),
              GTK_DIALOG_DESTROY_WITH_PARENT,
              GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
              _("Are you sure you want to remove\nthe selected item?"));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (window),
              _("If you delete an item, it is permanently removed from the launcher."));
          gtk_dialog_add_buttons (GTK_DIALOG (window),
                                  GTK_STOCK_REMOVE, GTK_RESPONSE_ACCEPT,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  NULL);
          gtk_dialog_set_default_response (GTK_DIALOG (window), GTK_RESPONSE_ACCEPT);

          /* run the dialog */
          if (gtk_dialog_run (GTK_DIALOG (window)) == GTK_RESPONSE_ACCEPT)
            gtk_list_store_remove (GTK_LIST_STORE (model), &iter_a);

          /* destroy */
          gtk_widget_destroy (window);
        }
      else if (exo_str_is_equal (name, "entry-edit"))
        {
          object = gtk_builder_get_object (dialog->builder, "dialog-editor");
          gtk_widget_show (GTK_WIDGET (object));
        }
      else if (exo_str_is_equal (name, "entry-move-up"))
        {
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter_a);
          if (gtk_tree_path_prev (path)
              && gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter_b, path))
            gtk_list_store_swap (GTK_LIST_STORE (model), &iter_a, &iter_b);
          gtk_tree_path_free (path);
        }
      else
        {
          panel_return_if_fail (exo_str_is_equal (name, "entry-move-down"));
          iter_b = iter_a;
          if (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter_b))
            gtk_list_store_swap (GTK_LIST_STORE (model), &iter_a, &iter_b);
        }

      /* store the new settings */
      launcher_dialog_tree_save (dialog);

      /* emit a changed signal to update the button states */
      launcher_dialog_tree_selection_changed (selection, dialog);
    }
}



static void
launcher_dialog_response (GtkWidget            *widget,
                          gint                  response_id,
                          LauncherPluginDialog *dialog)
{
  panel_return_if_fail (GTK_IS_DIALOG (widget));
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (dialog->plugin));
  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  if (G_UNLIKELY (response_id == 1))
    {
      /* TODO open help */
    }
  else
    {
      /* stop idle if still running */
      if (G_UNLIKELY (dialog->idle_populate_id != 0))
        g_source_remove (dialog->idle_populate_id);

      /* destroy the dialog and release the builder */
      gtk_widget_destroy (widget);
      g_object_unref (G_OBJECT (dialog->builder));

      /* disconnect signal */
      g_signal_handlers_disconnect_by_func (G_OBJECT (dialog->channel),
                                            launcher_dialog_entries_changed,
                                            dialog);

      /* unblock plugin menu */
      xfce_panel_plugin_unblock_menu (XFCE_PANEL_PLUGIN (dialog->plugin));

      /* cleanup */
      g_slice_free (LauncherPluginDialog, dialog);
    }
}



static void
launcher_dialog_editor_response (GtkWidget            *widget,
                                 gint                  response_id,
                                 LauncherPluginDialog *dialog)
{
  panel_return_if_fail (GTK_IS_DIALOG (widget));
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (dialog->plugin));

  /* hide the dialog, since it's owned by gtkbuilder */
  gtk_widget_hide (widget);
}



static void
launcher_dialog_add_response (GtkWidget            *widget,
                              gint                  response_id,
                              LauncherPluginDialog *dialog)
{
  GObject           *treeview, *store;
  GtkTreeSelection  *selection;
  GtkTreeModel      *model;
  GtkTreeIter        iter, sibling;
  gchar             *filename;

  panel_return_if_fail (GTK_IS_DIALOG (widget));
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (dialog->plugin));

  if (response_id != 0)
    {
      /* set the selected item in the treeview */
      treeview = gtk_builder_get_object (dialog->builder, "add-treeview");
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          /* get the selected file in the add dialog */
          gtk_tree_model_get (model, &iter, COL_FILENAME, &filename, -1);

          /* get the selected item in the entry treeview */
          treeview = gtk_builder_get_object (dialog->builder, "entry-treeview");
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
          if (gtk_tree_selection_get_selected (selection, &model, &sibling))
            {
              gtk_list_store_insert_after (GTK_LIST_STORE (model), &iter, &sibling);
            }
          else
            {
              model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
              gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            }

          /* insert the item */
          launcher_dialog_entries_insert_item (GTK_LIST_STORE (model),
                                               &iter, filename);

          /* cleanup */
          g_free (filename);

          /* write the model to xfconf */
          launcher_dialog_tree_save (dialog);

          /* update the selection */
          launcher_dialog_tree_selection_changed (selection, dialog);
        }
    }

  /* empty the store */
  store = gtk_builder_get_object (dialog->builder, "add-store");
  gtk_list_store_clear (GTK_LIST_STORE (store));

  /* hide the dialog, since it's owned by gtkbuilder */
  gtk_widget_hide (widget);
}



static void
launcher_dialog_entries_insert_item (GtkListStore *store,
                                     GtkTreeIter  *iter,
                                     const gchar  *filename)
{
  XfceRc      *rc;
  const gchar *icon, *name, *comment;
  gchar       *markup;

  panel_return_if_fail (GTK_IS_LIST_STORE (store));
  panel_return_if_fail (IS_STRING (filename));

  rc = xfce_rc_simple_open (filename, TRUE);
  if (G_LIKELY (rc != NULL))
    {
      xfce_rc_set_group (rc, "Desktop Entry");
      icon = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);
      name = xfce_rc_read_entry (rc, "Name", NULL);
      comment = xfce_rc_read_entry (rc, "Comment", NULL);

      if (IS_STRING (comment))
        markup = g_strdup_printf ("<b>%s</b>\n%s", name, comment);
      else
        markup = g_strdup_printf ("<b>%s</b>", name);

      gtk_list_store_set (GTK_LIST_STORE (store), iter,
                          COL_ICON, icon,
                          COL_NAME, markup,
                          COL_FILENAME, filename,
                          -1);
      xfce_rc_close (rc);
      g_free (markup);
    }
}



static void
launcher_dialog_entries_changed (XfconfChannel        *channel,
                                 const gchar          *property_name,
                                 const GValue         *value,
                                 LauncherPluginDialog *dialog)
{
  gchar       **filenames;
  gchar        *filename;
  guint         i;
  GObject      *store;
  GtkTreeIter   iter;
  gboolean      update = FALSE;
  gint          n_children;

  /* only handle something when the entries changes */
  if (!exo_str_is_equal (property_name, "/entries"))
    return;

  /* get the store and clear it */
  store = gtk_builder_get_object (dialog->builder, "entry-store");

  filenames = xfconf_channel_get_string_list (channel, "/entries");
  if (G_LIKELY (filenames != NULL))
    {
      /* compare if the number of items is different */
      n_children = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL);
      update = g_strv_length (filenames) != (guint) n_children;

      /* if not, compare the model and the array if there are differences */
      if (!update && gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
        {
          for (i = 0; filenames[i] != NULL; i++)
            {
              gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                                  COL_FILENAME, &filename, -1);
              update = !exo_str_is_equal (filenames[i], filename);
              if (G_UNLIKELY (update))
                break;

              if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter))
                break;
            }
        }

      if (update == TRUE)
        {
          gtk_list_store_clear (GTK_LIST_STORE (store));
          for (i = 0; update && filenames[i] != NULL; i++)
            {
              gtk_list_store_append (GTK_LIST_STORE (store), &iter);
              launcher_dialog_entries_insert_item (GTK_LIST_STORE (store),
                                                   &iter, filenames[i]);
            }
        }

      g_strfreev (filenames);
    }
  else if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) > 0)
    {
      /* model is not empty but the channel is */
      gtk_list_store_clear (GTK_LIST_STORE (store));
    }
}



void
launcher_dialog_show (LauncherPlugin *plugin)
{
  GtkBuilder           *builder;
  GObject              *window, *object, *entry;
  guint                 i;
  GtkTreeSelection     *selection;
  const gchar          *entry_names[] = { "entry-add", "entry-remove",
                                          "entry-move-up", "entry-move-down",
                                          "entry-edit" };
  LauncherPluginDialog *dialog;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  builder = gtk_builder_new ();
  if (gtk_builder_add_from_string (builder, launcher_dialog_glade,
                                   launcher_dialog_glade_length, NULL))
    {
      /* create structure */
      dialog = g_slice_new0 (LauncherPluginDialog);
      dialog->builder = builder;
      dialog->plugin = plugin;
      dialog->channel = launcher_plugin_get_channel (plugin);

      /* monitor the channel for any changes */
      g_signal_connect (G_OBJECT (dialog->channel), "property-changed",
                        G_CALLBACK (launcher_dialog_entries_changed), dialog);

      /* block plugin menu */
      xfce_panel_plugin_block_menu (XFCE_PANEL_PLUGIN (plugin));

      /* get dialog from builder, release builder when dialog is destroyed */
      window = gtk_builder_get_object (builder, "dialog");
      xfce_panel_plugin_take_window (XFCE_PANEL_PLUGIN (plugin), GTK_WINDOW (window));
      g_signal_connect (G_OBJECT (window), "response",
                        G_CALLBACK (launcher_dialog_response), dialog);

      /* connect entry buttons */
      for (i = 0; i < G_N_ELEMENTS (entry_names); i++)
        {
          object = gtk_builder_get_object (builder, entry_names[i]);
          g_signal_connect (G_OBJECT (object), "clicked",
                            G_CALLBACK (launcher_dialog_entry_button_clicked), dialog);
        }

      /* setup treeview selection */
      object = gtk_builder_get_object (builder, "entry-treeview");
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
      gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
      g_signal_connect (G_OBJECT (selection), "changed",
                        G_CALLBACK (launcher_dialog_tree_selection_changed), dialog);
      launcher_dialog_tree_selection_changed (selection, dialog);

      /* connect binding to the advanced properties */
      object = gtk_builder_get_object (builder, "disable-tooltips");
      xfconf_g_property_bind (dialog->channel, "/disable-tooltips",
                              G_TYPE_BOOLEAN, object, "active");

      object = gtk_builder_get_object (builder, "show-labels");
      xfconf_g_property_bind (dialog->channel, "/show-labels",
                              G_TYPE_BOOLEAN, object, "active");

      object = gtk_builder_get_object (builder, "move-first");
      xfconf_g_property_bind (dialog->channel, "/move-first",
                              G_TYPE_BOOLEAN, object, "active");

      object = gtk_builder_get_object (builder, "arrow-position");
      xfconf_g_property_bind (dialog->channel, "/arrow-position",
                              G_TYPE_UINT, object, "active");

      /* setup responses for the other dialogs */
      object = gtk_builder_get_object (builder, "dialog-editor");
      g_signal_connect (G_OBJECT (object), "response",
                        G_CALLBACK (launcher_dialog_editor_response), dialog);
      g_signal_connect (G_OBJECT (object), "delete-event",
                        G_CALLBACK (exo_noop_true), NULL);

      object = gtk_builder_get_object (builder, "dialog-add");
      g_signal_connect (G_OBJECT (object), "response",
                        G_CALLBACK (launcher_dialog_add_response), dialog);
      g_signal_connect (G_OBJECT (object), "delete-event",
                        G_CALLBACK (exo_noop_true), NULL);

      object = gtk_builder_get_object (builder, "add-store");
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (object),
                                            COL_NAME, GTK_SORT_ASCENDING);

      /* setup search filter in the add dialog */
      object = gtk_builder_get_object (builder, "add-store-filter");
      entry = gtk_builder_get_object (builder, "add-search");
      gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (object),
          launcher_dialog_add_visible_function, entry, NULL);
      g_signal_connect_swapped (G_OBJECT (entry), "changed",
                                G_CALLBACK (gtk_tree_model_filter_refilter), object);

      /* setup the icon size in the icon renderers */
      object = gtk_builder_get_object (builder, "addrenderericon");
      g_object_set (G_OBJECT (object), "stock-size", GTK_ICON_SIZE_DND, NULL);
      object = gtk_builder_get_object (builder, "entryrenderericon");
      g_object_set (G_OBJECT (object), "stock-size", GTK_ICON_SIZE_DND, NULL);

      /* load the launchers */
      launcher_dialog_entries_changed (dialog->channel, "/entries", NULL, dialog);

      /* show the dialog */
      gtk_widget_show (GTK_WIDGET (window));
    }
  else
    {
      /* release the builder and fire error */
      g_object_unref (G_OBJECT (builder));
      panel_assert_not_reached ();
    }
}
