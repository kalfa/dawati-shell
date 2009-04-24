/* mnb-toolbar.h */

#ifndef _MNB_TOOLBAR
#define _MNB_TOOLBAR

#include <glib-object.h>
#include <nbtk/nbtk.h>

G_BEGIN_DECLS

#define MNB_TYPE_TOOLBAR mnb_toolbar_get_type()

#define MNB_TOOLBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MNB_TYPE_TOOLBAR, MnbToolbar))

#define MNB_TOOLBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MNB_TYPE_TOOLBAR, MnbToolbarClass))

#define MNB_IS_TOOLBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MNB_TYPE_TOOLBAR))

#define MNB_IS_TOOLBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MNB_TYPE_TOOLBAR))

#define MNB_TOOLBAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MNB_TYPE_TOOLBAR, MnbToolbarClass))

typedef struct _MnbToolbarPrivate MnbToolbarPrivate;

typedef struct {
  NbtkTable parent;

  MnbToolbarPrivate *priv;
} MnbToolbar;

typedef struct {
  NbtkTableClass parent_class;
} MnbToolbarClass;

GType mnb_toolbar_get_type (void);

NbtkWidget* mnb_toolbar_new (MutterPlugin *plugin);

G_END_DECLS

#endif /* _MNB_TOOLBAR */

