/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Moblin Netbook
 * Copyright © 2009, Intel Corporation.
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
#include <string.h>
#include <clutter/x11/clutter-x11.h>

#include "mnb-switcher-app.h"
#include "mnb-switcher-zone.h"

struct _MnbSwitcherAppPrivate
{
  MutterWindow        *mw;         /* MutterWindow we represent */
  guint                focus_id;   /* id for our focus cb       */

  /* Draggable properties */
  guint               threshold;
  NbtkDragAxis        axis;
  NbtkDragContainment containment;
  ClutterActorBox     area;

  gboolean            disposed              : 1; /* disposed guard   */
  gboolean            enabled               : 1; /* dragging enabled */
  gboolean            ignore_button_release : 1; /* ingore release immediately
                                                  * following a drop */
  /* d&d related stuff */
  gint                orig_col;     /* column we had before drag began     */
  gint                orig_row;     /* row we had before drag began        */
  ClutterActor       *orig_parent;  /* pre-drag parent                     */
  MnbSwitcherZone    *orig_zone;    /* pre-drag zone                       */
  ClutterActor       *clone;        /* clone taking our place in orig zone */
};

enum
{
  PROP_0 = 0,

  PROP_MUTTER_WINDOW,

  /* d&d properties */
  PROP_DRAG_THRESHOLD,
  PROP_AXIS,
  PROP_CONTAINMENT_TYPE,
  PROP_CONTAINMENT_AREA,
  PROP_ENABLED
};

static void nbtk_draggable_iface_init (NbtkDraggableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MnbSwitcherApp,
                         mnb_switcher_app,
                         MNB_TYPE_SWITCHER_ITEM,
                         G_IMPLEMENT_INTERFACE (NBTK_TYPE_DRAGGABLE,
                                                nbtk_draggable_iface_init));

#define MNB_SWITCHER_APP_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MNB_TYPE_SWITCHER_APP,\
                                MnbSwitcherAppPrivate))

static void
mnb_switcher_app_dispose (GObject *object)
{
  MnbSwitcherAppPrivate *priv = MNB_SWITCHER_APP (object)->priv;
  MetaWindow            *meta_win;

  if (priv->disposed)
    return;

  priv->disposed = TRUE;

  meta_win = mutter_window_get_meta_window (priv->mw);

  if (priv->focus_id)
    {
      g_signal_handler_disconnect (meta_win, priv->focus_id);
      priv->focus_id = 0;
    }

  G_OBJECT_CLASS (mnb_switcher_app_parent_class)->dispose (object);
}

static void
mnb_switcher_app_drag_begin (NbtkDraggable       *draggable,
                             gfloat               event_x,
                             gfloat               event_y,
                             gint                 event_button,
                             ClutterModifierType  modifiers)
{
  MnbSwitcherAppPrivate *priv = MNB_SWITCHER_APP (draggable)->priv;
  ClutterActor          *self     = CLUTTER_ACTOR (draggable);
  MnbSwitcher           *switcher;
  ClutterActor          *stage    = clutter_stage_get_default ();
  ClutterActor          *parent;
  ClutterActor          *clone;
  gfloat                 x, y, width, height;
  gint                   col, row;
  MnbSwitcherZone       *zone;

  zone = mnb_switcher_item_get_zone (MNB_SWITCHER_ITEM (self));

  g_assert (zone);

  /*
   * We will get a button release event eventualy when the drag ends, we need
   * to ignore it (i.e., not activate this window on drop.
   */
  priv->ignore_button_release = TRUE;

  /*
   * Signal to switcher that we now doing d&d
   */
  switcher = mnb_switcher_item_get_switcher (MNB_SWITCHER_ITEM (self));
  mnb_switcher_dnd_started (switcher, zone);

  /*
   * Hide any tooltip we might be showing.
   */
  mnb_switcher_item_hide_tooltip (MNB_SWITCHER_ITEM (self));

  /*
   * Store the original parent and row/column, so we can put the actor
   * back if the d&d is cancelled.
   */
  parent = clutter_actor_get_parent (self);

  clutter_container_child_get (CLUTTER_CONTAINER (parent), self,
                               "col", &col,
                               "row", &row, NULL);

  priv->orig_parent = parent;
  priv->orig_zone = zone;
  priv->orig_col = col;
  priv->orig_row = row;

  /*
   * Reparent to stage, preserving size and position
   */
  clutter_actor_get_size (self, &width, &height);

  if (!clutter_actor_transform_stage_point (self, event_x, event_y, &x, &y))
    {
      x = event_x;
      y = event_y;
    }
  else
    {
      x = event_x - x;
      y = event_y - y;
    }

  /*
   * Create a clone of ourself and put it in our present place in the Zone.
   */
  clone = clutter_clone_new (self);
  priv->clone = clone;
  clutter_actor_set_opacity (clone, 0x7f);
  clutter_actor_set_size (clone, width, height);
  nbtk_table_add_actor (NBTK_TABLE (parent), clone, row, col);
  clutter_container_child_set (CLUTTER_CONTAINER (parent), clone,
                               "y-fill", FALSE,
                               "x-fill", FALSE,  NULL);

  /*
   * Release self from the Zone by reparenting to stage, so we can move about
   */
  clutter_actor_reparent (self, stage);
  clutter_actor_set_position (self, x, y);
  clutter_actor_set_size (self, width, height);
}

static void
mnb_switcher_app_drag_motion (NbtkDraggable *draggable,
                              gfloat         delta_x,
                              gfloat         delta_y)
{
  clutter_actor_move_by (CLUTTER_ACTOR (draggable), delta_x, delta_y);
}

static void
mnb_switcher_app_drag_end (NbtkDraggable *draggable,
                           gfloat         event_x,
                           gfloat         event_y)
{
  MnbSwitcherAppPrivate *priv = MNB_SWITCHER_APP (draggable)->priv;
  ClutterActor          *self     = CLUTTER_ACTOR (draggable);
  MnbSwitcher           *switcher;
  ClutterActor          *parent;
  ClutterActor          *clone;

  switcher = mnb_switcher_item_get_switcher (MNB_SWITCHER_ITEM (self));

  if (!mnb_switcher_get_dnd_in_progress (switcher))
    {
      g_warning ("Drag end whilst d&d not happening !!!");
      return;
    }

  clone = priv->clone;
  priv->clone = NULL;

  /*
   * Signal switcher the d&d is now over.
   */
  mnb_switcher_dnd_ended (switcher, priv->orig_zone);

  /*
   * See if there was a drop (if not, we are still parented to stage)
   */
  parent = clutter_actor_get_parent (self);

  if (parent == clutter_stage_get_default ())
    {
      ClutterContainer *orig_parent = CLUTTER_CONTAINER (priv->orig_parent);

      /*
       * This is the case where the drop was cancelled; put ourselves back
       * where we were.
       */
      g_object_ref (self);

      clutter_container_remove_actor (CLUTTER_CONTAINER (parent), self);
      clutter_actor_set_size (self, -1.0, -1.0);
      nbtk_table_add_actor (NBTK_TABLE (orig_parent), self,
                            priv->orig_row, priv->orig_col);

      clutter_container_child_set (orig_parent, self,
                                   "y-fill", FALSE,
                                   "x-fill", FALSE,  NULL);

      mnb_switcher_zone_reset_state (MNB_SWITCHER_ZONE (priv->orig_zone));

      g_object_unref (self);
    }

  /*
   * Now get rid of the clone that we put in our place
   */
  parent = clutter_actor_get_parent (clone);
  clutter_container_remove_actor (CLUTTER_CONTAINER (parent), clone);
}

static void
nbtk_draggable_iface_init (NbtkDraggableIface *iface)
{
  iface->drag_begin  = mnb_switcher_app_drag_begin;
  iface->drag_motion = mnb_switcher_app_drag_motion;
  iface->drag_end    = mnb_switcher_app_drag_end;
}

static void
mnb_switcher_app_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  MnbSwitcherAppPrivate *priv = MNB_SWITCHER_APP (gobject)->priv;

  switch (prop_id)
    {
    case PROP_MUTTER_WINDOW:
      priv->mw = g_value_get_object (value);
      break;
    case PROP_DRAG_THRESHOLD:
      priv->threshold = g_value_get_uint (value);
      break;

    case PROP_AXIS:
      priv->axis = g_value_get_enum (value);
      break;

    case PROP_CONTAINMENT_TYPE:
      priv->containment = g_value_get_enum (value);
      break;

    case PROP_CONTAINMENT_AREA:
      {
        ClutterActorBox *box = g_value_get_boxed (value);

        if (box)
          priv->area = *box;
        else
          memset (&priv->area, 0, sizeof (ClutterActorBox));
      }
      break;

    case PROP_ENABLED:
      priv->enabled = g_value_get_boolean (value);
      if (priv->enabled)
        nbtk_draggable_enable (NBTK_DRAGGABLE (gobject));
      else
        nbtk_draggable_disable (NBTK_DRAGGABLE (gobject));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
mnb_switcher_app_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  MnbSwitcherAppPrivate *priv = MNB_SWITCHER_APP (gobject)->priv;

  switch (prop_id)
    {
    case PROP_MUTTER_WINDOW:
      g_value_set_object (value, priv->mw);
      break;
    case PROP_DRAG_THRESHOLD:
      g_value_set_uint (value, priv->threshold);
      break;

    case PROP_AXIS:
      g_value_set_enum (value, priv->axis);
      break;

    case PROP_CONTAINMENT_TYPE:
      g_value_set_enum (value, priv->containment);
      break;

    case PROP_CONTAINMENT_AREA:
      g_value_set_boxed (value, &priv->area);
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, priv->enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

/*
 * An implemntation of the MnbSwitcherItem::activate() vfunction
 *
 * Activate the associated window.
 */
static gboolean
mnb_switcher_app_activate (MnbSwitcherItem *item)
{
  MnbSwitcherAppPrivate *priv   = MNB_SWITCHER_APP (item)->priv;
  MnbSwitcher           *switcher;
  MetaWindow            *window;
  MetaWorkspace         *workspace;
  MetaWorkspace         *active_workspace;
  MetaScreen            *screen;
  guint32                timestamp;

  switcher = mnb_switcher_item_get_switcher (MNB_SWITCHER_ITEM (item));

  window           = mutter_window_get_meta_window (priv->mw);
  screen           = meta_window_get_screen (window);
  workspace        = meta_window_get_workspace (window);
  active_workspace = meta_screen_get_active_workspace (screen);
  timestamp        = clutter_x11_get_current_event_time ();

  mnb_switcher_end_kbd_grab (switcher);
  clutter_ungrab_pointer ();

  mnb_drop_down_hide_with_toolbar (MNB_DROP_DOWN (switcher));

  if (!active_workspace || (active_workspace == workspace))
    {
      meta_window_activate_with_workspace (window, timestamp, workspace);
    }
  else
    {
      meta_workspace_activate_with_focus (workspace, window, timestamp);
    }

  return TRUE;
}

/*
 * Closures for ClutterActor::button-press/release-event
 *
 * Even when the the press-motion-release sequence is interpreted as a drag,
 * we still get the press and release events because of the way the
 * NbtkDraggable implementation is done. So, we have custom closures that
 * prevent the events from propagating.
 */
static gboolean
mnb_switcher_app_button_press_event (ClutterActor         *actor,
                                     ClutterButtonEvent   *event)
{
  /*
   * We never ever want press events propagated; the way d&d works, they are
   * pretty meaningless, and any custom handling needs to be done on the
   * button releases anyway.
   */
  return TRUE;
}

static gboolean
mnb_switcher_app_button_release_event (ClutterActor         *actor,
                                       ClutterButtonEvent   *event)
{
  MnbSwitcherAppPrivate *priv   = MNB_SWITCHER_APP (actor)->priv;
  gboolean               ignore = priv->ignore_button_release;
  MnbSwitcher           *switcher;

  /*
   * If the ignore_button_release flag is set, we stop it from propagating
   * to parents.
   */
  priv->ignore_button_release = FALSE;

  if (ignore)
    return ignore;

  if (mnb_switcher_get_dnd_in_progress (switcher))
    return FALSE;

  /*
   * If we got this far, this is just a regular button release, so we activate
   * the associated window.
   */
  mnb_switcher_app_activate (MNB_SWITCHER_ITEM (actor));

  return FALSE;
}

/*
 * Callback for when our associated window get/looses focus.
 *
 * TODO -- do we actually care ?
 */
static void
meta_window_focus_cb (MetaWindow *mw,  GParamSpec *pspec, gpointer data)
{
  MnbSwitcherItem        *item        = MNB_SWITCHER_ITEM (data);
  MnbSwitcherApp         *app         = MNB_SWITCHER_APP (data);
  MnbSwitcherItemClass   *klass       = MNB_SWITCHER_ITEM_GET_CLASS (item);
  const gchar            *active_name = "";
  gboolean                focused;
  MnbSwitcher            *switcher;

  if (klass->active_style)
    active_name = klass->active_style  (item);

  switcher = mnb_switcher_item_get_switcher (item);

  if (mnb_switcher_is_constructing (switcher))
    return;

  g_object_get (mw, "has-focus", &focused, NULL);

  if (focused)
    clutter_actor_set_name (CLUTTER_ACTOR (app), active_name);
  else
    clutter_actor_set_name (CLUTTER_ACTOR (app), "");
}

/*
 * We need to track the life span of the associated window.
 */
static void
mnb_switcher_app_origin_weak_notify (gpointer data, GObject *obj)
{
  ClutterActor *self = data;

  /*
   * The original MutterWindow destroyed, destroy self.
   */
  clutter_actor_destroy (self);
}

static void
mnb_switcher_app_constructed (GObject *self)
{
  ClutterActor          *actor    = CLUTTER_ACTOR (self);
  MnbSwitcherAppPrivate *priv     = MNB_SWITCHER_APP (self)->priv;
  MetaWindow            *meta_win = mutter_window_get_meta_window (priv->mw);
  const gchar           *title    = meta_window_get_title (meta_win);
  ClutterActor          *texture, *c_tx;

  if (G_OBJECT_CLASS (mnb_switcher_app_parent_class)->constructed)
    G_OBJECT_CLASS (mnb_switcher_app_parent_class)->constructed (self);

  nbtk_widget_set_style_class_name (NBTK_WIDGET (self),"switcher-application");

  /*
   * Clone the glx texture in the MutterWindow, and insert it into ourselves.
   */
  texture = mutter_window_get_texture (priv->mw);
  g_object_set (texture, "keep-aspect-ratio", TRUE, NULL);

  c_tx = clutter_clone_new (texture);
  clutter_container_add_actor (CLUTTER_CONTAINER (self), c_tx);
  clutter_actor_set_reactive (actor, TRUE);

  /*
   * Use the window title for tooltip
   */
  if (title)
    mnb_switcher_item_set_tooltip (MNB_SWITCHER_ITEM (self), title);

  priv->focus_id =
    g_signal_connect (meta_win, "notify::has-focus",
                      G_CALLBACK (meta_window_focus_cb), self);

  g_object_weak_ref (G_OBJECT (priv->mw),
                     mnb_switcher_app_origin_weak_notify, self);
}

static const gchar *
mnb_switcher_app_active_style (MnbSwitcherItem *item)
{
  return "switcher-application-active";
}

static void
mnb_switcher_app_class_init (MnbSwitcherAppClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass    *actor_class  = CLUTTER_ACTOR_CLASS (klass);
  MnbSwitcherItemClass *item_class   = MNB_SWITCHER_ITEM_CLASS (klass);

  object_class->dispose              = mnb_switcher_app_dispose;
  object_class->get_property         = mnb_switcher_app_get_property;
  object_class->set_property         = mnb_switcher_app_set_property;
  object_class->constructed          = mnb_switcher_app_constructed;

  actor_class->button_release_event  = mnb_switcher_app_button_release_event;
  actor_class->button_press_event    = mnb_switcher_app_button_press_event;

  item_class->active_style           = mnb_switcher_app_active_style;
  item_class->activate               = mnb_switcher_app_activate;

  g_type_class_add_private (klass, sizeof (MnbSwitcherAppPrivate));

  g_object_class_install_property (object_class,
                                   PROP_MUTTER_WINDOW,
                                   g_param_spec_object ("mutter-window",
                                                        "Mutter Window",
                                                        "Mutter Window",
                                                        MUTTER_TYPE_COMP_WINDOW,
                                                        G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY));

  g_object_class_override_property (object_class,
                                    PROP_DRAG_THRESHOLD,
                                    "drag-threshold");
  g_object_class_override_property (object_class,
                                    PROP_AXIS,
                                    "axis");
  g_object_class_override_property (object_class,
                                    PROP_CONTAINMENT_TYPE,
                                    "containment-type");
  g_object_class_override_property (object_class,
                                    PROP_CONTAINMENT_AREA,
                                    "containment-area");
  g_object_class_override_property (object_class,
                                    PROP_ENABLED,
                                    "enabled");
}

static void
mnb_switcher_app_init (MnbSwitcherApp *self)
{
  MnbSwitcherAppPrivate *priv;

  priv = self->priv = MNB_SWITCHER_APP_GET_PRIVATE (self);

  priv->threshold = 5;
  priv->axis = 0;
  priv->containment = NBTK_DISABLE_CONTAINMENT;
  priv->enabled = TRUE;
}

MutterWindow *
mnb_switcher_app_get_window (MnbSwitcherApp *self)
{
  MnbSwitcherAppPrivate *priv = self->priv;

  return priv->mw;
}

MnbSwitcherApp *
mnb_switcher_app_new (MnbSwitcher  *switcher, MutterWindow *mw)
{
  return g_object_new (MNB_TYPE_SWITCHER_APP,
                       "switcher",      switcher,
                       "mutter-window", mw,
                       NULL);
}

ClutterActor *
mnb_switcher_app_get_pre_drag_parent (MnbSwitcherApp *self)
{
  MnbSwitcherAppPrivate *priv = self->priv;

  return (ClutterActor*)priv->orig_zone;
}