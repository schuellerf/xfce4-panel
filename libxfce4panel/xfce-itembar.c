/* $Id$
 *
 * Copyright (c) 2004-2007 Jasper Huijsmans <jasper@xfce.org>
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
 *
 * Partly based on code from GTK+. Copyright (c) 1997-2000 The GTK+ Team
 * licensed under the LGPL.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libxfce4panel/libxfce4panel-marshal.h>
#include <libxfce4panel/libxfce4panel-enum-types.h>
#include <libxfce4panel/xfce-itembar.h>
#include <libxfce4panel/xfce-panel-macros.h>
#include <libxfce4panel/libxfce4panel-alias.h>

#define XFCE_ITEMBAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), XFCE_TYPE_ITEMBAR, XfceItembarPrivate))

#define DEFAULT_ORIENTATION     GTK_ORIENTATION_HORIZONTAL
#define DEFAULT_CHILD_EXPAND    FALSE

enum
{
    ORIENTATION_CHANGED,
    CONTENTS_CHANGED,
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_ORIENTATION
};

enum
{
    CHILD_PROP_0,
    CHILD_PROP_EXPAND
};



typedef struct _XfceItembarChild   XfceItembarChild;

struct _XfceItembarPrivate
{
    GtkOrientation  orientation;
    GSList         *children;

    GdkWindow      *event_window;
    GdkWindow      *drag_highlight;

    gint            drop_index;
    guint           raised : 1;
    guint           expand_allowed : 1;
    gint            maximum_size;
};

struct _XfceItembarChild
{
    GtkWidget     *widget;
    guint          expand : 1;
    GtkAllocation  allocation;
};



static void      xfce_itembar_class_init          (XfceItembarClass  *klass);
static void      xfce_itembar_init                (XfceItembar       *itembar);
static void      xfce_itembar_finalize            (GObject           *object);
static void      xfce_itembar_set_property        (GObject           *object,
                                                   guint              prop_id,
                                                   const GValue      *value,
                                                   GParamSpec        *pspec);
static void      xfce_itembar_get_property        (GObject           *object,
                                                   guint              prop_id,
                                                   GValue            *value,
                                                   GParamSpec        *pspec);
static gint      xfce_itembar_expose              (GtkWidget         *widget,
                                                   GdkEventExpose    *event);
static void      xfce_itembar_size_request        (GtkWidget         *widget,
                                                   GtkRequisition    *requisition);
static void      xfce_itembar_size_allocate       (GtkWidget         *widget,
                                                   GtkAllocation     *allocation);
static void      xfce_itembar_realize             (GtkWidget         *widget);
static void      xfce_itembar_unrealize           (GtkWidget         *widget);
static void      xfce_itembar_map                 (GtkWidget         *widget);
static void      xfce_itembar_unmap               (GtkWidget         *widget);
static void      xfce_itembar_drag_leave          (GtkWidget         *widget,
                                                   GdkDragContext    *context,
                                                   guint              time_);
static gboolean  xfce_itembar_drag_motion         (GtkWidget         *widget,
                                                   GdkDragContext    *context,
                                                   gint               x,
                                                   gint               y,
                                                   guint              time_);
static void      xfce_itembar_forall              (GtkContainer      *container,
                                                   gboolean           include_internals,
                                                   GtkCallback        callback,
                                                   gpointer           callback_data);
static GType     xfce_itembar_child_type          (GtkContainer      *container);
static void      xfce_itembar_add                 (GtkContainer      *container,
                                                   GtkWidget         *child);
static void      xfce_itembar_remove              (GtkContainer      *container,
                                                   GtkWidget         *child);
static void      xfce_itembar_set_child_property  (GtkContainer      *container,
                                                   GtkWidget         *child,
                                                   guint              prop_id,
                                                   const GValue      *value,
                                                   GParamSpec        *pspec);
static void      xfce_itembar_get_child_property  (GtkContainer      *container,
                                                   GtkWidget         *child,
                                                   guint              prop_id,
                                                   GValue            *value,
                                                   GParamSpec        *pspec);
static gint      _find_drop_index                 (XfceItembar       *itembar,
                                                   gint               x,
                                                   gint               y);

/* global vars */
static GtkContainerClass *parent_class = NULL;
static guint              itembar_signals[LAST_SIGNAL] = { 0 };



/* public GType function */
GType
xfce_itembar_get_type (void)
{
  static GtkType type = G_TYPE_INVALID;

    if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
        type = _panel_g_type_register_simple (GTK_TYPE_CONTAINER,
                                              "XfceItembar",
                                              sizeof (XfceItembarClass),
                                              xfce_itembar_class_init,
                                              sizeof (XfceItembar),
                                              xfce_itembar_init);
    }

    return type;
}



/* init */
static void
xfce_itembar_class_init (XfceItembarClass *klass)
{
    GObjectClass      *gobject_class;
    GtkWidgetClass    *widget_class;
    GtkContainerClass *container_class;

    g_type_class_add_private (klass, sizeof (XfceItembarPrivate));

    parent_class = g_type_class_peek_parent (klass);

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize     = xfce_itembar_finalize;
    gobject_class->get_property = xfce_itembar_get_property;
    gobject_class->set_property = xfce_itembar_set_property;

    widget_class = GTK_WIDGET_CLASS (klass);
    widget_class->expose_event   = xfce_itembar_expose;
    widget_class->size_request   = xfce_itembar_size_request;
    widget_class->size_allocate  = xfce_itembar_size_allocate;
    widget_class->realize        = xfce_itembar_realize;
    widget_class->unrealize      = xfce_itembar_unrealize;
    widget_class->map            = xfce_itembar_map;
    widget_class->unmap          = xfce_itembar_unmap;
    widget_class->drag_leave     = xfce_itembar_drag_leave;
    widget_class->drag_motion    = xfce_itembar_drag_motion;

    container_class = GTK_CONTAINER_CLASS (klass);
    container_class->forall             = xfce_itembar_forall;
    container_class->child_type         = xfce_itembar_child_type;
    container_class->add                = xfce_itembar_add;
    container_class->remove             = xfce_itembar_remove;
    container_class->get_child_property = xfce_itembar_get_child_property;
    container_class->set_child_property = xfce_itembar_set_child_property;

    /* signals */

    /**
     * XfceItembar::orientation-changed:
     * @itembar     : the object which emitted the signal
     * @orientation : the new #GtkOrientation of the itembar
     *
     * Emitted when the orientation of the itembar changes.
     **/
    itembar_signals[ORIENTATION_CHANGED] =
        g_signal_new (I_("orientation-changed"),
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (XfceItembarClass, orientation_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__ENUM,
                      G_TYPE_NONE, 1, GTK_TYPE_ORIENTATION);

    /**
     * XfceItembar::contents-changed:
     * @itembar     : the object which emitted the signal
     *
     * Emitted when the contents of the itembar change, either by adding
     * a child, removing a child, or reordering a child.
     **/
    itembar_signals[CONTENTS_CHANGED] =
        g_signal_new (I_("contents-changed"),
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (XfceItembarClass, contents_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    /* properties */

    /**
     * XfceItembar:orientation
     *
     * The orientation of the #XfceItembar.
     **/
    g_object_class_install_property (gobject_class,
                                     PROP_ORIENTATION,
                                     g_param_spec_enum ("orientation",
                                                        "Orientation",
                                                        "The orientation of the itembar",
                                                        GTK_TYPE_ORIENTATION,
                                                        DEFAULT_ORIENTATION,
                                                        PANEL_PARAM_READWRITE));

    /* child properties */

    /**
     * XfceItembar:expand
     *
     * Whether the child of the #XfceItembar should fill available space.
     **/
    gtk_container_class_install_child_property (container_class,
                                                CHILD_PROP_EXPAND,
                                                g_param_spec_boolean ("expand",
                                                                      "Expand",
                                                                      "Whether to grow with parent",
                                                                       DEFAULT_CHILD_EXPAND,
                                                                       PANEL_PARAM_READWRITE));
}



static void
xfce_itembar_init (XfceItembar *itembar)
{
    XfceItembarPrivate *priv;

    /* set the private pointer */
    itembar->priv = XFCE_ITEMBAR_GET_PRIVATE (itembar);
    priv = itembar->priv;

    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (itembar), GTK_NO_WINDOW);
    gtk_widget_set_redraw_on_allocate (GTK_WIDGET (itembar), FALSE);

    priv->orientation    = DEFAULT_ORIENTATION;
    priv->children       = NULL;
    priv->event_window   = NULL;
    priv->drag_highlight = NULL;
    priv->drop_index     = -1;
    priv->raised         = FALSE;
    priv->expand_allowed = FALSE;
    priv->maximum_size   = -1;
}



/* GObject */
static void
xfce_itembar_finalize (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->finalize (object);
}



static void
xfce_itembar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    XfceItembar *itembar = XFCE_ITEMBAR (object);

    switch (prop_id)
    {
        case PROP_ORIENTATION:
            xfce_itembar_set_orientation (itembar, g_value_get_enum (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
xfce_itembar_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    XfceItembar *itembar = XFCE_ITEMBAR (object);

    switch (prop_id)
    {
        case PROP_ORIENTATION:
            g_value_set_enum (value, xfce_itembar_get_orientation (itembar));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



/* GtkWidget */
static gint
xfce_itembar_expose (GtkWidget      *widget,
                     GdkEventExpose *event)
{
    XfceItembar *itembar = XFCE_ITEMBAR (widget);

    GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

    if (itembar->priv->raised)
        gdk_window_raise (itembar->priv->event_window);

    return TRUE;
}



static void
xfce_itembar_size_request (GtkWidget      *widget,
                           GtkRequisition *requisition)
{
    XfceItembarPrivate *priv = XFCE_ITEMBAR (widget)->priv;
    XfceItembarChild   *child;
    GSList             *l;
    GtkRequisition      req;

    /* start with the border width */
    requisition->width = requisition->height =
        2 * GTK_CONTAINER (widget)->border_width;

    for (l = priv->children; l != NULL; l = l->next)
    {
        child = l->data;

        /* skip invisible plugins */
        if (GTK_WIDGET_VISIBLE (child->widget))
        {
            /* get the child size request */
            gtk_widget_size_request (child->widget, &req);

            /* append the widget size request */
            if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
                requisition->width += req.width;
            else
                requisition->height += req.height;
        }
    }

    if (G_LIKELY (priv->maximum_size != -1))
    {
        /* limit the maximum size */
        if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            requisition->width = MIN (requisition->width, priv->maximum_size);
        else
            requisition->height = MIN (requisition->height, priv->maximum_size);
    }
}



static void
xfce_itembar_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
    XfceItembarPrivate *priv = XFCE_ITEMBAR (widget)->priv;
    XfceItembarChild   *child;
    GtkRequisition      req;
    GSList             *li;
    gint                x, y, width, height, border_width;
    gint                itembar_length, itembar_size, itembar_expandable;
    gint                itembar_expandreq, n_expand, size, expand_size;
    gboolean            horizontal, rtl;

    /* itembar allocation */
    widget->allocation = *allocation;

    /* wether the orientation is horizontal */
    horizontal = (priv->orientation == GTK_ORIENTATION_HORIZONTAL);

    /* get the border width */
    border_width = GTK_CONTAINER (widget)->border_width;

    /* event window allocation */
    x      = allocation->x + border_width;
    y      = allocation->y + border_width;
    width  = allocation->width - 2 * border_width;
    height = allocation->height - 2 * border_width;

    /* mova and resize the event window */
    if (G_LIKELY (priv->event_window != NULL))
        gdk_window_move_resize (priv->event_window, x, y, width, height);

    /* get the length and fixed size of the itembar */
    if (horizontal)
    {
        itembar_length = width;
        itembar_size = height;
    }
    else
    {

        itembar_length = height;
        itembar_size = width;
    }

    /* set expandable width equal to the length of the bar */
    itembar_expandable = itembar_length;
    itembar_expandreq = 0;

    /* walk the childeren to get the requested width, the size we share between
     * the expandable items and the number of expand items */
    for (li = priv->children, n_expand = 0; li != NULL; li = li->next)
    {
        child = li->data;

        if (!GTK_WIDGET_VISIBLE (child->widget))
        {
            child->allocation.width = 0;
            child->allocation.height = 0;

            continue;
        }

        /* get child size request */
        gtk_widget_size_request (child->widget, &req);

        if (horizontal)
        {
            width  = req.width;
            height = itembar_size;

            if (child->expand)
                itembar_expandreq += width;
            else
                itembar_expandable -= width;
        }
        else
        {
            height = req.height;
            width  = itembar_size;

            if (child->expand)
                itembar_expandreq += height;
            else
                itembar_expandable -= height;
        }

        /* increase counter if child can expand */
        if (child->expand)
            n_expand++;

        /* set the allocation sizes */
        child->allocation.width  = width;
        child->allocation.height = height;
    }

    /* resize the expanding items if the request and available space don't match */
    if (n_expand > 0 && itembar_expandreq != itembar_expandable)
    {
        for (li = priv->children; li != NULL && n_expand > 0; li = li->next)
        {
            child = li->data;

            /* skip non expand items */
            if (!child->expand)
                continue;

            /* get the size that matters */
            size = horizontal ? child->allocation.width : child->allocation.height;

            /* calculate the relative expanding size */
            expand_size = MAX (itembar_expandable * size / itembar_expandreq, 0);

            /* limit to the expandable size */
            if (horizontal)
                child->allocation.width = expand_size;
            else
                child->allocation.height = expand_size;

            /* remove the allocated size from the available size */
            itembar_expandable -= expand_size;

            /* remove the origionally requested size from the total expand request */
            itembar_expandreq -= size;

            /* decrease counter */
            n_expand--;
        }
    }

    /* init coordinates and the widgets direction */
    x = y = border_width;
    rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

    /* allocate the childeren */
    for (li = priv->children; li != NULL; li = li->next)
    {
        child = li->data;

        /* skip invisible plugins */
        if (G_UNLIKELY (child->allocation.width == 0))
            continue;

        child->allocation.x = x + allocation->x;
        child->allocation.y = y + allocation->y;

        if (horizontal)
        {
            if (rtl)
            {
                /* invert the direction of the packed widgets */
                child->allocation.x = allocation->x + itembar_length
                                      - x - child->allocation.width;
            }

            x += child->allocation.width;
        }
        else
        {
            y += child->allocation.height;
        }

        /* send the allocation */
        gtk_widget_size_allocate (child->widget, &child->allocation);
    }

    if (priv->raised && priv->event_window != NULL)
        gdk_window_raise (priv->event_window);
}



static void
xfce_itembar_realize (GtkWidget *widget)
{
    GdkWindowAttr       attributes;
    gint                attributes_mask;
    gint                border_width;
    XfceItembarPrivate *priv = XFCE_ITEMBAR (widget)->priv;

    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

    border_width = GTK_CONTAINER (widget)->border_width;

    attributes.x = widget->allocation.x + border_width;
    attributes.y = widget->allocation.y + border_width;
    attributes.width = widget->allocation.width - 2 * border_width;
    attributes.height = widget->allocation.height - 2 * border_width;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget)
                            | GDK_POINTER_MOTION_MASK
                            | GDK_BUTTON_MOTION_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_EXPOSURE_MASK
                            | GDK_ENTER_NOTIFY_MASK
                            | GDK_LEAVE_NOTIFY_MASK;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes_mask = GDK_WA_X | GDK_WA_Y;

    widget->window = gtk_widget_get_parent_window (widget);
    g_object_ref (G_OBJECT (widget->window));

    priv->event_window = gdk_window_new (widget->window,
                                         &attributes, attributes_mask);

    gdk_window_set_user_data (priv->event_window, widget);

    widget->style = gtk_style_attach (widget->style, widget->window);
}



static void
xfce_itembar_unrealize (GtkWidget *widget)
{
    XfceItembarPrivate *priv = XFCE_ITEMBAR (widget)->priv;

    if (priv->event_window != NULL)
    {
        gdk_window_set_user_data (priv->event_window, NULL);
        gdk_window_destroy (priv->event_window);
        priv->event_window = NULL;
    }

    if (GTK_WIDGET_CLASS (parent_class)->unrealize)
        (*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}



static void
xfce_itembar_map (GtkWidget *widget)
{
    XfceItembarPrivate *priv = XFCE_ITEMBAR (widget)->priv;

    gdk_window_show (priv->event_window);

    (*GTK_WIDGET_CLASS (parent_class)->map) (widget);

    if (priv->raised)
        gdk_window_raise (priv->event_window);
}



static void
xfce_itembar_unmap (GtkWidget *widget)
{
    XfceItembarPrivate *priv = XFCE_ITEMBAR (widget)->priv;

    if (priv->event_window != NULL)
        gdk_window_hide (priv->event_window);

    (*GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}



static void
xfce_itembar_drag_leave (GtkWidget      *widget,
                         GdkDragContext *context,
                         guint           time_)
{
    XfceItembarPrivate *priv = XFCE_ITEMBAR (widget)->priv;

    if (priv->drag_highlight)
    {
        gdk_window_set_user_data (priv->drag_highlight, NULL);
        gdk_window_destroy (priv->drag_highlight);
        priv->drag_highlight = NULL;
    }

    priv->drop_index = -1;
}



static gboolean
xfce_itembar_drag_motion (GtkWidget      *widget,
                          GdkDragContext *context,
                          gint            x,
                          gint            y,
                          guint           time_)
{
    XfceItembar        *itembar = XFCE_ITEMBAR (widget);
    XfceItembarPrivate *priv = itembar->priv;
    XfceItembarChild   *child;
    gint                new_index, new_pos, border_width;
    GdkWindowAttr       attributes;
    guint               attributes_mask;

    new_index = _find_drop_index (itembar, x, y);

    if (!priv->drag_highlight)
    {
        attributes.window_type = GDK_WINDOW_CHILD;
        attributes.wclass      = GDK_INPUT_OUTPUT;
        attributes.visual      = gtk_widget_get_visual (widget);
        attributes.colormap    = gtk_widget_get_colormap (widget);
        attributes.event_mask  = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK |
                                 GDK_POINTER_MOTION_MASK;
        attributes.width       = 1;
        attributes.height      = 1;
        attributes_mask        = GDK_WA_VISUAL | GDK_WA_COLORMAP;
        priv->drag_highlight   = gdk_window_new (gtk_widget_get_parent_window (widget),
                                                 &attributes, attributes_mask);

        gdk_window_set_user_data (priv->drag_highlight, widget);
        gdk_window_set_background (priv->drag_highlight,
                                   &widget->style->fg[widget->state]);
    }

    if (priv->drop_index < 0 || priv->drop_index != new_index)
    {
        border_width = GTK_CONTAINER (widget)->border_width;

        child = g_slist_nth_data (priv->children, new_index);

        priv->drop_index = new_index;
        if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
            if (child)
                new_pos = child->widget->allocation.x;
            else
                new_pos = widget->allocation.x + widget->allocation.width
                          - border_width;

            gdk_window_move_resize (priv->drag_highlight, new_pos - 1,
                                    widget->allocation.y + border_width, 2,
                                    widget->allocation.height - border_width * 2);
        }
        else
        {
            if (child)
                new_pos = child->widget->allocation.y;
            else
                new_pos = widget->allocation.y + widget->allocation.height;

            gdk_window_move_resize (priv->drag_highlight,
                                    widget->allocation.x + border_width,
                                    new_pos - 1, widget->allocation.width
                                                 - border_width * 2, 2);
        }
    }

    gdk_window_show (priv->drag_highlight);

    /* gdk_drag_status (context, context->suggested_action, time_); */

    return TRUE;
}



static void
xfce_itembar_forall (GtkContainer *container,
                     gboolean      include_internals,
                     GtkCallback   callback,
                     gpointer      callback_data)
{
    GSList             *l;
    XfceItembarChild   *child;
    XfceItembarPrivate *priv = XFCE_ITEMBAR (container)->priv;

    for (l = priv->children; l != NULL; l = l->next)
    {
        child = l->data;

        if (child && GTK_IS_WIDGET (child->widget))
            (*callback) (child->widget, callback_data);
    }
}



static GType
xfce_itembar_child_type (GtkContainer *container)
{
    return GTK_TYPE_WIDGET;
}



static void
xfce_itembar_add (GtkContainer *container,
                  GtkWidget    *child)
{
    xfce_itembar_insert (XFCE_ITEMBAR (container), child, -1);
}



static void
xfce_itembar_remove (GtkContainer *container,
                     GtkWidget    *child)
{
    XfceItembarPrivate *priv = XFCE_ITEMBAR (container)->priv;
    XfceItembarChild   *ic;
    gboolean            was_visible;
    GSList             *l;

    _panel_return_if_fail (child != NULL && child->parent == GTK_WIDGET (container));

    for (l = priv->children; l != NULL; l = l->next)
    {
        ic = l->data;

        if (ic->widget == child)
        {
            priv->children = g_slist_delete_link (priv->children, l);

            was_visible = GTK_WIDGET_VISIBLE (ic->widget);

            gtk_widget_unparent (ic->widget);

            panel_slice_free (XfceItembarChild, ic);

            if (was_visible)
                gtk_widget_queue_resize (GTK_WIDGET (container));

            g_signal_emit (G_OBJECT (container),
                           itembar_signals [CONTENTS_CHANGED], 0);

            break;
        }
    }
}



static void
xfce_itembar_get_child_property (GtkContainer *container,
                                 GtkWidget    *child,
                                 guint         property_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
    gboolean expand;

    expand = FALSE;

    switch (property_id)
    {
        case CHILD_PROP_EXPAND:
            expand = xfce_itembar_get_child_expand (XFCE_ITEMBAR (container),
                                                    child);
            g_value_set_boolean (value, expand);
            break;
        default:
            GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container,
                                                          property_id, pspec);
            break;
    }
}



static void
xfce_itembar_set_child_property (GtkContainer *container,
                                 GtkWidget    *child,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    switch (property_id)
    {
        case CHILD_PROP_EXPAND:
            xfce_itembar_set_child_expand (XFCE_ITEMBAR (container), child,
                                           g_value_get_boolean (value));
            break;
        default:
            GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container,
                                                          property_id, pspec);
            break;
    }
}



/* ginternal functions */
static gint
_find_drop_index (XfceItembar *itembar,
                  gint         x,
                  gint         y)
{
    GSList             *l;
    GtkTextDirection    direction;
    gint                distance, cursor, pos, i, idx;
    XfceItembarChild   *child;
    XfceItembarPrivate *priv = itembar->priv;
    gint                best_distance = G_MAXINT;

    if (!priv->children)
        return 0;

    direction = gtk_widget_get_direction (GTK_WIDGET (itembar));

    idx = 0;

    child = priv->children->data;

    if (GTK_ORIENTATION_HORIZONTAL == priv->orientation)
    {
        cursor =  x;
        x = GTK_WIDGET (itembar)->allocation.x;

        if (direction == GTK_TEXT_DIR_LTR)
        {
            pos = child->widget->allocation.x - x;
        }
        else
        {
            pos = child->widget->allocation.x
                  + child->widget->allocation.width - x;
        }
    }
    else
    {
        cursor = y;
        y = GTK_WIDGET (itembar)->allocation.y;

        pos = child->widget->allocation.y - y;
    }

    best_distance = ABS (pos - cursor);

    /* distance to far end of each item */
    for (i = 1, l = priv->children; l != NULL; l = l->next, i++)
    {
        child = l->data;

        if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
            if (direction == GTK_TEXT_DIR_LTR)
                pos = child->widget->allocation.x
                      + child->widget->allocation.width - x;
            else
                pos = child->widget->allocation.x - x;
        }
        else
        {
            pos = y + child->widget->allocation.y
                  + child->widget->allocation.height - y;
        }

        distance = ABS (pos - cursor);

        if (distance <= best_distance)
        {
            best_distance = distance;
            idx = i;
        }
    }

    return idx;
}



/* public interface */

/**
 * xfce_itembar_new:
 * @orientation : #GtkOrientation for new itembar.
 *
 * Creates a new #XfceItembar widget.
 *
 * Returns: new #XfceItembar widget with @orientation.
 **/
GtkWidget *
xfce_itembar_new (GtkOrientation orientation)
{
    return g_object_new (XFCE_TYPE_ITEMBAR,
                         "orientation", orientation, NULL);
}



/**
 * xfce_itembar_set_maximum_size:
 * @size: maximum size request
 *
 * Control the maximum allocatable size of the itembar. This
 * function was introduced to handle larger request then the screen
 * on floating/normal width panels.
 **/
void
xfce_itembar_set_maximum_size (XfceItembar *itembar,
                               gint         size)
{
    XfceItembarPrivate *priv = itembar->priv;

    _panel_return_if_fail (XFCE_IS_ITEMBAR (itembar));

    priv->maximum_size = size;
}



/**
 * xfce_itembar_set_allow_expand:
 * @itembar: a #XfceItembar
 * @allow   : %TRUE when the expansion is allowed.
 *
 * Set whether the 'expand' child property is honored.
 *
 * See also: xfce_itembar_set_child_expand().
 **/
void
xfce_itembar_set_allow_expand (XfceItembar *itembar,
                               gboolean     allow)
{
    XfceItembarPrivate *priv = itembar->priv;

    _panel_return_if_fail (XFCE_IS_ITEMBAR (itembar));

    priv->expand_allowed = allow;
    gtk_widget_queue_resize (GTK_WIDGET(itembar));
}



/**
 * xfce_itembar_get_orientation:
 * @itembar : a #XfceItembar
 *
 * Return the orientation of the #XfceItembar.
 *
 * Returns: #GtkOrienation of @itembar.
 **/
GtkOrientation
xfce_itembar_get_orientation (XfceItembar *itembar)
{
    XfceItembarPrivate *priv = itembar->priv;

    _panel_return_val_if_fail (XFCE_IS_ITEMBAR (itembar), DEFAULT_ORIENTATION);

    return priv->orientation;
}



/**
 * xfce_itembar_set_orientation:
 * @itembar     : a #XfceItembar
 * @orientation : new #GtkOrientation
 *
 * Set the orienation of @itembar.
 **/
void
xfce_itembar_set_orientation (XfceItembar    *itembar,
                              GtkOrientation  orientation)
{
    XfceItembarPrivate *priv = itembar->priv;

    _panel_return_if_fail (XFCE_IS_ITEMBAR (itembar));

    if (orientation == priv->orientation)
        return;

    priv->orientation = orientation;

    g_signal_emit (G_OBJECT (itembar), itembar_signals[ORIENTATION_CHANGED], 0,
                   orientation);

    g_object_notify (G_OBJECT (itembar), "orientation");

    gtk_widget_queue_resize (GTK_WIDGET (itembar));
}



/**
 * xfce_itembar_insert:
 * @itembar  : a #XfceItembar
 * @item     : #GtkWidget to insert
 * @position : position for @item
 *
 * Insert new item at @position on @itembar.
 **/
void
xfce_itembar_insert (XfceItembar *itembar,
                     GtkWidget   *item,
                     gint         position)
{
    XfceItembarPrivate *priv = itembar->priv;
    XfceItembarChild   *child;

    _panel_return_if_fail (XFCE_IS_ITEMBAR (itembar));
    _panel_return_if_fail (item != NULL && GTK_WIDGET (item)->parent == NULL);

    child = panel_slice_new0 (XfceItembarChild);

    child->widget = item;

    priv->children = g_slist_insert (priv->children, child, position);

    gtk_widget_set_parent (GTK_WIDGET (item), GTK_WIDGET (itembar));

    gtk_widget_queue_resize (GTK_WIDGET (itembar));

    g_signal_emit (G_OBJECT (itembar), itembar_signals [CONTENTS_CHANGED], 0);
}



/**
 * xfce_itembar_append:
 * @itembar : a #XfceItembar
 * @item    : #GtkWidget to add
 *
 * Add a new item at the end of @itembar.
 **/
void
xfce_itembar_append (XfceItembar *itembar,
                     GtkWidget   *item)
{
    xfce_itembar_insert (itembar, item, -1);
}



/**
 * xfce_itembar_prepend:
 * @itembar : a #XfceItembar
 * @item    : #GtkWidget to add
 *
 * Add a new item at the start of @itembar.
 **/
void
xfce_itembar_prepend (XfceItembar *itembar,
                      GtkWidget   *item)
{
    xfce_itembar_insert (itembar, item, 0);
}



/**
 * xfce_itembar_reorder_child:
 * @itembar  : a #XfceItembar
 * @item     : a child #GtkWidget of @itembar
 * @position : new index for @item
 *
 * Move @item to a new position on @itembar.
 **/
void
xfce_itembar_reorder_child (XfceItembar *itembar,
                            GtkWidget   *item,
                            gint         position)
{
    XfceItembarPrivate *priv = itembar->priv;
    XfceItembarChild   *child;
    GSList             *l;

    _panel_return_if_fail (XFCE_IS_ITEMBAR (itembar));
    _panel_return_if_fail (item != NULL
                      && GTK_WIDGET (item)->parent == GTK_WIDGET (itembar));

    for (l = priv->children; l != NULL; l = l->next)
    {
        child = l->data;

        if (item == child->widget)
        {
            priv->children = g_slist_delete_link (priv->children, l);

            priv->children = g_slist_insert (priv->children, child, position);

            gtk_widget_queue_resize (GTK_WIDGET (itembar));

            g_signal_emit (G_OBJECT (itembar),
                           itembar_signals [CONTENTS_CHANGED], 0);

            break;
        }
    }
}



/**
 * xfce_itembar_get_child_expand:
 * @itembar : a #XfceItembar
 * @item    : a child #GtkWidget of @itembar
 *
 * Return if the #GtkWidget in the #XfceItembar expands when the panel
 * is resized.
 *
 * Returns: %TRUE if @item will expand when the size of @itembar increases,
 *          otherwise %FALSE.
 **/
gboolean
xfce_itembar_get_child_expand (XfceItembar *itembar,
                               GtkWidget   *item)
{
    XfceItembarPrivate *priv = itembar->priv;
    XfceItembarChild   *child;
    GSList             *l;

    _panel_return_val_if_fail (XFCE_IS_ITEMBAR (itembar), FALSE);
    _panel_return_val_if_fail (item != NULL && GTK_WIDGET (item)->parent == GTK_WIDGET (itembar),
                               FALSE);

    for (l = priv->children; l != NULL; l = l->next)
    {
        child = l->data;

        if (item == child->widget)
            return child->expand;
    }

    return FALSE;
}



/**
 * xfce_itembar_set_child_expand:
 * @itembar : a #XfceItembar
 * @item    : a child #GtkWidget of @itembar
 * @expand  : whether to expand the item
 *
 * Sets whether @item should expand when the size of @itembar increases.
 **/
void
xfce_itembar_set_child_expand (XfceItembar *itembar,
                               GtkWidget   *item,
                               gboolean     expand)
{
    XfceItembarPrivate *priv = itembar->priv;
    XfceItembarChild   *child;
    GSList             *l;

    _panel_return_if_fail (XFCE_IS_ITEMBAR (itembar));
    _panel_return_if_fail (item != NULL && GTK_WIDGET (item)->parent == GTK_WIDGET (itembar));

    for (l = priv->children; l != NULL; l = l->next)
    {
        child = l->data;

        if (item == child->widget)
        {
            child->expand = expand;
            break;
        }
    }

    gtk_widget_queue_resize (GTK_WIDGET (itembar));
}



/**
 * xfce_itembar_get_n_items:
 * @itembar : a #XfceItembar
 *
 * Return the number of items on a #XfceItembar.
 *
 * Returns: the number of items on @itembar.
 **/
gint
xfce_itembar_get_n_items (XfceItembar *itembar)
{
    XfceItembarPrivate *priv = itembar->priv;

    _panel_return_val_if_fail (XFCE_IS_ITEMBAR (itembar), 0);

    return g_slist_length (priv->children);
}



/**
 * xfce_itembar_get_item_index:
 * @itembar : a #XfceItembar
 * @item    : a child #GtkWidget of @itembar
 *
 * The index if the child #GtkWidget in a #XfceItembar.
 *
 * Returns: the index of @item or -1 if @itembar does not contain @item.
 **/
gint
xfce_itembar_get_item_index (XfceItembar *itembar,
                             GtkWidget   *item)
{
    XfceItembarPrivate *priv = itembar->priv;
    XfceItembarChild   *child;
    GSList             *l;
    gint                n;

    _panel_return_val_if_fail (XFCE_IS_ITEMBAR (itembar), -1);
    _panel_return_val_if_fail (item != NULL && GTK_WIDGET (item)->parent == GTK_WIDGET (itembar), -1);

    for (n = 0, l = priv->children; l != NULL; l = l->next, ++n)
    {
        child = l->data;

        if (item == child->widget)
        {
            return n;
        }
    }

    return -1;
}



/**
 * xfce_itembar_get_nth_item:
 * @itembar : a #XfceItembar
 * @n       : a position on the itembar
 *
 * Return the #GtkWidget inside the #XfceItembar with the @n<!-- -->'s
 * index.
 *
 * Returns: the @n<!-- -->'s item on @itembar, or %NULL if the
 * itembar does not contain an @n<!-- -->'th item.
 **/
GtkWidget *
xfce_itembar_get_nth_item (XfceItembar *itembar,
                           gint         n)
{
    XfceItembarPrivate *priv = itembar->priv;
    XfceItembarChild   *child;
    gint                n_items;

    _panel_return_val_if_fail (XFCE_IS_ITEMBAR (itembar), NULL);

    n_items = g_slist_length (priv->children);

    if (n < 0 || n >= n_items)
        return NULL;

    child = g_slist_nth_data (priv->children, n);

    return child->widget;
}



/**
 * xfce_itembar_raise_event_window:
 * @itembar : a #XfceItembar
 *
 * Raise the event window of @itembar. This causes all events, like
 * mouse clicks or key presses to be send to the itembar and not to
 * any item.
 *
 * See also: xfce_itembar_lower_event_window()
 **/
void
xfce_itembar_raise_event_window (XfceItembar *itembar)
{
    XfceItembarPrivate *priv = itembar->priv;

    _panel_return_if_fail (XFCE_IS_ITEMBAR (itembar));

    priv->raised = TRUE;

    if (priv->event_window)
        gdk_window_raise (priv->event_window);
}



/**
 * xfce_itembar_lower_event_window:
 * @itembar : a #XfceItembar
 *
 * Lower the event window of @itembar. This causes all events, like
 * mouse clicks or key presses to be send to the items, before reaching the
 * itembar.
 *
 * See also: xfce_itembar_raise_event_window()
 **/
void
xfce_itembar_lower_event_window (XfceItembar *itembar)
{
    XfceItembarPrivate *priv = itembar->priv;

    _panel_return_if_fail (XFCE_IS_ITEMBAR (itembar));

    priv->raised = FALSE;

    if (priv->event_window)
        gdk_window_lower (priv->event_window);
}



/**
 * xfce_itembar_event_window_is_raised:
 * @itembar : a #XfceItembar
 *
 * Whether the event window of the #XfceItembar is raised.
 *
 * Returns: %TRUE if event window is raised.
 **/
gboolean
xfce_itembar_event_window_is_raised (XfceItembar *itembar)
{
    XfceItembarPrivate *priv = itembar->priv;

    _panel_return_val_if_fail (XFCE_IS_ITEMBAR (itembar), FALSE);

    return priv->raised;
}



/**
 * xfce_itembar_get_item_at_point:
 * @itembar : a #XfceItembar
 * @x       : x coordinate relative to the itembar window
 * @y       : y coordinate relative to the itembar window
 *
 * Return the #GtkWidget in the #XfceItembar that is below the (x,y)
 * coordinate.
 *
 * Returns: a #GtkWidget or %NULL.
 **/
GtkWidget *
xfce_itembar_get_item_at_point (XfceItembar *itembar,
                                gint         x,
                                gint         y)
{
    XfceItembarPrivate *priv = itembar->priv;
    XfceItembarChild   *child;
    GSList             *l;
    GtkWidget          *w;
    GtkAllocation      *a;

    _panel_return_val_if_fail (XFCE_IS_ITEMBAR (itembar), NULL);

    x += GTK_WIDGET (itembar)->allocation.x;
    y += GTK_WIDGET (itembar)->allocation.y;

    for (l = priv->children; l != NULL; l = l->next)
    {
        child = l->data;
        w = child->widget;
        a = &(w->allocation);

        if (x >= a->x && x < a->x + a->width
            && y >= a->y && y < a->y + a->height)
        {
            return w;
        }
    }

    return NULL;
}



/**
 * xfce_itembar_get_drop_index:
 * @itembar : a #XfceItembar
 * @x       : x coordinate of a point on the itembar
 * @y       : y coordinate of a point on the itembar
 *
 * Returns the position corresponding to the indicated point on
 * @itembar. This is useful when dragging items to the itembar:
 * this function returns the position a new item should be
 * inserted.
 *
 * @x and @y are in @itembar coordinates.
 *
 * Returns: The position corresponding to the point (@x, @y) on the
 * itembar.
 **/
gint
xfce_itembar_get_drop_index (XfceItembar *itembar,
                             gint         x,
                             gint         y)
{
    _panel_return_val_if_fail (XFCE_IS_ITEMBAR (itembar), FALSE);

    return _find_drop_index (itembar, x, y);
}



#define __XFCE_ITEMBAR_C__
#include <libxfce4panel/libxfce4panel-aliasdef.c>