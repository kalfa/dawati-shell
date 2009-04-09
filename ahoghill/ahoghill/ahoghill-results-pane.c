#include <glib/gi18n.h>

#include <nbtk/nbtk.h>

#include <bickley/bkl-item.h>

#include "nbtk-fixed.h"
#include "ahoghill-media-tile.h"
#include "ahoghill-results-pane.h"
#include "ahoghill-results-table.h"

enum {
    PROP_0,
    PROP_TITLE,
};

enum {
    ITEM_CLICKED,
    LAST_SIGNAL
};

#define TILES_PER_ROW 6
#define ROWS_PER_PAGE 2
#define TILES_PER_PAGE (TILES_PER_ROW * ROWS_PER_PAGE)

#define PAGE_CHANGE_DURATION 750

struct _paging_data {
    guint old_anim_id;
    ClutterActor *new_page;
    ClutterAnimation *old_page_anim;
    ClutterAnimation *new_page_anim;
};

struct _AhoghillResultsPanePrivate {
    NbtkWidget *title;
    NbtkWidget *fixed;
    NbtkWidget *previous_button;
    NbtkWidget *next_button;

    GPtrArray *results;

    AhoghillResultsTable *current_page;
    guint current_page_num;
    guint last_page;

    struct _paging_data *animation;
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), AHOGHILL_TYPE_RESULTS_PANE, AhoghillResultsPanePrivate))
G_DEFINE_TYPE (AhoghillResultsPane, ahoghill_results_pane, NBTK_TYPE_TABLE);

#define ROW_SPACING 14

static guint32 signals[LAST_SIGNAL] = { 0, };

static void
ahoghill_results_pane_finalize (GObject *object)
{
    AhoghillResultsPane *pane = (AhoghillResultsPane *) object;
    AhoghillResultsPanePrivate *priv = pane->priv;

    if (priv->animation) {
        g_signal_handler_disconnect (priv->animation->old_page_anim,
                                     priv->animation->old_anim_id);
        clutter_animation_completed (priv->animation->old_page_anim);
        clutter_animation_completed (priv->animation->new_page_anim);

        g_slice_free (struct _paging_data, priv->animation);
    }

    g_signal_handlers_destroy (object);
    G_OBJECT_CLASS (ahoghill_results_pane_parent_class)->finalize (object);
}

static void
ahoghill_results_pane_dispose (GObject *object)
{
    G_OBJECT_CLASS (ahoghill_results_pane_parent_class)->dispose (object);
}

static void
ahoghill_results_pane_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    AhoghillResultsPane *pane = (AhoghillResultsPane *) object;
    AhoghillResultsPanePrivate *priv = pane->priv;

    switch (prop_id) {

    case PROP_TITLE:
        nbtk_label_set_text (NBTK_LABEL (priv->title),
                             g_value_get_string (value));
        break;

    default:
        break;
    }
}

static void
ahoghill_results_pane_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    switch (prop_id) {

    default:
        break;
    }
}

static void
ahoghill_results_pane_class_init (AhoghillResultsPaneClass *klass)
{
    GObjectClass *o_class = (GObjectClass *) klass;

    o_class->dispose = ahoghill_results_pane_dispose;
    o_class->finalize = ahoghill_results_pane_finalize;
    o_class->set_property = ahoghill_results_pane_set_property;
    o_class->get_property = ahoghill_results_pane_get_property;

    g_type_class_add_private (klass, sizeof (AhoghillResultsPanePrivate));

    g_object_class_install_property (o_class, PROP_TITLE,
                                     g_param_spec_string ("title", "", "", "",
                                                          G_PARAM_WRITABLE |
                                                          G_PARAM_STATIC_STRINGS));
    signals[ITEM_CLICKED] = g_signal_new ("item-clicked",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_FIRST |
                                          G_SIGNAL_NO_RECURSE, 0, NULL, NULL,
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE, 1,
                                          BKL_TYPE_ITEM);
}

static void
item_clicked_cb (AhoghillResultsTable *table,
                 int                   tileno,
                 AhoghillResultsPane  *pane)
{
    AhoghillResultsPanePrivate *priv = pane->priv;
    int item_no;
    BklItem *item;

    item_no = (TILES_PER_PAGE * priv->current_page_num) + tileno;
    item = priv->results->pdata[item_no];

    g_signal_emit (pane, signals[ITEM_CLICKED], 0, item);
}

static void
page_change_complete (ClutterAnimation    *anim,
                      AhoghillResultsPane *pane)
{
    AhoghillResultsPanePrivate *priv = pane->priv;

    clutter_actor_destroy ((ClutterActor *) priv->current_page);
    priv->current_page = (AhoghillResultsTable *) priv->animation->new_page;

    g_slice_free (struct _paging_data, priv->animation);
    priv->animation = NULL;
}

static gboolean
show_previous_page (ClutterActor        *actor,
                    ClutterButtonEvent  *event,
                    AhoghillResultsPane *pane)
{
    AhoghillResultsPanePrivate *priv = pane->priv;
    ClutterActor *new_page;
    guint width, height;

    if (priv->current_page_num == 0) {
        return FALSE;
    }

    if (priv->animation) {
        /* Move the new page to its destination */
        clutter_actor_set_position (priv->animation->new_page, 0, 0);

        /* Stop the animations */
        g_signal_handler_disconnect (priv->animation->old_page_anim,
                                     priv->animation->old_anim_id);
        clutter_animation_completed (priv->animation->old_page_anim);
        clutter_animation_completed (priv->animation->new_page_anim);

        /* Destroy old page and replace */
        clutter_actor_destroy (CLUTTER_ACTOR (priv->current_page));
        priv->current_page = (AhoghillResultsTable *) priv->animation->new_page;
    } else {
        priv->animation = g_slice_new (struct _paging_data);
    }

    new_page = g_object_new (AHOGHILL_TYPE_RESULTS_TABLE, NULL);
    ahoghill_results_table_update ((AhoghillResultsTable *) new_page,
                                   priv->results, priv->current_page_num - 1);
    nbtk_fixed_add_actor (NBTK_FIXED (priv->fixed), new_page);
    clutter_actor_get_size (new_page, &width, &height);
    clutter_actor_set_position (new_page, 0 - (int) width, 0);

    priv->animation->new_page = new_page;

    clutter_actor_get_size ((ClutterActor *) priv->current_page,
                            &width, &height);
    priv->animation->old_page_anim = clutter_actor_animate
        (CLUTTER_ACTOR (priv->current_page),
         CLUTTER_EASE_OUT_EXPO, PAGE_CHANGE_DURATION,
         "x", (int) width,
         NULL);
    priv->animation->old_anim_id = g_signal_connect
        (priv->animation->old_page_anim, "completed",
         G_CALLBACK (page_change_complete), pane);

    priv->animation->new_page_anim = clutter_actor_animate
        (new_page, CLUTTER_EASE_OUT_EXPO,
         PAGE_CHANGE_DURATION,
         "x", 0,
         NULL);

    priv->current_page_num--;

    return FALSE;
}

static gboolean
show_next_page (ClutterActor        *actor,
                ClutterButtonEvent  *event,
                AhoghillResultsPane *pane)
{
    AhoghillResultsPanePrivate *priv = pane->priv;
    ClutterActor *new_page;
    guint width, height;

    if (priv->current_page_num == priv->last_page) {
        return FALSE;
    }

    if (priv->animation) {
        /* Move the new page to its destination */
        clutter_actor_set_position (priv->animation->new_page, 0, 0);

        /* Stop the animations */
        g_signal_handler_disconnect (priv->animation->old_page_anim,
                                     priv->animation->old_anim_id);
        clutter_animation_completed (priv->animation->old_page_anim);
        clutter_animation_completed (priv->animation->new_page_anim);

        /* Destroy old page and replace */
        clutter_actor_destroy (CLUTTER_ACTOR (priv->current_page));
        priv->current_page = (AhoghillResultsTable *) priv->animation->new_page;
    } else {
        priv->animation = g_slice_new (struct _paging_data);
    }

    clutter_actor_get_size ((ClutterActor *) priv->current_page,
                            &width, &height);

    new_page = g_object_new (AHOGHILL_TYPE_RESULTS_TABLE, NULL);
    ahoghill_results_table_update ((AhoghillResultsTable *) new_page,
                                   priv->results,
                                   priv->current_page_num + 1);
    nbtk_fixed_add_actor (NBTK_FIXED (priv->fixed), new_page);
    clutter_actor_show (new_page);
    clutter_actor_set_position (new_page, width, 0);

    priv->animation->new_page = new_page;

    priv->animation->old_page_anim = clutter_actor_animate
        (CLUTTER_ACTOR (priv->current_page),
         CLUTTER_EASE_OUT_EXPO, PAGE_CHANGE_DURATION,
         "x", 0 - (int) width,
         NULL);
    priv->animation->old_anim_id = g_signal_connect
        (priv->animation->old_page_anim, "completed",
         G_CALLBACK (page_change_complete), pane);

    priv->animation->new_page_anim = clutter_actor_animate
        (new_page, CLUTTER_EASE_OUT_EXPO,
         PAGE_CHANGE_DURATION,
         "x", 0,
         NULL);

    priv->current_page_num++;

    return FALSE;
}

static void
ahoghill_results_pane_init (AhoghillResultsPane *self)
{
    AhoghillResultsPanePrivate *priv = GET_PRIVATE (self);

    self->priv = priv;

    clutter_actor_set_name (CLUTTER_ACTOR (self), "media-pane-results");
    nbtk_table_set_row_spacing (NBTK_TABLE (self), ROW_SPACING);

    priv->title = nbtk_label_new ("");
    clutter_actor_set_name (CLUTTER_ACTOR (priv->title),
                            "media-pane-results-label");
    nbtk_table_add_widget_full (NBTK_TABLE (self), priv->title,
                                0, 0, 1, 1, 0, 0.0, 0.0);

    priv->fixed = g_object_new (NBTK_TYPE_FIXED, NULL);
    nbtk_table_add_widget_full (NBTK_TABLE (self), priv->fixed,
                                1, 0, 1, 2,
                                NBTK_X_EXPAND | NBTK_Y_EXPAND |
                                NBTK_X_FILL | NBTK_Y_FILL, 0.0, 0.0);

    priv->current_page = g_object_new (AHOGHILL_TYPE_RESULTS_TABLE, NULL);
    g_signal_connect (priv->current_page, "item-clicked",
                      G_CALLBACK (item_clicked_cb), self);
    nbtk_fixed_add_actor (NBTK_FIXED (priv->fixed),
                          (ClutterActor *) priv->current_page);
    clutter_actor_show ((ClutterActor *) priv->current_page);
    clutter_actor_set_position ((ClutterActor *) priv->current_page, 0, 0);

    priv->previous_button = nbtk_button_new_with_label ("Previous");
    clutter_actor_set_name (CLUTTER_ACTOR (priv->previous_button),
                            "media-pane-previous-page");
    g_signal_connect (CLUTTER_ACTOR (priv->previous_button),
                      "button-release-event",
                      G_CALLBACK (show_previous_page), self);
    nbtk_table_add_widget_full (NBTK_TABLE (self), priv->previous_button,
                                2, 0, 1, 1, 0, 0.0, 0.5);

    priv->next_button = nbtk_button_new_with_label ("Next");
    clutter_actor_set_name (CLUTTER_ACTOR (priv->next_button),
                            "media-pane-next-page");
    g_signal_connect (CLUTTER_ACTOR (priv->next_button), "button-release-event",
                      G_CALLBACK (show_next_page), self);
    nbtk_table_add_widget_full (NBTK_TABLE (self), priv->next_button,
                                2, 1, 1, 1, 0, 1.0, 0.5);
}

static void
unref_and_free_array (GPtrArray *array)
{
    int i;

    for (i = 0; i < array->len; i++) {
        g_object_unref (array->pdata[i]);
    }

    g_ptr_array_free (array, TRUE);
}

static GPtrArray *
copy_and_ref_array (GPtrArray *original)
{
    GPtrArray *clone;
    int i;

    clone = g_ptr_array_sized_new (original->len);
    for (i = 0; i < original->len; i++) {
        g_ptr_array_add (clone, g_object_ref (original->pdata[i]));
    }

    return clone;
}

void
ahoghill_results_pane_set_results (AhoghillResultsPane *self,
                                   GPtrArray           *results)
{
    AhoghillResultsPanePrivate *priv;

    priv = self->priv;

    if (priv->results) {
        unref_and_free_array (priv->results);
        priv->results = NULL;
    }

    priv->results = copy_and_ref_array (results);

    ahoghill_results_table_update (priv->current_page, priv->results, 0);
    priv->current_page_num = 0;

    priv->last_page = (results->len / TILES_PER_PAGE);
}
