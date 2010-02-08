/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* mtp-toolbar-button.c */
/*
 * Copyright (c) 2009, 2010 Intel Corp.
 *
 * Author: Tomas Frydrych <tf@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <glib/gi18n.h>

#include "mtp-toolbar.h"
#include "mtp-toolbar-button.h"
#include "mtp-space.h"

static void mx_droppable_iface_init (MxDroppableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MtpToolbar, mtp_toolbar, MX_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (MX_TYPE_DROPPABLE,
                                                mx_droppable_iface_init));

#define MTP_TOOLBAR_GET_PRIVATE(obj)    \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MTP_TYPE_TOOLBAR, MtpToolbarPrivate))

enum
{
  ZONE_PROP_0 = 0,

  /* d&d */
  ZONE_PROP_ENABLED
};

struct _MtpToolbarPrivate
{
  ClutterActor *applet_area;
  ClutterActor *panel_area;
  ClutterActor *clock;

  gboolean enabled  : 1;
  gboolean modified : 1;
  gboolean disposed : 1;
};

static void
mtp_toolbar_dispose (GObject *object)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (object)->priv;

  if (priv->disposed)
    return;

  priv->disposed = TRUE;

  clutter_actor_destroy (priv->clock);
  priv->clock = NULL;

  clutter_actor_destroy (priv->panel_area);
  priv->panel_area = NULL;

  clutter_actor_destroy (priv->applet_area);
  priv->applet_area = NULL;

  G_OBJECT_CLASS (mtp_toolbar_parent_class)->dispose (object);
}

static void
mtp_toolbar_map (ClutterActor *actor)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (actor)->priv;

  CLUTTER_ACTOR_CLASS (mtp_toolbar_parent_class)->map (actor);

  clutter_actor_map (priv->clock);
  clutter_actor_map (priv->panel_area);
  clutter_actor_map (priv->applet_area);
}

static void
mtp_toolbar_unmap (ClutterActor *actor)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (actor)->priv;

  clutter_actor_unmap (priv->clock);
  clutter_actor_unmap (priv->panel_area);
  clutter_actor_unmap (priv->applet_area);

  CLUTTER_ACTOR_CLASS (mtp_toolbar_parent_class)->unmap (actor);
}

static void
mtp_toolbar_allocate (ClutterActor          *actor,
                      const ClutterActorBox *box,
                      ClutterAllocationFlags flags)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (actor)->priv;
  MxPadding          padding;

  CLUTTER_ACTOR_CLASS (
             mtp_toolbar_parent_class)->allocate (actor, box, flags);

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  clutter_actor_set_position (priv->clock, padding.left, padding.top);
  clutter_actor_allocate_preferred_size (priv->clock, flags);

  clutter_actor_set_position (priv->panel_area, 213.0, padding.top);
  clutter_actor_allocate_preferred_size (priv->panel_area, flags);

  clutter_actor_set_position (priv->applet_area, 793.0, padding.top);
  clutter_actor_allocate_preferred_size (priv->applet_area, flags);
}

static void
mtp_toolbar_update_time_date (ClutterActor *time_label,
                              ClutterActor *date_label)
{
  time_t         t;
  struct tm     *tmp;
  char           time_str[64];

  t = time (NULL);
  tmp = localtime (&t);
  if (tmp)
    /* translators: translate this to a suitable time format for your locale
     * showing only hours and minutes. For available format specifiers see
     * http://www.opengroup.org/onlinepubs/007908799/xsh/strftime.html
     */
    strftime (time_str, 64, _("%l:%M %P"), tmp);
  else
    snprintf (time_str, 64, "Time");
  mx_label_set_text (MX_LABEL (time_label), time_str);

  if (tmp)
    /* translators: translate this to a suitable date format for your locale.
     * For availabe format specifiers see
     * http://www.opengroup.org/onlinepubs/007908799/xsh/strftime.html
     */
    strftime (time_str, 64, _("%B %e, %Y"), tmp);
  else
    snprintf (time_str, 64, "Date");

  mx_label_set_text (MX_LABEL (date_label), time_str);
}

static void
mtp_toolbar_constructed (GObject *self)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (self)->priv;
  ClutterActor      *actor = CLUTTER_ACTOR (self);

  mx_box_layout_set_spacing (MX_BOX_LAYOUT (self), 10);

  {
    ClutterActor *clock;
    ClutterActor *time_bin;
    ClutterActor *time;
    ClutterActor *date_bin;
    ClutterActor *date;

    priv->clock = clock = clutter_group_new ();

    time = mx_label_new ("");
    clutter_actor_set_name (time, "time-label");

    time_bin = mx_frame_new ();
    mx_bin_set_child (MX_BIN (time_bin), time);
    clutter_actor_set_position (time_bin, 20.0, 8.0);
    clutter_actor_set_width (time_bin, 161.0);

    date = mx_label_new ("");
    clutter_actor_set_name (date, "date-label");

    date_bin = mx_frame_new ();
    mx_bin_set_child (MX_BIN (date_bin), date);
    clutter_actor_set_position (date_bin, 20.0, 35.0);
    clutter_actor_set_size (date_bin, 161.0, 25.0);

    /*
     * Get the current time / date (though we currently do not bother about
     * updating it)
     */
    mtp_toolbar_update_time_date (time, date);

    clutter_container_add (CLUTTER_CONTAINER (clock),
                           CLUTTER_ACTOR (time_bin),
                           CLUTTER_ACTOR (date_bin),
                           NULL);

    clutter_actor_set_parent (clock, actor);
  }

  priv->panel_area  = mx_box_layout_new ();
  clutter_actor_set_name (priv->panel_area, "panel-area");
  mx_box_layout_set_spacing (MX_BOX_LAYOUT (priv->panel_area), 2);
  mx_box_layout_set_enable_animations (MX_BOX_LAYOUT (priv->panel_area), TRUE);
  clutter_actor_set_parent (priv->panel_area, actor);

  priv->applet_area = mx_box_layout_new ();
  clutter_actor_set_name (priv->applet_area, "applet-area");
  mx_box_layout_set_spacing (MX_BOX_LAYOUT (priv->applet_area), 2);
  mx_box_layout_set_pack_start (MX_BOX_LAYOUT (priv->applet_area), FALSE);
  mx_box_layout_set_enable_animations (MX_BOX_LAYOUT (priv->applet_area), TRUE);
  clutter_actor_set_parent (priv->applet_area, actor);
}

static void
mtp_toolbar_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (gobject)->priv;

  switch (prop_id)
    {
    case ZONE_PROP_ENABLED:
      priv->enabled = g_value_get_boolean (value);
      if (priv->enabled)
        mx_droppable_enable (MX_DROPPABLE (gobject));
      else
        mx_droppable_disable (MX_DROPPABLE (gobject));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
mtp_toolbar_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (gobject)->priv;

  switch (prop_id)
    {
    case ZONE_PROP_ENABLED:
      g_value_set_boolean (value, priv->enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
mtp_toolbar_over_in (MxDroppable *droppable, MxDraggable *draggable)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (droppable)->priv;

  if (!priv->enabled)
    return;
}

static void
mtp_toolbar_over_out (MxDroppable *droppable, MxDraggable *draggable)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (droppable)->priv;

  if (!priv->enabled)
    return;
}

/*
 * returns FALSE if no space could be removed.
 */
static gboolean
mtp_toolbar_remove_space (MtpToolbar *toolbar, gboolean applet)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;
  GList             *children;
  GList             *last;
  gboolean           retval = FALSE;
  ClutterContainer  *container;

  container = applet ? CLUTTER_CONTAINER (priv->applet_area) :
    CLUTTER_CONTAINER (priv->panel_area);

  children = clutter_container_get_children (container);

  last = applet ? g_list_first (children) : g_list_last (children);

  if (last)
    {
      ClutterActor *actor = last->data;

      if (MTP_IS_SPACE (actor))
        {
          retval = TRUE;
          clutter_container_remove_actor (container, actor);
        }
      else
        g_warning (G_STRLOC ":%s: no space left on toolbar !!!", __FUNCTION__);
    }

  g_list_free (children);

  return retval;
}

/*
 * D&D drop handler
 */
static void
mtp_toolbar_drop (MxDroppable         *droppable,
                  MxDraggable         *draggable,
                  gfloat               event_x,
                  gfloat               event_y,
                  gint                 button,
                  ClutterModifierType  modifiers)
{
  MtpToolbar        *toolbar = MTP_TOOLBAR (droppable);
  MtpToolbarPrivate *priv = toolbar->priv;
  MtpToolbarButton  *tbutton = MTP_TOOLBAR_BUTTON (draggable);
  ClutterActor      *actor = CLUTTER_ACTOR (draggable);
  ClutterActor      *parent = clutter_actor_get_parent (actor);
  ClutterActor      *stage = clutter_actor_get_stage (actor);
  MtpToolbarButton  *before = NULL;

  /*
   * Check we are not disabled (we should really not be getting drop events on
   * disabled droppables, but we do).
   */
  if (!priv->enabled)
    {
      g_warning ("Bug: received a drop on a disabled droppable -- ignoring");
      return;
    }

  /*
   * We need to work out where on the Toolbar the drop happened; specifically,
   * we are interested which current button is the new button being inserted
   * before. Using the pick for this is simple.
   */
  {
    ClutterActor *target;

    /*
     * Disable pick on the current draggable, since that is the top-most actor
     * at the position we care about.
     */
    mtp_toolbar_button_set_dont_pick (tbutton, TRUE);

    target = clutter_stage_get_actor_at_pos (CLUTTER_STAGE (stage),
                                             CLUTTER_PICK_REACTIVE,
                                             event_x, event_y);

    while (target && !(MTP_IS_TOOLBAR_BUTTON (target) ||
                       MTP_IS_TOOLBAR (target)))
      target = clutter_actor_get_parent (target);

    if (target)
      {
        if (!(MTP_IS_TOOLBAR_BUTTON (target)))
          {
            /*
             * We either hit the empty part of the toolbar, or a gap between
             * buttons. See what's 20px to the right.
             */
            target = clutter_stage_get_actor_at_pos (CLUTTER_STAGE (stage),
                                                     CLUTTER_PICK_REACTIVE,
                                                     event_x + 20, event_y);
            if (!MTP_IS_TOOLBAR_BUTTON (target))
              target = NULL;
          }

        before = (MtpToolbarButton*)target;
      }

    mtp_toolbar_button_set_dont_pick (tbutton, FALSE);
  }

  /*
   * If we do not have a 'before' button, find the first space.
   */
  if (!before)
    {
      GList *children, *l;

      if (mtp_toolbar_button_is_applet (tbutton))
        children = clutter_container_get_children (CLUTTER_CONTAINER (
                                                          priv->applet_area));
      else
        children = clutter_container_get_children (CLUTTER_CONTAINER (
                                                          priv->panel_area));

      for (l = children; l; l = l->next)
        {
          if (MTP_IS_SPACE (l->data))
            {
              before = l->data;
              break;
            }
        }

      g_list_free (children);
    }

  if (mtp_toolbar_remove_space (toolbar,
                                mtp_toolbar_button_is_applet (tbutton)))
    {
      g_object_ref (draggable);
      clutter_container_remove_actor (CLUTTER_CONTAINER (parent), actor);

      mtp_toolbar_insert_button (toolbar,
                                 (ClutterActor*)tbutton, (ClutterActor*)before);

      g_object_unref (draggable);
    }
}

static void
mx_droppable_iface_init (MxDroppableIface *iface)
{
  iface->over_in  = mtp_toolbar_over_in;
  iface->over_out = mtp_toolbar_over_out;
  iface->drop     = mtp_toolbar_drop;
}

static void
mtp_toolbar_pick (ClutterActor *self, const ClutterColor *color)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (self)->priv;

  CLUTTER_ACTOR_CLASS (mtp_toolbar_parent_class)->pick (self, color);

  clutter_actor_paint (priv->clock);
  clutter_actor_paint (priv->panel_area);
  clutter_actor_paint (priv->applet_area);
}

static void
mtp_toolbar_paint (ClutterActor *self)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (self)->priv;

  CLUTTER_ACTOR_CLASS (mtp_toolbar_parent_class)->paint (self);

  clutter_actor_paint (priv->clock);
  clutter_actor_paint (priv->panel_area);
  clutter_actor_paint (priv->applet_area);
}

static void
mtp_toolbar_class_init (MtpToolbarClass *klass)
{
  ClutterActorClass *actor_class  = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MtpToolbarPrivate));

  actor_class->allocate           = mtp_toolbar_allocate;
  actor_class->map                = mtp_toolbar_map;
  actor_class->unmap              = mtp_toolbar_unmap;
  actor_class->paint              = mtp_toolbar_paint;
  actor_class->pick               = mtp_toolbar_pick;

  object_class->constructed       = mtp_toolbar_constructed;
  object_class->dispose           = mtp_toolbar_dispose;
  object_class->set_property      = mtp_toolbar_set_property;
  object_class->get_property      = mtp_toolbar_get_property;

  g_object_class_override_property (object_class, ZONE_PROP_ENABLED,"enabled");
}

static void
mtp_toolbar_init (MtpToolbar *self)
{
  self->priv = MTP_TOOLBAR_GET_PRIVATE (self);

  clutter_actor_set_reactive ((ClutterActor*)self, TRUE);
}

ClutterActor*
mtp_toolbar_new (void)
{
  return g_object_new (MTP_TYPE_TOOLBAR, NULL);
}

void
mtp_toolbar_add_button (MtpToolbar *toolbar, ClutterActor *button)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;

  priv->modified = TRUE;

  if (MTP_IS_SPACE (button))
    {
      clutter_container_add_actor (CLUTTER_CONTAINER (priv->panel_area),
                                   button);
      clutter_container_child_set (CLUTTER_CONTAINER (priv->panel_area),
                                   button,
                                   "expand", FALSE, NULL);
    }
  else if (MTP_IS_TOOLBAR_BUTTON (button))
    {
      if (mtp_toolbar_button_is_applet ((MtpToolbarButton*)button))
        {
          gfloat depth = 0.0;

          GList *children = clutter_container_get_children (
                                       CLUTTER_CONTAINER (priv->applet_area));

          if (children)
            {
              depth = clutter_actor_get_depth (children->data) - 0.05;

              g_list_free (children);
            }

          clutter_container_add_actor (CLUTTER_CONTAINER (priv->applet_area),
                                       button);
          clutter_container_child_set (CLUTTER_CONTAINER (priv->applet_area),
                                       button,
                                       "expand", FALSE, NULL);
          clutter_actor_set_depth (button, depth);
        }
      else
        {
          clutter_container_add_actor (CLUTTER_CONTAINER (priv->panel_area),
                                       button);
          clutter_container_child_set (CLUTTER_CONTAINER (priv->panel_area),
                                       button,
                                       "expand", FALSE, NULL);
        }
    }
  else
    g_warning (G_STRLOC ":%s: unsupported actor type %s",
               __FUNCTION__, G_OBJECT_TYPE_NAME (button));
}

void
mtp_toolbar_remove_button (MtpToolbar *toolbar, ClutterActor *button)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;

  priv->modified = TRUE;

  if (MTP_IS_SPACE (button))
    {
      clutter_container_remove_actor (CLUTTER_CONTAINER (priv->panel_area),
                                      button);
    }
  else if (MTP_IS_TOOLBAR_BUTTON (button))
    {
      if (mtp_toolbar_button_is_applet ((MtpToolbarButton*)button))
        clutter_container_remove_actor (CLUTTER_CONTAINER (priv->applet_area),
                                        button);
      else
        clutter_container_remove_actor (CLUTTER_CONTAINER (priv->panel_area),
                                        button);
    }
  else
    g_warning (G_STRLOC ":%s: unsupported actor type %s",
               __FUNCTION__, G_OBJECT_TYPE_NAME (button));
}

/*
 * The MxBoxLayout container does really nice animations for us when we
 * move buttons around, but it does not support inserting an item at a specific
 * position. We work around this taking advantage of the fact that the actors
 * inside this container are always sorted by their depth; we use a very small
 * depth increment, so that even for a large number of buttons the effective
 * depth still remains below 1px.
 */
typedef struct _DepthData
{
  gfloat        depth;
  gfloat        new_depth;
  ClutterActor *before;
  ClutterActor *new;
  gboolean      applets;
} DepthData;

static void
set_depth_for_each (ClutterActor *child, gpointer data)
{
  DepthData *ddata = data;

  if (child == ddata->before)
    ddata->new_depth = ddata->depth;

  if (child != ddata->new)
    {
      if (child == ddata->before)
        ddata->depth += 0.05;

      clutter_actor_set_depth (child, ddata->depth);
    }

  ddata->depth += 0.05;
}

void
mtp_toolbar_insert_button (MtpToolbar   *toolbar,
                           ClutterActor *button,
                           ClutterActor *before)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;
  DepthData          ddata;
  GList             *children = NULL;

  if (!before)
    {
      mtp_toolbar_add_button (toolbar, button);
      return;
    }

  priv->modified = TRUE;

  ddata.depth = 0.0;
  ddata.before = before;
  ddata.new = button;
  ddata.applets = FALSE;

  if (MTP_IS_SPACE (button))
    {
      children =
        clutter_container_get_children (CLUTTER_CONTAINER (priv->panel_area));

      g_list_foreach (children, (GFunc)set_depth_for_each, &ddata);

      clutter_container_add_actor (CLUTTER_CONTAINER (priv->panel_area),
                                   button);

      clutter_actor_set_depth (ddata.new, ddata.new_depth);

      clutter_container_child_set (CLUTTER_CONTAINER (priv->panel_area),
                                   button,
                                   "expand", FALSE, NULL);
    }
  else if (MTP_IS_TOOLBAR_BUTTON (button))
    {
      if (mtp_toolbar_button_is_applet ((MtpToolbarButton*)button))
        {
          ddata.applets = TRUE;

          children =
            clutter_container_get_children (CLUTTER_CONTAINER (
                                                priv->applet_area));

          g_list_foreach (children, (GFunc)set_depth_for_each, &ddata);

          clutter_container_add_actor (CLUTTER_CONTAINER (priv->applet_area),
                                       button);

          clutter_actor_set_depth (ddata.new, ddata.new_depth);

          clutter_container_child_set (CLUTTER_CONTAINER (priv->applet_area),
                                       button,
                                       "expand", FALSE, NULL);
        }
      else
        {
          children =
            clutter_container_get_children (CLUTTER_CONTAINER (
                                                        priv->panel_area));

          g_list_foreach (children, (GFunc)set_depth_for_each, &ddata);

          clutter_container_add_actor (CLUTTER_CONTAINER (priv->panel_area),
                                       button);

          clutter_actor_set_depth (ddata.new, ddata.new_depth);

          clutter_container_child_set (CLUTTER_CONTAINER (priv->panel_area),
                                       button,
                                       "expand", FALSE, NULL);
        }
    }

  g_list_free (children);
}

gboolean
mtp_toolbar_was_modified (MtpToolbar *toolbar)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;

  return priv->modified;
}

GList *
mtp_toolbar_get_panel_buttons (MtpToolbar *toolbar)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;
  GList *children,  *ret = NULL, *l;

  children =
    clutter_container_get_children (CLUTTER_CONTAINER (priv->panel_area));

  for (l = children; l; l = l->next)
    if (MTP_IS_TOOLBAR_BUTTON (l->data))
      ret = g_list_append (ret, l->data);

  g_list_free (children);

  return ret;
}

GList *
mtp_toolbar_get_applet_buttons (MtpToolbar *toolbar)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;
  GList *children, *ret = NULL, *l;

  children =
    clutter_container_get_children (CLUTTER_CONTAINER (priv->applet_area));

  for (l = children; l; l = l->next)
    if (MTP_IS_TOOLBAR_BUTTON (l->data))
      ret = g_list_append (ret, l->data);

  g_list_free (children);

  return ret;
}

void
mtp_toolbar_clear_modified_state (MtpToolbar *toolbar)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;

  priv->modified = FALSE;
}

void
mtp_toolbar_mark_modified (MtpToolbar *toolbar)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;

  priv->modified = TRUE;
}

void
mtp_toolbar_fill_space (MtpToolbar *toolbar)
{
  MtpToolbarPrivate *priv = MTP_TOOLBAR (toolbar)->priv;
  GList             *l;
  gint               i, n_panels, n_applets;
  gfloat             applet_depth = 0.0;

  l = clutter_container_get_children (CLUTTER_CONTAINER (priv->panel_area));
  n_panels = g_list_length (l);
  g_list_free (l);

  l = clutter_container_get_children (CLUTTER_CONTAINER (priv->applet_area));
  n_applets = g_list_length (l);

  if (l)
    applet_depth = clutter_actor_get_depth (l->data) - 0.05;

  g_list_free (l);

    for (i = 0; i < 8 - n_panels; ++i)
    {
      ClutterActor *space = mtp_space_new ();

      clutter_container_add_actor (CLUTTER_CONTAINER (priv->panel_area),
                                   space);
      clutter_container_child_set (CLUTTER_CONTAINER (priv->panel_area),
                                   space,
                                   "expand", FALSE, NULL);
    }

    for (i = 0; i < 4 - n_applets; ++i)
    {
      ClutterActor *space = mtp_space_new ();
      mtp_space_set_is_applet ((MtpSpace*)space, TRUE);

      clutter_container_add_actor (CLUTTER_CONTAINER (priv->applet_area),
                                   space);
      clutter_container_child_set (CLUTTER_CONTAINER (priv->applet_area),
                                   space,
                                   "expand", FALSE, NULL);
      clutter_actor_set_depth (space, applet_depth);
      applet_depth -= 0.05;
    }
}