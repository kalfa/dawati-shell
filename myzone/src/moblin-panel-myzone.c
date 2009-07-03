#include <glib/gi18n.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <nbtk/nbtk.h>
#include <moblin-panel/mpl-panel-clutter.h>
#include <moblin-panel/mpl-panel-common.h>
#include <penge/penge-grid-view.h>

#include <config.h>

static void
_client_set_size_cb (MplPanelClient *client,
                     guint           width,
                     guint           height,
                     gpointer        userdata)
{
  clutter_actor_set_size ((ClutterActor *)userdata,
                          width,
                          height);

  g_debug (G_STRLOC ": Dimensions for grid view: %d x %d",
           width,
           height);
}

static void
_grid_view_activated_cb (PengeGridView *grid_view,
                         gpointer       userdata)
{
  mpl_panel_client_request_hide ((MplPanelClient *)userdata);
}

static gboolean standalone = FALSE;

static GOptionEntry entries[] = {
  {"standalone", 's', 0, G_OPTION_ARG_NONE, &standalone, "Do not embed into the mutter-moblin panel", NULL}
};

int
main (int    argc,
      char **argv)
{
  MplPanelClient *client;
  ClutterActor *stage;
  NbtkWidget *grid_view;
  GOptionContext *context;
  GError *error = NULL;

  context = g_option_context_new ("- mutter-moblin myzone panel");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, clutter_get_option_group_without_init ());
  g_option_context_add_group (context, gtk_get_option_group (FALSE));
  if (!g_option_context_parse (context, &argc, &argv, &error))
  {
    g_critical (G_STRLOC ": Error parsing option: %s", error->message);
    g_clear_error (&error);
  }
  g_option_context_free (context);

  MPL_PANEL_CLUTTER_INIT_WITH_GTK (&argc, &argv);

  nbtk_texture_cache_load_cache (nbtk_texture_cache_get_default (),
                                 NBTK_CACHE);
  nbtk_style_load_from_file (nbtk_style_get_default (),
                             THEMEDIR "/panel.css", NULL);

  if (!standalone)
  {
    client = mpl_panel_clutter_new (MPL_PANEL_MYZONE,
                                    _("myzone"),
                                    NULL,
                                    "myzone-button",
                                    TRUE);

    MPL_PANEL_CLUTTER_SETUP_EVENTS_WITH_GTK (client);

    stage = mpl_panel_clutter_get_stage (MPL_PANEL_CLUTTER (client));

    grid_view = g_object_new (PENGE_TYPE_GRID_VIEW,
                              "panel-client",
                              client,
                              NULL);
    clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                                 (ClutterActor *)grid_view);
    g_signal_connect (client,
                      "set-size",
                      (GCallback)_client_set_size_cb,
                      grid_view);

    g_signal_connect (grid_view,
                      "activated",
                      (GCallback)_grid_view_activated_cb,
                      client);
  } else {
    Window xwin;

    stage = clutter_stage_get_default ();
    clutter_actor_realize (stage);
    xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

    MPL_PANEL_CLUTTER_SETUP_EVENTS_WITH_GTK_FOR_XID (xwin);

    grid_view = g_object_new (PENGE_TYPE_GRID_VIEW,
                              NULL);
    clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                                 (ClutterActor *)grid_view);
    clutter_actor_set_size ((ClutterActor *)grid_view, 1016, 504);
    clutter_actor_set_size (stage, 1016, 504);
    clutter_actor_show_all (stage);
  }

  clutter_main ();

  return 0;
}
