/*
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include <libmissioncontrol/mission-control.h>

#include "mnb-im-status-row.h"
#include "mnb-status-marshal.h"

#define MNB_IM_STATUS_ROW_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MNB_TYPE_IM_STATUS_ROW, MnbIMStatusRowPrivate))

#define ICON_SIZE       48.0
#define H_PADDING       9.0

struct _MnbIMStatusRowPrivate
{
  ClutterActor *header;
  ClutterActor *status_grid;

  ClutterActor *user_icon;
  ClutterActor *presence_icon;
  ClutterActor *status_label;
  ClutterActor *account_label;

  ClutterActor *expand_icon;
  ClutterActor *expand_label;
  ClutterActor *expand_box;

  gchar *account_name;
  gchar *display_name;

  gchar *no_icon_file;

  gfloat spacing;

  guint in_hover    : 1;
  guint is_online   : 1;
  guint is_expanded : 1;

  McAccount *account;

  TpConnectionPresenceType presence;
  gchar *status;

  ClutterTimeline *timeline;
  ClutterAlpha *alpha;

  gdouble progress;
};

enum
{
  PROP_0,

  PROP_ACCOUNT_NAME
};

enum
{
  STATUS_CHANGED,

  LAST_SIGNAL
};

G_DEFINE_TYPE (MnbIMStatusRow, mnb_im_status_row, NBTK_TYPE_WIDGET);

static const struct {
  TpConnectionPresenceType presence;
  const gchar *filename;
  const gchar *status_msg;
  guint is_visible    : 1;
  guint is_selectable : 1;
} presence_states[] = {
  { TP_CONNECTION_PRESENCE_TYPE_OFFLINE,       "im-offline.png",   N_("Offline"),   TRUE,  TRUE  },
  { TP_CONNECTION_PRESENCE_TYPE_UNSET,         "im-offline.png",   N_("Offline"),   FALSE, FALSE },
  { TP_CONNECTION_PRESENCE_TYPE_AVAILABLE,     "im-available.png", N_("Available"), TRUE,  TRUE  },
  { TP_CONNECTION_PRESENCE_TYPE_AWAY,          "im-away.png",      N_("Away"),      TRUE,  TRUE  },
  { TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY, "im-away.png",      N_("Away"),      TRUE,  FALSE },
  { TP_CONNECTION_PRESENCE_TYPE_BUSY,          "im-busy.png",      N_("Busy"),      TRUE,  TRUE  },
  { TP_CONNECTION_PRESENCE_TYPE_HIDDEN,        "im-busy.png",      N_("Busy"),      TRUE,  FALSE }
};

static const int n_presence_states = G_N_ELEMENTS (presence_states);

static guint row_signals[LAST_SIGNAL] = { 0, };

static gboolean
on_expand_clicked (ClutterActor   *box,
                   ClutterEvent   *event,
                   MnbIMStatusRow *row)
{
  MnbIMStatusRowPrivate *priv = row->priv;

  if (clutter_event_get_button (event) != 1)
    return FALSE;

  priv->is_expanded = !priv->is_expanded;

  if (priv->is_expanded)
    {
      clutter_timeline_set_direction (priv->timeline, CLUTTER_TIMELINE_FORWARD);

      nbtk_label_set_text (NBTK_LABEL (priv->expand_label), _("Close"));
      nbtk_widget_set_style_class_name (NBTK_WIDGET (priv->expand_icon),
                                        "MnbImExpandIconClose");
    }
  else
    {
      clutter_actor_hide (priv->status_grid);
      clutter_timeline_set_direction (priv->timeline, CLUTTER_TIMELINE_BACKWARD);

      nbtk_label_set_text (NBTK_LABEL (priv->expand_label), _("Change"));
      nbtk_widget_set_style_class_name (NBTK_WIDGET (priv->expand_icon),
                                        "MnbImExpandIconOpen");
    }

  if (!clutter_timeline_is_playing (priv->timeline))
    clutter_timeline_rewind (priv->timeline);

  clutter_timeline_start (priv->timeline);

  return TRUE;
}

static void
on_timeline_completed (ClutterTimeline *timeline,
                       MnbIMStatusRow  *row)
{
  MnbIMStatusRowPrivate *priv = row->priv;

  if (!priv->is_expanded)
    return;

  clutter_actor_set_opacity (priv->status_grid, 0);
  clutter_actor_show (priv->status_grid);
  clutter_actor_animate (priv->status_grid, CLUTTER_LINEAR, 100,
                         "opacity", 254,
                         NULL);
}

static void
on_timeline_frame (ClutterTimeline *timeline,
                   gint             elapsed_msecs,
                   MnbIMStatusRow  *row)
{
  MnbIMStatusRowPrivate *priv = row->priv;

  priv->progress = clutter_alpha_get_alpha (priv->alpha);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (row));
}

static gboolean
on_presence_row_clicked (ClutterActor   *actor,
                         ClutterEvent   *event,
                         MnbIMStatusRow *row)
{
  gint id;

  if (clutter_event_get_button (event) != 1)
    return FALSE;

  id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (actor), "presence-id"));

  g_assert (id >= 0 && id < n_presence_states);

  g_signal_emit (row, row_signals[STATUS_CHANGED], 0,
                 presence_states[id].presence,
                 presence_states[id].status_msg);

  return on_expand_clicked (row->priv->expand_box, event, row);
}

static void
mnb_im_status_row_get_preferred_width (ClutterActor *actor,
                                       gfloat        for_height,
                                       gfloat       *min_width_p,
                                       gfloat       *natural_width_p)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;
  NbtkPadding padding = { 0, };
  gfloat header_natural_width = 0;

  nbtk_widget_get_padding (NBTK_WIDGET (actor), &padding);

  clutter_actor_get_preferred_width (priv->header, for_height,
                                     NULL,
                                     &header_natural_width);

  if (min_width_p)
    *min_width_p = padding.left + ICON_SIZE + padding.right;

  if (natural_width_p)
    *natural_width_p = padding.left
                     + ICON_SIZE + H_PADDING + header_natural_width
                     + padding.right;
}

static void
mnb_im_status_row_get_preferred_height (ClutterActor *actor,
                                        gfloat        for_width,
                                        gfloat       *min_height_p,
                                        gfloat       *natural_height_p)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;
  NbtkPadding padding = { 0, };
  gfloat min_height, natural_height;
  gfloat grid_min, grid_natural;

  nbtk_widget_get_padding (NBTK_WIDGET (actor), &padding);

  min_height = natural_height = padding.top
                              + ICON_SIZE
                              + padding.bottom;

  clutter_actor_get_preferred_height (priv->status_grid, for_width,
                                      &grid_min,
                                      &grid_natural);

  min_height     += (priv->spacing + (grid_min     * priv->progress));
  natural_height += (priv->spacing + (grid_natural * priv->progress));

  if (min_height_p)
    *min_height_p = min_height;

  if (natural_height_p)
    *natural_height_p = natural_height;
}

static void
mnb_im_status_row_allocate (ClutterActor           *actor,
                            const ClutterActorBox  *box,
                            ClutterAllocationFlags  flags)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;
  ClutterActorClass *parent_class;
  NbtkPadding padding = { 0, };
  gfloat available_width, available_height;
  gfloat min_width, min_height, natural_width, natural_height;
  gfloat child_width, child_height;
  gfloat button_width, button_height;
  ClutterActorBox child_box = { 0, };

  parent_class = CLUTTER_ACTOR_CLASS (mnb_im_status_row_parent_class);
  parent_class->allocate (actor, box, flags);

  nbtk_widget_get_padding (NBTK_WIDGET (actor), &padding);

  available_width  = (box->x2 - box->x1)
                   - padding.left
                   - padding.right;
  available_height = (box->y2 - box->y1)
                   - padding.top
                   - padding.bottom;

  child_box.x1 = (int) padding.left;
  child_box.y1 = (int) padding.top;
  child_box.x2 = (int) (child_box.x1 + ICON_SIZE);
  child_box.y2 = (int) (child_box.y1 + ICON_SIZE);
  clutter_actor_allocate (priv->user_icon, &child_box, flags);

  /* the "Change" button must sit on the right side so we
   * get its preferred size first, which will be used to
   * compute the new available width for the header
   */
  clutter_actor_get_preferred_width (priv->expand_box, available_height,
                                     &min_width,
                                     &natural_width);
  child_width = CLAMP (natural_width, min_width, available_width);
  clutter_actor_get_preferred_height (priv->expand_box, child_width,
                                      &min_height,
                                      &natural_height);
  child_height = CLAMP (natural_height, min_height, available_height);

  /* reduce the available width by the size of the button + spacing */
  available_width -= (ICON_SIZE
                      - (priv->spacing * 2)
                      - child_width
                      - (priv->spacing * 2));

  button_width = child_width;
  button_height = child_height;

  /* header, containing the user icon, the presence icon,
   * the status message and the account display name; the
   * width depends on the available width minus the size
   * of the expander button; the height, instead, will
   * determine the position of the expander button
   */
  clutter_actor_get_preferred_width (priv->header, available_height,
                                     &min_width,
                                     &natural_width);
  child_width = CLAMP (natural_width, min_width, available_width);
  clutter_actor_get_preferred_height (priv->header, child_width,
                                      &min_height,
                                      &natural_height);
  child_height = CLAMP (natural_height, min_height, available_height);

  child_box.x1 = (int) padding.left + ICON_SIZE + (priv->spacing * 2);
  child_box.y1 = (int) (padding.top + ((ICON_SIZE - child_height) / 2));
  child_box.x2 = (int) child_box.x1 + child_width;
  child_box.y2 = (int) child_box.y1 + child_height;
  clutter_actor_allocate (priv->header, &child_box, flags);

  /* we want the header button to stay at the same place even when
   * expanding the row
   */
  child_box.x1 = (int) ((box->x2 - box->x1)
               - padding.right
               - button_width
               - (priv->spacing * 2));
  child_box.y1 = (int) (padding.top + ((ICON_SIZE - button_height) / 2));
  child_box.x2 = (int) (child_box.x1 + button_width);
  child_box.y2 = (int) (child_box.y1 + button_height);
  clutter_actor_allocate (priv->expand_box, &child_box, flags);

  /* we allocate the status grid only if it's visible - meaning that the
   * status row has been expanded
   */
  if (CLUTTER_ACTOR_IS_VISIBLE (priv->status_grid))
    {
      clutter_actor_get_preferred_width (priv->status_grid, available_height,
                                         &min_width,
                                         &natural_width);
      child_width = CLAMP (natural_width, min_width, available_width);

      clutter_actor_get_preferred_height (priv->status_grid, child_width,
                                          &min_height,
                                          &natural_height);
      child_height = CLAMP (natural_height, min_height, available_height);

      child_box.x1 = padding.left;
      child_box.y1 = padding.top + ICON_SIZE + priv->spacing;
      child_box.x2 = child_box.x1 + available_width - (ICON_SIZE + (priv->spacing * 2));
      child_box.y2 = child_box.y1 + child_height;
      clutter_actor_allocate (priv->status_grid, &child_box, flags);
    }
}

static void
mnb_im_status_row_paint (ClutterActor *actor)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;

  CLUTTER_ACTOR_CLASS (mnb_im_status_row_parent_class)->paint (actor);

  clutter_actor_paint (priv->user_icon);
  clutter_actor_paint (priv->header);
  clutter_actor_paint (priv->expand_box);

  if (CLUTTER_ACTOR_IS_MAPPED (priv->status_grid))
    clutter_actor_paint (priv->status_grid);
}

static void
mnb_im_status_row_pick (ClutterActor       *actor,
                        const ClutterColor *pick_color)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;

  CLUTTER_ACTOR_CLASS (mnb_im_status_row_parent_class)->pick (actor, pick_color);

  clutter_actor_paint (priv->expand_box);
  clutter_actor_paint (priv->status_grid);
}

static void
mnb_im_status_row_map (ClutterActor *actor)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;

  CLUTTER_ACTOR_CLASS (mnb_im_status_row_parent_class)->map (actor);

  clutter_actor_map (priv->user_icon);
  clutter_actor_map (priv->header);
  clutter_actor_map (priv->expand_box);
  clutter_actor_map (priv->status_grid);
}

static void
mnb_im_status_row_unmap (ClutterActor *actor)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;

  CLUTTER_ACTOR_CLASS (mnb_im_status_row_parent_class)->unmap (actor);

  clutter_actor_unmap (priv->user_icon);
  clutter_actor_unmap (priv->header);
  clutter_actor_unmap (priv->expand_box);
  clutter_actor_unmap (priv->status_grid);
}

static gboolean
mnb_im_status_row_enter (ClutterActor *actor,
                      ClutterCrossingEvent *event)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;

  priv->in_hover = TRUE;

  nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (priv->expand_icon), "hover");

  return TRUE;
}

static gboolean
mnb_im_status_row_leave (ClutterActor *actor,
                        ClutterCrossingEvent *event)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (actor)->priv;

  priv->in_hover = FALSE;

  nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (priv->expand_icon), NULL);

  return TRUE;
}

static void
mnb_im_status_row_style_changed (NbtkWidget *widget)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (widget)->priv;

#if 0
  nbtk_stylable_get (NBTK_STYLABLE (widget),
                     "spacing", &priv->spacing,
                     NULL);
#endif

  g_signal_emit_by_name (priv->header, "style-changed");
  g_signal_emit_by_name (priv->expand_box, "style-changed");
  g_signal_emit_by_name (priv->status_grid, "style-changed");
}

static void
mnb_im_status_row_finalize (GObject *gobject)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (gobject)->priv;

  if (priv->account)
    g_object_unref (priv->account);

  /* this will take care of the timeline as well */
  if (priv->alpha)
    g_object_unref (priv->alpha);

  g_free (priv->account_name);
  g_free (priv->display_name);
  g_free (priv->no_icon_file);

  clutter_actor_destroy (priv->user_icon);
  clutter_actor_destroy (priv->header);
  clutter_actor_destroy (priv->expand_box);
  clutter_actor_destroy (priv->status_grid);

  G_OBJECT_CLASS (mnb_im_status_row_parent_class)->finalize (gobject);
}

static void
mnb_im_status_row_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ACCOUNT_NAME:
      g_free (priv->account_name);
      priv->account_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
mnb_im_status_row_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  MnbIMStatusRowPrivate *priv = MNB_IM_STATUS_ROW (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ACCOUNT_NAME:
      g_value_set_string (value, priv->account_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
mnb_im_status_row_constructed (GObject *gobject)
{
  MnbIMStatusRow *row = MNB_IM_STATUS_ROW (gobject);
  MnbIMStatusRowPrivate *priv = row->priv;
  gchar *name;

  g_assert (priv->account_name != NULL);

  priv->account = mc_account_lookup (priv->account_name);
  priv->display_name = g_strdup (mc_account_get_display_name (priv->account));
  priv->is_online = FALSE;

  name = g_strconcat (" - ", priv->display_name, NULL);
  nbtk_label_set_text (NBTK_LABEL (priv->account_label), name);
  g_free (name);

  if (mc_account_get_avatar (priv->account, &name, NULL, NULL))
    {
      GError *error = NULL;

      clutter_texture_set_load_async (CLUTTER_TEXTURE (priv->user_icon), TRUE);
      clutter_texture_set_from_file (CLUTTER_TEXTURE (priv->user_icon),
                                     name,
                                     &error);
      if (error)
        {
          g_warning ("Unable to load avatar image: %s", error->message);
          g_error_free (error);

          clutter_texture_set_from_file (CLUTTER_TEXTURE (priv->user_icon),
                                         priv->no_icon_file,
                                         NULL);
        }

      g_free (name);
    }

  if (G_OBJECT_CLASS (mnb_im_status_row_parent_class)->constructed)
    G_OBJECT_CLASS (mnb_im_status_row_parent_class)->constructed (gobject);
}

static void
mnb_im_status_row_class_init (MnbIMStatusRowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MnbIMStatusRowPrivate));

  gobject_class->constructed = mnb_im_status_row_constructed;
  gobject_class->set_property = mnb_im_status_row_set_property;
  gobject_class->get_property = mnb_im_status_row_get_property;
  gobject_class->finalize = mnb_im_status_row_finalize;

  actor_class->get_preferred_width = mnb_im_status_row_get_preferred_width;
  actor_class->get_preferred_height = mnb_im_status_row_get_preferred_height;
  actor_class->allocate = mnb_im_status_row_allocate;
  actor_class->paint = mnb_im_status_row_paint;
  actor_class->pick = mnb_im_status_row_pick;
  actor_class->map = mnb_im_status_row_map;
  actor_class->unmap = mnb_im_status_row_unmap;
  actor_class->enter_event = mnb_im_status_row_enter;
  actor_class->leave_event = mnb_im_status_row_leave;

  pspec = g_param_spec_string ("account-name",
                               "Account Name",
                               "The unique name of the MissionControl account",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_ACCOUNT_NAME, pspec);

  row_signals[STATUS_CHANGED] =
    g_signal_new (g_intern_static_string ("status-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  mnb_status_marshal_VOID__INT_STRING,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT,
                  G_TYPE_STRING);
}

static void
mnb_im_status_row_init (MnbIMStatusRow *self)
{
  MnbIMStatusRowPrivate *priv;
  NbtkGrid *grid;
  gchar *file;
  gint i;

  self->priv = priv = MNB_IM_STATUS_ROW_GET_PRIVATE (self);

  priv->timeline = clutter_timeline_new (250);
  clutter_timeline_set_direction (priv->timeline, CLUTTER_TIMELINE_BACKWARD);
  g_signal_connect (priv->timeline, "new-frame", G_CALLBACK (on_timeline_frame), self);
  g_signal_connect (priv->timeline, "completed", G_CALLBACK (on_timeline_completed), self);

  priv->alpha = clutter_alpha_new_full (priv->timeline, CLUTTER_EASE_IN_SINE);
  g_object_ref_sink (priv->alpha);
  g_object_unref (priv->timeline); /* let the alpha handle it */

  priv->spacing = 6.0;

  g_signal_connect (self, "style-changed",
                    G_CALLBACK (mnb_im_status_row_style_changed),
                    NULL);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);

  priv->header = CLUTTER_ACTOR (nbtk_grid_new ());
  grid = NBTK_GRID (priv->header);
  nbtk_grid_set_column_major (grid, FALSE);
  nbtk_grid_set_column_gap (grid, priv->spacing);
  nbtk_grid_set_halign (grid, 0.0);
  nbtk_grid_set_valign (grid, 0.5);
  clutter_actor_set_parent (priv->header, CLUTTER_ACTOR (self));

  file = g_build_filename (THEMEDIR,
                           "no_image_icon.png",
                           NULL);
  priv->user_icon = clutter_texture_new ();
  clutter_texture_set_from_file (CLUTTER_TEXTURE (priv->user_icon), file, NULL);
  priv->no_icon_file = file;

  clutter_actor_set_parent (priv->user_icon, CLUTTER_ACTOR (self));

  file = g_build_filename (THEMEDIR,
                           "im-offline.png",
                           NULL);
  priv->presence_icon = clutter_texture_new ();
  clutter_texture_set_from_file (CLUTTER_TEXTURE (priv->presence_icon), file, NULL);
  g_free (file);

  priv->status_label = CLUTTER_ACTOR (nbtk_label_new (_("Offline")));
  nbtk_widget_set_style_class_name (NBTK_WIDGET (priv->status_label),
                                    "MnbImStatusLabel");
  priv->account_label = CLUTTER_ACTOR (nbtk_label_new (_("Unknown account")));
  nbtk_widget_set_style_class_name (NBTK_WIDGET (priv->account_label),
                                    "MnbImAccountLabel");

  clutter_container_add (CLUTTER_CONTAINER (priv->header),
                         priv->presence_icon,
                         priv->status_label,
                         priv->account_label,
                         NULL);

  /* expand control */
  priv->expand_icon = CLUTTER_ACTOR (nbtk_icon_new ());
  nbtk_widget_set_style_class_name (NBTK_WIDGET (priv->expand_icon),
                                    "MnbImExpandIconOpen");

  priv->expand_label = CLUTTER_ACTOR (nbtk_label_new (_("Change")));
  nbtk_widget_set_style_class_name (NBTK_WIDGET (priv->expand_label),
                                    "MnbImExpandLabel");

  priv->expand_box = CLUTTER_ACTOR (nbtk_grid_new ());
  nbtk_grid_set_column_gap (NBTK_GRID (priv->expand_box), priv->spacing);
  nbtk_grid_set_valign (NBTK_GRID (priv->expand_box), 0.5);
  clutter_container_add (CLUTTER_CONTAINER (priv->expand_box),
                         priv->expand_icon,
                         priv->expand_label,
                         NULL);
  nbtk_widget_set_style_class_name (NBTK_WIDGET (priv->expand_box),
                                    "MnbImExpandButton");
  clutter_actor_set_parent (priv->expand_box, CLUTTER_ACTOR (self));
  g_signal_connect (priv->expand_box,
                    "button-press-event", G_CALLBACK (on_expand_clicked),
                    self);

  priv->status_grid = CLUTTER_ACTOR (nbtk_box_layout_new ());
  nbtk_box_layout_set_vertical (NBTK_BOX_LAYOUT (priv->status_grid), TRUE);
  nbtk_box_layout_set_spacing (NBTK_BOX_LAYOUT (priv->status_grid), 4);
  nbtk_widget_set_style_class_name (NBTK_WIDGET (priv->status_grid),
                                    "MnbImStatusSelector");
  clutter_actor_set_parent (priv->status_grid, CLUTTER_ACTOR (self));
  clutter_actor_set_reactive (priv->status_grid, TRUE);
  clutter_actor_hide (priv->status_grid);

  for (i = 0; i < n_presence_states; i++)
    {
      NbtkWidget *presence_grid;
      NbtkWidget *presence_label;
      ClutterActor *presence_icon;

      if (!presence_states[i].is_selectable)
        continue;

      file = g_build_filename (THEMEDIR,
                               presence_states[i].filename,
                               NULL);

      presence_grid = nbtk_grid_new ();
      g_object_set_data (G_OBJECT (presence_grid), "presence-id", GINT_TO_POINTER (i));
      nbtk_grid_set_column_gap (NBTK_GRID (presence_grid), priv->spacing);
      nbtk_grid_set_halign (NBTK_GRID (presence_grid), 0.0);
      nbtk_grid_set_valign (NBTK_GRID (presence_grid), 0.5);
      nbtk_widget_set_style_class_name (NBTK_WIDGET (presence_grid),
                                        "MnbImPresenceRow");

      clutter_actor_set_reactive (CLUTTER_ACTOR (presence_grid), TRUE);
      g_signal_connect (presence_grid, "button-press-event",
                        G_CALLBACK (on_presence_row_clicked),
                        self);

      presence_icon = clutter_texture_new ();
      clutter_texture_set_from_file (CLUTTER_TEXTURE (presence_icon), file, NULL);

      presence_label = nbtk_label_new (gettext (presence_states[i].status_msg));

      /* FIXME - ugh, this sucks, but NbtkGrid does not allow expanding
       * a child to cover the whole allocation and NBTK does not have
       * a non-reflowing box that I can use
       */
      //clutter_actor_set_width (CLUTTER_ACTOR (presence_label), 900);

      clutter_container_add (CLUTTER_CONTAINER (presence_grid),
                             presence_icon,
                             CLUTTER_ACTOR (presence_label),
                             NULL);

      clutter_container_add_actor (CLUTTER_CONTAINER (priv->status_grid),
                                   CLUTTER_ACTOR (presence_grid));

      g_free (file);
    }
}

NbtkWidget *
mnb_im_status_row_new (const gchar *account_name)
{
  g_return_val_if_fail (account_name != NULL, NULL);

  return g_object_new (MNB_TYPE_IM_STATUS_ROW,
                       "account-name", account_name,
                       NULL);
}

void
mnb_im_status_row_set_online (MnbIMStatusRow *row,
                              gboolean        is_online)
{
  MnbIMStatusRowPrivate *priv;

  g_return_if_fail (MNB_IS_IM_STATUS_ROW (row));

  priv = row->priv;

  priv->is_online = is_online ? TRUE : FALSE;

  clutter_actor_set_opacity (CLUTTER_ACTOR (priv->user_icon),
                             priv->is_online ? 255 :128);
  clutter_actor_set_opacity (CLUTTER_ACTOR (priv->presence_icon),
                             priv->is_online ? 255 :128);
  clutter_actor_set_opacity (CLUTTER_ACTOR (priv->status_label),
                             priv->is_online ? 255 :128);
  clutter_actor_set_opacity (CLUTTER_ACTOR (priv->account_label),
                             priv->is_online ? 255 :128);
  clutter_actor_set_opacity (CLUTTER_ACTOR (priv->expand_box),
                             priv->is_online ? 255 :128);
  clutter_actor_set_reactive (priv->expand_box,
                              priv->is_online ? TRUE : FALSE);
}

G_CONST_RETURN gchar *
mnb_im_status_row_get_status (MnbIMStatusRow           *row,
                              TpConnectionPresenceType *presence)
{
  g_return_val_if_fail (MNB_IS_IM_STATUS_ROW (row), NULL);

  if (presence)
    *presence = row->priv->presence;

  return row->priv->status;
}

void
mnb_im_status_row_set_status (MnbIMStatusRow           *row,
                              TpConnectionPresenceType  presence,
                              const gchar              *status)
{
  MnbIMStatusRowPrivate *priv;
  const gchar *status_file = NULL;
  gchar *file;
  gint i;

  g_return_if_fail (MNB_IS_IM_STATUS_ROW (row));

  priv = row->priv;

  priv->presence = presence;

  for (i = 0; i < n_presence_states; i++)
    {
      if (presence_states[i].presence == priv->presence)
        {
          if (status == NULL || *status == '\0')
            status = presence_states[i].status_msg;

          status_file = presence_states[i].filename;
          break;
        }
    }

  g_assert (status_file != NULL);

  g_free (priv->status);
  priv->status = g_strdup (status);

  file = g_build_filename (THEMEDIR,
                           status_file,
                           NULL);
  clutter_texture_set_from_file (CLUTTER_TEXTURE (priv->presence_icon), file, NULL);
  g_free (file);

  if (priv->status != NULL)
    nbtk_label_set_text (NBTK_LABEL (priv->status_label), priv->status);
}
