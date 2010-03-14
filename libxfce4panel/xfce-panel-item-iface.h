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

#ifndef __XFCE_PANEL_ITEM_IFACE_H__
#define __XFCE_PANEL_ITEM_IFACE_H__

#include <glib-object.h>
#include <libxfce4panel/xfce-panel-enums.h>

G_BEGIN_DECLS

typedef struct _XfcePanelItem          XfcePanelItem;
typedef struct _XfcePanelItemInterface XfcePanelItemInterface;

#define XFCE_TYPE_PANEL_ITEM                (xfce_panel_item_get_type ())
#define XFCE_PANEL_ITEM(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_PANEL_ITEM, XfcePanelItem))
#define XFCE_IS_PANEL_ITEM(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_PANEL_ITEM))
#define XFCE_PANEL_ITEM_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), XFCE_TYPE_PANEL_ITEM, XfcePanelItemInterface))

struct _XfcePanelItemInterface
{
    GTypeInterface parent;

    /* signals */
    const gchar *(*get_name)            (XfcePanelItem      *item);
    const gchar *(*get_id)              (XfcePanelItem      *item);
    const gchar *(*get_display_name)    (XfcePanelItem      *item);
    gboolean     (*get_expand)          (XfcePanelItem      *item);
    void         (*free_data)           (XfcePanelItem      *item);
    void         (*save)                (XfcePanelItem      *item);
    void         (*set_size)            (XfcePanelItem      *item,
                                         gint                size);
    void         (*set_screen_position) (XfcePanelItem      *item,
                                         XfceScreenPosition  position);
    void         (*set_sensitive)       (XfcePanelItem      *item,
                                         gboolean            sensitive);
    void         (*remove)              (XfcePanelItem      *item);
    void         (*configure)           (XfcePanelItem      *item);

    /* reserved for future expansion */
    void (*reserved1) (void);
    void (*reserved2) (void);
    void (*reserved3) (void);
};

GType         xfce_panel_item_get_type             (void) G_GNUC_CONST;
void          xfce_panel_item_focus_panel          (XfcePanelItem       *item);
void          xfce_panel_item_expand_changed       (XfcePanelItem       *item,
                                                    gboolean             expand);
void          xfce_panel_item_menu_deactivated     (XfcePanelItem       *item);
void          xfce_panel_item_menu_opened          (XfcePanelItem       *item);
void          xfce_panel_item_customize_panel      (XfcePanelItem       *item);
void          xfce_panel_item_customize_items      (XfcePanelItem       *item);
void          xfce_panel_item_move                 (XfcePanelItem       *item);
void          xfce_panel_item_set_panel_hidden     (XfcePanelItem       *item,
                                                    gboolean             hidden);
const gchar  *xfce_panel_item_get_name             (XfcePanelItem       *item);
const gchar  *xfce_panel_item_get_id               (XfcePanelItem       *item);
const gchar  *xfce_panel_item_get_display_name     (XfcePanelItem       *item);
gboolean      xfce_panel_item_get_expand           (XfcePanelItem       *item);
void          xfce_panel_item_save                 (XfcePanelItem       *item);
void          xfce_panel_item_free_data            (XfcePanelItem       *item);
void          xfce_panel_item_set_size             (XfcePanelItem       *item,
                                                    gint                 size);
void          xfce_panel_item_set_screen_position  (XfcePanelItem       *item,
                                                    XfceScreenPosition   position);
void          xfce_panel_item_set_sensitive        (XfcePanelItem       *item,
                                                    gboolean             sensitive);
void          xfce_panel_item_remove               (XfcePanelItem       *item);
void          xfce_panel_item_configure            (XfcePanelItem       *item);

G_END_DECLS

#endif /* !__XFCE_PANEL_ITEM_IFACE_H__ */