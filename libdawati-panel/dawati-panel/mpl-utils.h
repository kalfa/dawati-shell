/*
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
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
#ifndef _MPL_UTILS_H
#define _MPL_UTILS_H

#include <glib.h>
#include <gtk/gtk.h>

gchar *mpl_utils_get_thumbnail_path (const gchar *uri);

GtkListStore *mpl_create_audio_store (void);

void mpl_audio_store_set (GtkListStore *store,
                          GtkTreeIter *iter,
                          const gchar *id,
                          const gchar *thumbnail,
                          const gchar *track_name,
                          const gchar *artist_name,
                          const gchar *album_name);

gboolean mpl_utils_panel_in_standalone_mode (void);

#endif /* _MPL_UTILS_H */
