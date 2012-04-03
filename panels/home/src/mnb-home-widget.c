/*
 * Copyright (c) 2012 Intel Corp.
 *
 * Author: Danielle Madeley <danielle.madeley@collabora.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glib/gi18n.h>

#include "mnb-home-new-widget-dialog.h"
#include "mnb-home-plugins-engine.h"
#include "mnb-home-widget.h"
#include "utils.h"

#include "dawati-home-plugins-app.h"

#define GSETTINGS_PLUGIN_PATH_PREFIX "/org/dawati/shell/home/plugin/"

G_DEFINE_TYPE (MnbHomeWidget, mnb_home_widget, MX_TYPE_FRAME);

enum /* properties */
{
  PROP_0,
  PROP_ROW,
  PROP_COLUMN,
  PROP_OBJECT_PATH,
  PROP_EDIT_MODE,
  PROP_LAST
};

struct _MnbHomeWidgetPrivate
{
  GSettings *settings;
  MnbHomePluginsEngine *engine;
  DawatiHomePluginsApp *app;

  guint row;
  guint column;
  gchar *object_path;
  gboolean edit_mode;
  char *module;
};

static void
mnb_home_widget_get_property (GObject *self,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  MnbHomeWidgetPrivate *priv = MNB_HOME_WIDGET (self)->priv;

  switch (prop_id)
    {
      case PROP_ROW:
        g_value_set_uint (value, priv->row);
        break;

      case PROP_COLUMN:
        g_value_set_uint (value, priv->column);
        break;

      case PROP_OBJECT_PATH:
        g_value_set_string (value, priv->object_path);
        break;

      case PROP_EDIT_MODE:
        g_value_set_boolean (value, priv->edit_mode);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
        break;
    }
}

static void
mnb_home_widget_set_property (GObject *self,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec)
{
  MnbHomeWidgetPrivate *priv = MNB_HOME_WIDGET (self)->priv;
  guint col, row;

  switch (prop_id)
    {
      case PROP_ROW:
        priv->row = g_value_get_uint (value);
        DEBUG ("PROP_ROW changed to %d", priv->row);
        break;

      case PROP_COLUMN:
        priv->column = g_value_get_uint (value);
        DEBUG ("PROP_COLUMN changed to %d", priv->column);
        break;
        break;

      case PROP_OBJECT_PATH:
        priv->object_path = g_strdup (g_value_get_string (value));
        break;

      case PROP_EDIT_MODE:
        mnb_home_widget_set_edit_mode (MNB_HOME_WIDGET (self),
            g_value_get_boolean (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
        break;
    }
}

static void
home_widget_module_changed (GSettings *settings,
    char *key,
    MnbHomeWidget *self)
{
  g_free (self->priv->module);
  self->priv->module = g_settings_get_string (settings, key);

  if (self->priv->app != NULL)
    dawati_home_plugins_app_deinit (self->priv->app);

  g_clear_object (&self->priv->app);

  if (STR_EMPTY (self->priv->module))
    {
      DEBUG ("no module");
    }
  else
    {
      DEBUG ("module = '%s'", self->priv->module);

      if (self->priv->engine == NULL)
        self->priv->engine = mnb_home_plugins_engine_dup ();

      self->priv->app = mnb_home_plugins_engine_create_app (self->priv->engine,
          self->priv->module, self->priv->object_path);
      if (self->priv->app != NULL)
        dawati_home_plugins_app_init (self->priv->app);
    }

  /* reload the widget */
  mnb_home_widget_set_edit_mode (self, self->priv->edit_mode);
}

static void
mnb_home_widget_constructed (GObject *self)
{
  MnbHomeWidgetPrivate *priv = MNB_HOME_WIDGET (self)->priv;
  gchar *p = NULL;

  g_object_get (self, "object-path", &p, NULL);

  DEBUG ("widget path = '%s'", p);

  priv->settings = g_settings_new_with_path ("org.dawati.shell.home.plugin",
      priv->object_path);

  g_signal_connect (priv->settings, "changed::module",
      G_CALLBACK (home_widget_module_changed), self);
  home_widget_module_changed (priv->settings, "module",
      MNB_HOME_WIDGET (self));

  /* for now let's not monitor changes, no one else is supposed to change it
   */
  priv->column = g_settings_get_uint (priv->settings, "column");
  priv->row = g_settings_get_uint (priv->settings, "row");
}

static void
mnb_home_widget_dispose (GObject *self)
{
  MnbHomeWidgetPrivate *priv = MNB_HOME_WIDGET (self)->priv;

  g_clear_object (&priv->settings);
  g_clear_object (&priv->engine);
  g_clear_object (&priv->app);

  G_OBJECT_CLASS (mnb_home_widget_parent_class)->dispose (self);
}

static void
mnb_home_widget_finalize (GObject *self)
{
  MnbHomeWidgetPrivate *priv = MNB_HOME_WIDGET (self)->priv;

  g_free (priv->module);
  g_free (priv->object_path);

  G_OBJECT_CLASS (mnb_home_widget_parent_class)->finalize (self);
}

static void
mnb_home_widget_class_init (MnbHomeWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = mnb_home_widget_get_property;
  gobject_class->set_property = mnb_home_widget_set_property;
  gobject_class->constructed = mnb_home_widget_constructed;
  gobject_class->dispose = mnb_home_widget_dispose;
  gobject_class->finalize = mnb_home_widget_finalize;

  g_type_class_add_private (gobject_class, sizeof (MnbHomeWidgetPrivate));

  g_object_class_install_property (gobject_class, PROP_ROW,
      g_param_spec_uint ("row",
        "Row",
        "",
        0, G_MAXUINT, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_COLUMN,
      g_param_spec_uint ("column",
        "Column",
        "",
        0, G_MAXUINT, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_OBJECT_PATH,
      g_param_spec_string ("object-path",
        "ObjectPath",
        "The GSettings object path for the widget",
        NULL,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_EDIT_MODE,
      g_param_spec_boolean ("edit-mode",
        "Edit Mode",
        "%TRUE if we are editing the settings for this widget",
        FALSE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
mnb_home_widget_init (MnbHomeWidget *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MNB_TYPE_HOME_WIDGET,
      MnbHomeWidgetPrivate);

  g_object_set (self,
      "x-fill", TRUE,
      "y-fill", TRUE,
      NULL);
}

ClutterActor *
mnb_home_widget_new (const char* path)
{
  return g_object_new (MNB_TYPE_HOME_WIDGET,
      "object-path", path,
      NULL);
}

static void
home_widget_add_module_response (ClutterActor *dialog,
    const char *module,
    MnbHomeWidget *self)
{
  if (!STR_EMPTY (module))
    {
      DEBUG ("Adding widget '%s'", module);

      g_settings_set_string (self->priv->settings, "module", module);
    }

  clutter_actor_destroy (dialog);
  /* FIXME: why do I need this */
  clutter_actor_queue_redraw (clutter_actor_get_stage (CLUTTER_ACTOR (self)));
}

static void
home_widget_add_module (MxButton *button,
    MnbHomeWidget *self)
{
  ClutterActor *dialog;

  dialog = mnb_home_new_widget_dialog_new (
      clutter_actor_get_stage (CLUTTER_ACTOR (button)));

  g_signal_connect (dialog, "response",
      G_CALLBACK (home_widget_add_module_response), self);

  clutter_actor_show (dialog);
}

static void
home_widget_remove_module (MxButton *button,
    MnbHomeWidget *self)
{
  char *path;

  path = g_strdup_printf (GSETTINGS_PLUGIN_PATH_PREFIX "%u_%u/",
      self->priv->row, self->priv->column);
  dconf_recursive_unset (path, NULL);

  g_free (path);
}

void
mnb_home_widget_set_edit_mode (MnbHomeWidget *self,
    gboolean edit_mode)
{
  self->priv->edit_mode = edit_mode;

  g_object_notify (G_OBJECT (self), "edit-mode");

  /* FIXME: should hold refs to the actors rather than destroy/recreate them? */
  mx_bin_set_child (MX_BIN (self), NULL);

  /* FIXME: animate this */
  if (edit_mode)
    {
      ClutterActor *table;

      table = mx_table_new ();
      mx_bin_set_child (MX_BIN (self), table);

      if (!STR_EMPTY (self->priv->module))
        {
          ClutterActor *config, *remove;

          remove = mx_button_new_with_label ("x");
          mx_table_insert_actor_with_properties (MX_TABLE (table), remove, 0, 1,
                                                 "x-expand", FALSE,
                                                 "y-expand", FALSE,
                                                 "x-fill", FALSE,
                                                 "y-fill", FALSE,
                                                 NULL);

          g_signal_connect (remove, "clicked",
              G_CALLBACK (home_widget_remove_module), self);

          if (self->priv->app != NULL)
            config = dawati_home_plugins_app_get_configuration (
                self->priv->app);
          else
            config = mx_label_new_with_text (_("Plugin missing"));

          if (CLUTTER_IS_ACTOR (config))
            mx_table_insert_actor_with_properties (MX_TABLE (table), config, 1, 0,
                                                   "x-expand", TRUE,
                                                   "y-expand", TRUE,
                                                   "x-fill", TRUE,
                                                   "y-fill", TRUE,
                                                   NULL);
        }
      else
        {
          ClutterActor *button;

          button = mx_button_new_with_label ("+");
          mx_table_insert_actor (MX_TABLE (table), button, 0, 0);

          g_signal_connect (button, "clicked",
              G_CALLBACK (home_widget_add_module), self);
        }

      clutter_actor_show_all (table);
    }
  else
    {
      ClutterActor *widget = NULL;

      if (self->priv->app != NULL)
        {

          widget = dawati_home_plugins_app_get_widget (self->priv->app);

          if (!CLUTTER_IS_ACTOR (widget))
            /* FIXME: make this better */
            widget = mx_label_new_with_text (_("Broken plugin"));

        }
      else if (!STR_EMPTY (self->priv->module))
        {
          widget = mx_label_new_with_text (_("Plugin missing"));
        }

      if (widget != NULL)
        mx_bin_set_child (MX_BIN (self), widget);
    }
}
