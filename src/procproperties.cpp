/* Process properties dialog
 * Copyright (C) 2010 Krishnan Parthasarathi <krishnan.parthasarathi@gmail.com>
 *                    Robert Ancell <robert.ancell@canonical.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include <glibtop/procmem.h>
#include <glibtop/procmap.h>
#include <glibtop/procstate.h>

#include "procman.h"
#include "procproperties.h"
#include "proctable.h"
#include "util.h"

enum
{
    COL_PROP = 0,
    COL_VAL,
    NUM_COLS,
};

typedef struct _proc_arg {
    const gchar *prop;
    gchar *val;
} proc_arg;

static void
get_process_memory_writable (ProcInfo *info)
{
    glibtop_proc_map buf;
    glibtop_map_entry *maps;

    maps = glibtop_get_proc_map(&buf, info->pid);

    gulong memwritable = 0;
    const unsigned number = buf.number;

    for (unsigned i = 0; i < number; ++i) {
#ifdef __linux__
        memwritable += maps[i].private_dirty;
#else
        if (maps[i].perm & GLIBTOP_MAP_PERM_WRITE)
            memwritable += maps[i].size;
#endif
    }

    info->memwritable = memwritable;

    g_free(maps);
}

static void
get_process_memory_info (ProcInfo *info)
{
    glibtop_proc_mem procmem;
    WnckResourceUsage xresources;

    wnck_pid_read_resource_usage (cdk_screen_get_display (cdk_screen_get_default ()),
                                  info->pid,
                                  &xresources);

    glibtop_get_proc_mem(&procmem, info->pid);

    info->vmsize	= procmem.vsize;
    info->memres	= procmem.resident;
    info->memshared	= procmem.share;

    info->memxserver = xresources.total_bytes_estimate;

    get_process_memory_writable(info);

    // fake the smart memory column if writable is not available
    info->mem = info->memxserver + (info->memwritable ? info->memwritable : info->memres);
}

static gchar*
format_memsize(guint64 size)
{
    if (size == 0)
        return g_strdup(_("N/A"));
    else
        return g_format_size_full(size, G_FORMAT_SIZE_IEC_UNITS);
}

static gchar*
format_size(guint64 size)
{
    if (size == 0)
        return g_strdup(_("N/A"));
    else
        return g_format_size(size);
}

static void
fill_proc_properties (CtkWidget *tree, ProcInfo *info)
{
    guint i;
    CtkListStore *store;

    if (!info)
        return;

    get_process_memory_info(info);

    proc_arg proc_props[] = {
        { N_("Process Name"), g_strdup_printf("%s", info->name)},
        { N_("User"), g_strdup_printf("%s (%d)", info->user.c_str(), info->uid)},
        { N_("Status"), g_strdup(format_process_state(info->status))},
        { N_("Memory"), format_memsize(info->mem)},
        { N_("Virtual Memory"), format_memsize(info->vmsize)},
        { N_("Resident Memory"), format_memsize(info->memres)},
        { N_("Writable Memory"), format_memsize(info->memwritable)},
        { N_("Shared Memory"), format_memsize(info->memshared)},
        { N_("X Server Memory"), format_memsize(info->memxserver)},
        { N_("Disk Read Total"), format_size(info->disk_read_bytes_total)},
        { N_("Disk Write Total"), format_size(info->disk_write_bytes_total)},
        { N_("CPU"), g_strdup_printf("%d%%", info->pcpu)},
        { N_("CPU Time"), procman::format_duration_for_display(100 * info->cpu_time / ProcData::get_instance()->frequency) },
        { N_("Started"), procman_format_date_for_display(info->start_time) },
        { N_("Nice"), g_strdup_printf("%d", info->nice)},
        { N_("Priority"), g_strdup_printf("%s", procman::get_nice_level(info->nice)) },
        { N_("ID"), g_strdup_printf("%d", info->pid)},
        { N_("Security Context"), info->security_context ? g_strdup_printf("%s", info->security_context) : g_strdup(_("N/A"))},
        { N_("Command Line"), g_strdup_printf("%s", info->arguments)},
        { N_("Waiting Channel"), g_strdup_printf("%s", info->wchan)},
        { N_("Control Group"), info->cgroup_name ? g_strdup_printf("%s", info->cgroup_name) : g_strdup(_("N/A"))},
        { NULL, NULL}
    };

    store = CTK_LIST_STORE(ctk_tree_view_get_model(CTK_TREE_VIEW(tree)));
    for (i = 0; proc_props[i].prop; i++) {
        CtkTreeIter iter;

        if (!ctk_tree_model_iter_nth_child (CTK_TREE_MODEL(store), &iter, NULL, i)) {
            ctk_list_store_append(store, &iter);
            ctk_list_store_set(store, &iter, COL_PROP, gettext(proc_props[i].prop), -1);
        }

        ctk_list_store_set(store, &iter, COL_VAL, g_strstrip(proc_props[i].val), -1);
        g_free(proc_props[i].val);
    }
}

static void
update_procproperties_dialog (CtkWidget *tree)
{
    ProcInfo *info;

    pid_t pid = GPOINTER_TO_UINT(static_cast<pid_t*>(g_object_get_data (G_OBJECT (tree), "selected_info")));
    info = ProcInfo::find(pid);

    fill_proc_properties(tree, info);
}

static void
close_procprop_dialog (CtkDialog *dialog, gint id, gpointer data)
{
    CtkWidget *tree = static_cast<CtkWidget*>(data);
    guint timer;

    timer = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (tree), "timer"));
    g_source_remove (timer);

    ctk_widget_destroy (CTK_WIDGET (dialog));
}

static CtkWidget *
create_procproperties_tree (ProcData *procdata)
{
    CtkWidget *tree;
    CtkListStore *model;
    CtkTreeViewColumn *column;
    CtkCellRenderer *cell;
    gint i;

    model = ctk_list_store_new (NUM_COLS,
                                G_TYPE_STRING,	/* Property */
                                G_TYPE_STRING	/* Value */
        );

    tree = ctk_tree_view_new_with_model (CTK_TREE_MODEL (model));
    g_object_unref (G_OBJECT (model));

    for (i = 0; i < NUM_COLS; i++) {
        cell = ctk_cell_renderer_text_new ();

        column = ctk_tree_view_column_new_with_attributes (NULL,
                                                           cell,
                                                           "text", i,
                                                           NULL);
        ctk_tree_view_column_set_resizable (column, TRUE);
        ctk_tree_view_append_column (CTK_TREE_VIEW (tree), column);
    }

    ctk_tree_view_set_headers_visible (CTK_TREE_VIEW(tree), FALSE);
    fill_proc_properties(tree, procdata->selected_process);

    return tree;
}

static gboolean
procprop_timer (gpointer data)
{
    CtkWidget *tree = static_cast<CtkWidget*>(data);
    CtkTreeModel *model;

    model = ctk_tree_view_get_model (CTK_TREE_VIEW (tree));
    g_assert(model);

    update_procproperties_dialog (tree);

    return TRUE;
}

static void
create_single_procproperties_dialog (CtkTreeModel *model, CtkTreePath *path,
                                     CtkTreeIter *iter, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    CtkWidget *procpropdialog;
    CtkWidget *dialog_vbox, *vbox;
    CtkWidget *cmd_hbox;
    CtkWidget *label;
    CtkWidget *scrolled;
    CtkWidget *tree;
    ProcInfo *info;
    guint timer;

    ctk_tree_model_get (model, iter, COL_POINTER, &info, -1);

    if (!info)
        return;

    procpropdialog = ctk_dialog_new_with_buttons (_("Process Properties"), NULL,
                                                  CTK_DIALOG_DESTROY_WITH_PARENT,
                                                  "ctk-close", CTK_RESPONSE_CLOSE,
                                                  NULL);
    ctk_window_set_resizable (CTK_WINDOW (procpropdialog), TRUE);
    ctk_window_set_default_size (CTK_WINDOW (procpropdialog), 575, 400);
    ctk_container_set_border_width (CTK_CONTAINER (procpropdialog), 5);

    vbox = ctk_dialog_get_content_area (CTK_DIALOG (procpropdialog));
    ctk_box_set_spacing (CTK_BOX (vbox), 2);
    ctk_container_set_border_width (CTK_CONTAINER (vbox), 5);

    dialog_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_container_set_border_width (CTK_CONTAINER (dialog_vbox), 5);
    ctk_box_pack_start (CTK_BOX (vbox), dialog_vbox, TRUE, TRUE, 0);

    cmd_hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start (CTK_BOX (dialog_vbox), cmd_hbox, FALSE, FALSE, 0);

    label = procman_make_label_for_mmaps_or_ofiles (
        _("Properties of process \"%s\" (PID %u):"),
        info->name,
        info->pid);

    ctk_box_pack_start (CTK_BOX (cmd_hbox),label, FALSE, FALSE, 0);

    scrolled = ctk_scrolled_window_new (NULL, NULL);
    ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (scrolled),
                                    CTK_POLICY_AUTOMATIC,
                                    CTK_POLICY_AUTOMATIC);
    ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (scrolled),
                                         CTK_SHADOW_IN);

    tree = create_procproperties_tree (procdata);
    ctk_container_add (CTK_CONTAINER (scrolled), tree);
    g_object_set_data (G_OBJECT (tree), "selected_info", GUINT_TO_POINTER (info->pid));

    ctk_box_pack_start (CTK_BOX (dialog_vbox), scrolled, TRUE, TRUE, 0);
    ctk_widget_show_all (scrolled);

    g_signal_connect (G_OBJECT (procpropdialog), "response",
                      G_CALLBACK (close_procprop_dialog), tree);

    ctk_widget_show_all (procpropdialog);

    timer = g_timeout_add_seconds (5, procprop_timer, tree);
    g_object_set_data (G_OBJECT (tree), "timer", GUINT_TO_POINTER (timer));

    update_procproperties_dialog (tree);
}

void
create_procproperties_dialog (ProcData *procdata)
{
    ctk_tree_selection_selected_foreach (procdata->selection, create_single_procproperties_dialog,
                                         procdata);
}
