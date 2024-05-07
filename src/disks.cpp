#include <config.h>

#include <giomm.h>
#include <giomm/themedicon.h>
#include <ctk/ctk.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>
#include <glib/gi18n.h>

#include "procman.h"
#include "disks.h"
#include "util.h"
#include "interface.h"
#include "iconthemewrapper.h"

namespace
{
  const unsigned DISK_ICON_SIZE = 24;
}

enum DiskColumns
{
    /* string columns* */
    DISK_DEVICE,
    DISK_DIR,
    DISK_TYPE,
    DISK_TOTAL,
    DISK_FREE,
    DISK_AVAIL,
    /* USED has to be the last column */
    DISK_USED,
    // then unvisible columns
    /* Surface column */
    DISK_ICON,
    /* numeric columns */
    DISK_USED_PERCENTAGE,
    DISK_N_COLUMNS
};



static void
fsusage_stats(const glibtop_fsusage *buf,
              guint64 *bused, guint64 *bfree, guint64 *bavail, guint64 *btotal,
              gint *percentage)
{
    guint64 total = buf->blocks * buf->block_size;

    if (!total) {
        /* not a real device */
        *btotal = *bfree = *bavail = *bused = 0ULL;
        *percentage = 0;
    } else {
        int percent;
        *btotal = total;
        *bfree = buf->bfree * buf->block_size;
        *bavail = buf->bavail * buf->block_size;
        *bused = *btotal - *bfree;
        /* percent = 100.0f * *bused / *btotal; */
        percent = 100 * *bused / (*bused + *bavail);
        *percentage = CLAMP(percent, 0, 100);
    }
}


namespace
{
    string get_icon_for_path(const std::string& path)
    {
        using namespace Glib;
        using namespace Gio;

        // FIXME: I don't know whether i should use Volume or Mount or UnixMount
        // all i need an icon name.
        RefPtr<VolumeMonitor> monitor = VolumeMonitor::get();

        std::vector<RefPtr<Mount> > mounts = monitor->get_mounts();

        for (size_t i = 0; i != mounts.size(); ++i) {
            if (mounts[i]->get_name() != path)
                continue;

            RefPtr<Icon> icon = mounts[i]->get_icon();
            RefPtr<ThemedIcon> themed_icon = RefPtr<ThemedIcon>::cast_dynamic(icon);

            if (themed_icon) {
                char* name = 0;
                // FIXME: not wrapped yet
                g_object_get(G_OBJECT(themed_icon->gobj()), "name", &name, NULL);
                return make_string(name);
            }
        }

        return "";
    }
}


static Glib::RefPtr<Gdk::Pixbuf>
get_icon_for_device(const char *mountpoint)
{
    procman::IconThemeWrapper icon_theme;
    string icon_name = get_icon_for_path(mountpoint);
    if (icon_name == "")
        // FIXME: defaults to a safe value
        icon_name = "drive-harddisk"; // get_icon_for_path("/");
    return icon_theme->load_icon(icon_name, DISK_ICON_SIZE);
}


static gboolean
find_disk_in_model(CtkTreeModel *model, const char *mountpoint,
                   CtkTreeIter *result)
{
    CtkTreeIter iter;
    gboolean found = FALSE;

    if (ctk_tree_model_get_iter_first(model, &iter)) {
        do {
            char *dir;

            ctk_tree_model_get(model, &iter,
                               DISK_DIR, &dir,
                               -1);

            if (dir && !strcmp(dir, mountpoint)) {
                *result = iter;
                found = TRUE;
            }

            g_free(dir);

        } while (!found && ctk_tree_model_iter_next(model, &iter));
    }

    return found;
}



static void
remove_old_disks(CtkTreeModel *model, const glibtop_mountentry *entries, guint n)
{
    CtkTreeIter iter;

    if (!ctk_tree_model_get_iter_first(model, &iter))
        return;

    while (true) {
        char *dir;
        guint i;
        gboolean found = FALSE;

        ctk_tree_model_get(model, &iter,
                           DISK_DIR, &dir,
                           -1);

        for (i = 0; i != n; ++i) {
            if (!strcmp(dir, entries[i].mountdir)) {
                found = TRUE;
                break;
            }
        }

        g_free(dir);

        if (!found) {
            if (!ctk_list_store_remove(CTK_LIST_STORE(model), &iter))
                break;
            else
                continue;
        }

        if (!ctk_tree_model_iter_next(model, &iter))
            break;
    }
}



static void
add_disk(CtkListStore *list, const glibtop_mountentry *entry, bool show_all_fs)
{
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    cairo_surface_t *surface;
    CtkTreeIter iter;
    glibtop_fsusage usage;
    guint64 bused, bfree, bavail, btotal;
    gint percentage;

    glibtop_get_fsusage(&usage, entry->mountdir);

    if (not show_all_fs and usage.blocks == 0) {
        if (find_disk_in_model(CTK_TREE_MODEL(list), entry->mountdir, &iter))
            ctk_list_store_remove(list, &iter);
        return;
    }

    fsusage_stats(&usage, &bused, &bfree, &bavail, &btotal, &percentage);
    pixbuf = get_icon_for_device(entry->mountdir);
    surface = gdk_cairo_surface_create_from_pixbuf (pixbuf->gobj(), 0, NULL);

    /* if we can find a row with the same mountpoint, we get it but we
       still need to update all the fields.
       This makes selection persistent.
    */
    if (!find_disk_in_model(CTK_TREE_MODEL(list), entry->mountdir, &iter))
        ctk_list_store_append(list, &iter);

    ctk_list_store_set(list, &iter,
                       DISK_ICON, surface,
                       DISK_DEVICE, entry->devname,
                       DISK_DIR, entry->mountdir,
                       DISK_TYPE, entry->type,
                       DISK_USED_PERCENTAGE, percentage,
                       DISK_TOTAL, btotal,
                       DISK_FREE, bfree,
                       DISK_AVAIL, bavail,
                       DISK_USED, bused,
                       -1);
}



int
cb_update_disks(gpointer data)
{
    ProcData *const procdata = static_cast<ProcData*>(data);

    CtkListStore *list;
    glibtop_mountentry * entries;
    glibtop_mountlist mountlist;
    guint i;

    list = CTK_LIST_STORE(ctk_tree_view_get_model(CTK_TREE_VIEW(procdata->disk_list)));

    entries = glibtop_get_mountlist(&mountlist, procdata->config.show_all_fs);

    remove_old_disks(CTK_TREE_MODEL(list), entries, mountlist.number);

    for (i = 0; i < mountlist.number; i++)
        add_disk(list, &entries[i], procdata->config.show_all_fs);

    g_free(entries);

    return TRUE;
}


static void
cb_disk_columns_changed(CtkTreeView *treeview, gpointer user_data)
{
    ProcData * const procdata = static_cast<ProcData*>(user_data);

    procman_save_tree_state(procdata->settings,
                            CTK_WIDGET(treeview),
                            "disktreenew");
}


static void
open_dir(CtkTreeView       *tree_view,
         CtkTreePath       *path,
         CtkTreeViewColumn *column,
         gpointer           user_data)
{
    CtkTreeIter iter;
    CtkTreeModel *model;
    char *dir, *url;

    model = ctk_tree_view_get_model(tree_view);

    if (!ctk_tree_model_get_iter(model, &iter, path)) {
        char *p;
        p = ctk_tree_path_to_string(path);
        g_warning("Cannot get iter for path '%s'\n", p);
        g_free(p);
        return;
    }

    ctk_tree_model_get(model, &iter, DISK_DIR, &dir, -1);

    url = g_strdup_printf("file://%s", dir);

    GError* error = 0;
    if (!g_app_info_launch_default_for_uri(url, NULL, &error)) {
        g_warning("Cannot open '%s' : %s\n", url, error->message);
        g_error_free(error);
    }

    g_free(url);
    g_free(dir);
}

static guint timeout_id = 0;
static CtkTreeViewColumn *current_column;

static gboolean
save_column_width (gpointer data)
{
    gint width;
    gchar *key;
    int id;
    GSettings *settings;

    settings = g_settings_get_child (G_SETTINGS (data), "disktreenew");
    id = ctk_tree_view_column_get_sort_column_id (current_column);
    width = ctk_tree_view_column_get_width (current_column);

    key = g_strdup_printf ("col-%d-width", id);
    g_settings_set_int(settings, key, width);
    g_free (key);

    if (timeout_id) {
        g_source_remove (timeout_id);
        timeout_id = 0;
    }

    return FALSE;
}

static void
cb_disks_column_resized(CtkWidget *widget, GParamSpec *pspec, gpointer data)
{
    current_column = CTK_TREE_VIEW_COLUMN(widget);

    if (timeout_id)
        g_source_remove (timeout_id);

    timeout_id = g_timeout_add (250, save_column_width, data);
}

CtkWidget *
create_disk_view(ProcData *procdata)
{
    CtkWidget *disk_box;
    CtkWidget *scrolled;
    CtkWidget *disk_tree;
    CtkListStore *model;
    CtkTreeViewColumn *col;
    CtkCellRenderer *cell;
    guint i;

    const gchar * const titles[] = {
        N_("Device"),
        N_("Directory"),
        N_("Type"),
        N_("Total"),
        N_("Free"),
        N_("Available"),
        N_("Used")
    };

    disk_box = ctk_box_new(CTK_ORIENTATION_VERTICAL, 6);

    ctk_container_set_border_width(CTK_CONTAINER(disk_box), 12);

    scrolled = ctk_scrolled_window_new(NULL, NULL);
    ctk_scrolled_window_set_policy(CTK_SCROLLED_WINDOW(scrolled),
                                   CTK_POLICY_AUTOMATIC,
                                   CTK_POLICY_AUTOMATIC);
    ctk_scrolled_window_set_shadow_type(CTK_SCROLLED_WINDOW(scrolled),
                                        CTK_SHADOW_IN);

    ctk_box_pack_start(CTK_BOX(disk_box), scrolled, TRUE, TRUE, 0);

    model = ctk_list_store_new(DISK_N_COLUMNS,             /* n columns */
                               G_TYPE_STRING,              /* DISK_DEVICE */
                               G_TYPE_STRING,              /* DISK_DIR */
                               G_TYPE_STRING,              /* DISK_TYPE */
                               G_TYPE_UINT64,              /* DISK_TOTAL */
                               G_TYPE_UINT64,              /* DISK_FREE */
                               G_TYPE_UINT64,              /* DISK_AVAIL */
                               G_TYPE_UINT64,              /* DISK_USED */
                               CAIRO_GOBJECT_TYPE_SURFACE, /* DISK_ICON */
                               G_TYPE_INT                  /* DISK_USED_PERCENTAGE */
        );

    disk_tree = ctk_tree_view_new_with_model(CTK_TREE_MODEL(model));
    g_signal_connect(G_OBJECT(disk_tree), "row-activated", G_CALLBACK(open_dir), NULL);
    procdata->disk_list = disk_tree;
    ctk_container_add(CTK_CONTAINER(scrolled), disk_tree);
    g_object_unref(G_OBJECT(model));

    /* icon + device */

    col = ctk_tree_view_column_new();
    cell = ctk_cell_renderer_pixbuf_new();
    ctk_tree_view_column_pack_start(col, cell, FALSE);
    ctk_tree_view_column_set_attributes(col, cell, "surface", DISK_ICON,
                                        NULL);

    cell = ctk_cell_renderer_text_new();
    ctk_tree_view_column_pack_start(col, cell, FALSE);
    ctk_tree_view_column_set_attributes(col, cell, "text", DISK_DEVICE,
                                        NULL);
    ctk_tree_view_column_set_title(col, _(titles[DISK_DEVICE]));
    ctk_tree_view_column_set_sort_column_id(col, DISK_DEVICE);
    ctk_tree_view_column_set_reorderable(col, TRUE);
    ctk_tree_view_column_set_resizable(col, TRUE);
    ctk_tree_view_column_set_min_width (col, 30);
    ctk_tree_view_column_set_sizing(col, CTK_TREE_VIEW_COLUMN_FIXED);
    g_signal_connect(G_OBJECT(col), "notify::fixed-width", G_CALLBACK(cb_disks_column_resized), procdata->settings);
    ctk_tree_view_append_column(CTK_TREE_VIEW(disk_tree), col);

    /* sizes - used */

    for (i = DISK_DIR; i <= DISK_AVAIL; i++) {
        cell = ctk_cell_renderer_text_new();
        col = ctk_tree_view_column_new();
        ctk_tree_view_column_pack_start(col, cell, TRUE);
        ctk_tree_view_column_set_title(col, _(titles[i]));
        ctk_tree_view_column_set_sort_column_id(col, i);
        ctk_tree_view_column_set_reorderable(col, TRUE);
        ctk_tree_view_column_set_resizable(col, TRUE);
        ctk_tree_view_column_set_min_width (col, 30);
        ctk_tree_view_column_set_sizing(col, CTK_TREE_VIEW_COLUMN_FIXED);
        g_signal_connect(G_OBJECT(col), "notify::fixed-width", G_CALLBACK(cb_disks_column_resized), procdata->settings);
        ctk_tree_view_append_column(CTK_TREE_VIEW(disk_tree), col);

        switch (i) {
            case DISK_TOTAL:
            case DISK_FREE:
            case DISK_AVAIL:
                g_object_set(cell, "xalign", 1.0f, NULL);
                ctk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::storage_size_cell_data_func,
                                                        GUINT_TO_POINTER(i),
                                                        NULL);
                break;

        default:
            ctk_tree_view_column_set_attributes(col, cell,
                                                "text", i,
                                                NULL);
            break;
        }
    }

    /* used + percentage */

    col = ctk_tree_view_column_new();
    cell = ctk_cell_renderer_text_new();
    g_object_set(cell, "xalign", 1.0f, NULL);
    ctk_tree_view_column_pack_start(col, cell, FALSE);
    ctk_tree_view_column_set_cell_data_func(col, cell,
                                            &procman::storage_size_cell_data_func,
                                            GUINT_TO_POINTER(DISK_USED),
                                            NULL);

    cell = ctk_cell_renderer_progress_new();
    ctk_tree_view_column_pack_start(col, cell, TRUE);
    ctk_tree_view_column_set_attributes(col, cell, "value",
                                        DISK_USED_PERCENTAGE, NULL);
    ctk_tree_view_column_set_title(col, _(titles[DISK_USED]));
    ctk_tree_view_column_set_sort_column_id(col, DISK_USED);
    ctk_tree_view_column_set_reorderable(col, TRUE);
    ctk_tree_view_column_set_resizable(col, TRUE);
    ctk_tree_view_column_set_min_width (col, 150);
    ctk_tree_view_column_set_sizing(col, CTK_TREE_VIEW_COLUMN_FIXED);
    g_signal_connect(G_OBJECT(col), "notify::fixed-width", G_CALLBACK(cb_disks_column_resized), procdata->settings);
    ctk_tree_view_append_column(CTK_TREE_VIEW(disk_tree), col);

    /* numeric sort */

    ctk_widget_show_all(disk_box);

    procman_get_tree_state(procdata->settings, disk_tree,
                   "disktreenew");

    g_signal_connect (G_OBJECT(disk_tree), "columns-changed",
                      G_CALLBACK(cb_disk_columns_changed), procdata);

    return disk_box;
}
