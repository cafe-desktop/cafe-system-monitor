/* Procman - dialogs
 * Copyright (C) 2001 Kevin Vandersloot
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */


#include <config.h>

#include <glib/gi18n.h>

#include <signal.h>
#include <string.h>

#include "procdialogs.h"
#include "proctable.h"
#include "callbacks.h"
#include "prettytable.h"
#include "procactions.h"
#include "util.h"
#include "load-graph.h"
#include "settings-keys.h"
#include "procman_gksu.h"
#include "procman_pkexec.h"
#include "cgroups.h"


static CtkWidget *renice_dialog = NULL;
static CtkWidget *prefs_dialog = NULL;
static gint new_nice_value = 0;


static void
kill_dialog_button_pressed (CtkDialog *dialog, gint id, gpointer data)
{
    struct KillArgs *kargs = static_cast<KillArgs*>(data);

    ctk_widget_destroy (CTK_WIDGET (dialog));

    if (id == CTK_RESPONSE_OK)
        kill_process (kargs->procdata, kargs->signal);

    g_free (kargs);
}

void
procdialog_create_kill_dialog (ProcData *procdata, int signal)
{
    CtkWidget *kill_alert_dialog;
    gchar *primary, *secondary, *button_text;
    struct KillArgs *kargs;

    kargs = g_new(KillArgs, 1);
    kargs->procdata = procdata;
    kargs->signal = signal;


    if (signal == SIGKILL) {
        /*xgettext: primary alert message*/
        primary = g_strdup_printf (_("Kill the selected process “%s” (PID: %u)?"),
                                   procdata->selected_process->name,
                                   procdata->selected_process->pid);
        /*xgettext: secondary alert message*/
        secondary = _("Killing a process may destroy data, break the "
                    "session or introduce a security risk. "
                    "Only unresponsive processes should be killed.");
        button_text = _("_Kill Process");
    }
    else {
        /*xgettext: primary alert message*/
        primary = g_strdup_printf (_("End the selected process “%s” (PID: %u)?"),
                                   procdata->selected_process->name,
                                   procdata->selected_process->pid);
        /*xgettext: secondary alert message*/
        secondary = _("Ending a process may destroy data, break the "
                    "session or introduce a security risk. "
                    "Only unresponsive processes should be ended.");
        button_text = _("_End Process");
    }

    kill_alert_dialog = ctk_message_dialog_new (CTK_WINDOW (procdata->app),
                                                static_cast<CtkDialogFlags>(CTK_DIALOG_MODAL | CTK_DIALOG_DESTROY_WITH_PARENT),
                                                CTK_MESSAGE_WARNING,
                                                CTK_BUTTONS_NONE,
                                                "%s",
                                                primary);
    g_free (primary);

    ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (kill_alert_dialog),
                                              "%s",
                                              secondary);

    ctk_dialog_add_buttons (CTK_DIALOG (kill_alert_dialog),
                            "ctk-cancel", CTK_RESPONSE_CANCEL,
                            button_text, CTK_RESPONSE_OK,
                            NULL);

    ctk_dialog_set_default_response (CTK_DIALOG (kill_alert_dialog),
                                     CTK_RESPONSE_CANCEL);

    g_signal_connect (G_OBJECT (kill_alert_dialog), "response",
                      G_CALLBACK (kill_dialog_button_pressed), kargs);

    ctk_widget_show_all (kill_alert_dialog);
}

static void
renice_scale_changed (CtkAdjustment *adj, gpointer data)
{
    CtkWidget *label = CTK_WIDGET (data);

    new_nice_value = int(ctk_adjustment_get_value (adj));
    gchar* text = g_strdup_printf(_("(%s Priority)"), procman::get_nice_level (new_nice_value));
    ctk_label_set_text (CTK_LABEL (label), text);
    g_free(text);

}

static void
renice_dialog_button_pressed (CtkDialog *dialog, gint id, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);

    if (id == 100) {
        if (new_nice_value == -100)
            return;
        renice(procdata, new_nice_value);
    }

    ctk_widget_destroy (CTK_WIDGET (dialog));
    renice_dialog = NULL;
}

void
procdialog_create_renice_dialog (ProcData *procdata)
{
    ProcInfo  *info = procdata->selected_process;
    CtkWidget *dialog = NULL;
    CtkWidget *dialog_vbox;
    CtkWidget *vbox;
    CtkWidget *label;
    CtkWidget *priority_label;
    CtkWidget *grid;
    CtkAdjustment *renice_adj;
    CtkWidget *hscale;
    CtkWidget *button;
    CtkWidget *icon;
    gchar     *text;
    gchar     *dialog_title;

    if (renice_dialog)
        return;

    if (!info)
        return;

    dialog_title = g_strdup_printf (_("Change Priority of Process “%s” (PID: %u)"),
                                    info->name, info->pid);
    dialog = ctk_dialog_new_with_buttons (dialog_title, NULL,
                                          CTK_DIALOG_DESTROY_WITH_PARENT,
                                          "ctk-cancel", CTK_RESPONSE_CANCEL,
                                          NULL);
    g_free (dialog_title);

    renice_dialog = dialog;
    ctk_window_set_resizable (CTK_WINDOW (renice_dialog), FALSE);
    ctk_container_set_border_width (CTK_CONTAINER (renice_dialog), 5);

    button = ctk_button_new_with_mnemonic (_("Change _Priority"));
    ctk_widget_set_can_default (button, TRUE);

    icon = ctk_image_new_from_stock ("ctk-ok", CTK_ICON_SIZE_BUTTON);
    ctk_button_set_image (CTK_BUTTON (button), icon);

    ctk_dialog_add_action_widget (CTK_DIALOG (renice_dialog), button, 100);
    ctk_dialog_set_default_response (CTK_DIALOG (renice_dialog), 100);
    new_nice_value = -100;

    dialog_vbox = ctk_dialog_get_content_area (CTK_DIALOG (dialog));
    ctk_box_set_spacing (CTK_BOX (dialog_vbox), 2);
    ctk_container_set_border_width (CTK_CONTAINER (dialog_vbox), 5);

    vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 12);
    ctk_box_pack_start (CTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);
    ctk_container_set_border_width (CTK_CONTAINER (vbox), 12);

    grid = ctk_grid_new ();
    ctk_grid_set_column_spacing (CTK_GRID(grid), 12);
    ctk_grid_set_row_spacing (CTK_GRID(grid), 6);
    ctk_box_pack_start (CTK_BOX (vbox), grid, TRUE, TRUE, 0);

    label = ctk_label_new_with_mnemonic (_("_Nice value:"));
    ctk_grid_attach (CTK_GRID (grid), label, 0, 0, 1, 2);

    renice_adj = ctk_adjustment_new (info->nice, RENICE_VAL_MIN, RENICE_VAL_MAX, 1, 1, 0);
    new_nice_value = 0;
    hscale = ctk_scale_new (CTK_ORIENTATION_HORIZONTAL, renice_adj);
    ctk_label_set_mnemonic_widget (CTK_LABEL (label), hscale);
    ctk_scale_set_digits (CTK_SCALE (hscale), 0);
    ctk_widget_set_hexpand (hscale, TRUE);
    ctk_grid_attach (CTK_GRID (grid), hscale, 1, 0, 1, 1);
    text = g_strdup_printf(_("(%s Priority)"), procman::get_nice_level (info->nice));
    priority_label = ctk_label_new (text);
    ctk_grid_attach (CTK_GRID (grid), priority_label, 1, 1, 1, 1);
    g_free(text);

    text = g_strconcat("<small><i><b>", _("Note:"), "</b> ",
        _("The priority of a process is given by its nice value. A lower nice value corresponds to a higher priority."),
        "</i></small>", NULL);
    label = ctk_label_new (_(text));
    ctk_label_set_line_wrap (CTK_LABEL (label), TRUE);
    ctk_label_set_use_markup (CTK_LABEL (label), TRUE);
    ctk_box_pack_start (CTK_BOX (vbox), label, FALSE, FALSE, 0);
    g_free (text);

    g_signal_connect (G_OBJECT (dialog), "response",
                      G_CALLBACK (renice_dialog_button_pressed), procdata);
    g_signal_connect (G_OBJECT (renice_adj), "value_changed",
                      G_CALLBACK (renice_scale_changed), priority_label);

    ctk_widget_show_all (dialog);


}

static void
prefs_dialog_button_pressed (CtkDialog *dialog, gint id, gpointer data)
{
    if (id == CTK_RESPONSE_HELP)
    {
        GError* error = 0;
        if (!g_app_info_launch_default_for_uri("help:cafe-system-monitor/cafe-system-monitor-prefs", NULL, &error))
        {
            g_warning("Could not display preferences help : %s", error->message);
            g_error_free(error);
        }
    }
    else
    {
        ctk_widget_destroy (CTK_WIDGET (dialog));
        prefs_dialog = NULL;
    }
}


static void
show_kill_dialog_toggled (CtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;

    gboolean toggled;

    toggled = ctk_toggle_button_get_active (button);

    g_settings_set_boolean (settings, "kill-dialog", toggled);

}



static void
solaris_mode_toggled(CtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;
    gboolean toggled;
    toggled = ctk_toggle_button_get_active(button);
    g_settings_set_boolean(settings, procman::settings::solaris_mode.c_str(), toggled);
}


static void
network_in_bits_toggled(CtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;
    gboolean toggled;
    toggled = ctk_toggle_button_get_active(button);
    g_settings_set_boolean(settings, procman::settings::network_in_bits.c_str(), toggled);
}



static void
smooth_refresh_toggled(CtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;

    gboolean toggled;

    toggled = ctk_toggle_button_get_active(button);

    g_settings_set_boolean(settings, SmoothRefresh::KEY.c_str(), toggled);
}



static void
show_all_fs_toggled (CtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;

    gboolean toggled;

    toggled = ctk_toggle_button_get_active (button);

    g_settings_set_boolean (settings, "show-all-fs", toggled);
}


class SpinButtonUpdater
{
public:
    SpinButtonUpdater(const string& key)
        : key(key)
    { }

    static gboolean callback(CtkWidget *widget, CdkEventFocus *event, gpointer data)
    {
        SpinButtonUpdater* updater = static_cast<SpinButtonUpdater*>(data);
        ctk_spin_button_update(CTK_SPIN_BUTTON(widget));
        updater->update(CTK_SPIN_BUTTON(widget));
        return FALSE;
    }

private:

    void update(CtkSpinButton* spin)
    {
        int new_value = int(1000 * ctk_spin_button_get_value(spin));
        g_settings_set_int(ProcData::get_instance()->settings,
                           this->key.c_str(), new_value);

        procman_debug("set %s to %d", this->key.c_str(), new_value);
    }

    const string key;
};


static void
field_toggled (const gchar *child_schema, CtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    CtkTreeModel *model = static_cast<CtkTreeModel*>(data);
    CtkTreePath *path = ctk_tree_path_new_from_string (path_str);
    CtkTreeIter iter;
    CtkTreeViewColumn *column;
    gboolean toggled;
    GSettings *settings = g_settings_get_child (ProcData::get_instance()->settings, child_schema);
    gchar *key;
    int id;

    if (!path)
        return;

    ctk_tree_model_get_iter (model, &iter, path);

    ctk_tree_model_get (model, &iter, 2, &column, -1);
    toggled = ctk_cell_renderer_toggle_get_active (cell);

    ctk_list_store_set (CTK_LIST_STORE (model), &iter, 0, !toggled, -1);
    ctk_tree_view_column_set_visible (column, !toggled);

    id = ctk_tree_view_column_get_sort_column_id (column);

    key = g_strdup_printf ("col-%d-visible", id);
    g_settings_set_boolean (settings, key, !toggled);
    g_free (key);

    ctk_tree_path_free (path);
}

static void
proc_field_toggled (CtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    field_toggled ("proctree", cell, path_str, data);
}

static void
disk_field_toggled (CtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    field_toggled ("disktreenew", cell, path_str, data);
}

static CtkWidget *
create_field_page(CtkWidget *tree, const gchar *child_schema, const gchar *text)
{
    CtkWidget *vbox;
    CtkWidget *scrolled;
    CtkWidget *label;
    CtkWidget *treeview;
    GList *it, *columns;
    CtkListStore *model;
    CtkTreeViewColumn *column;
    CtkCellRenderer *cell;

    vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);

    label = ctk_label_new_with_mnemonic (text);
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_box_pack_start (CTK_BOX (vbox), label, FALSE, TRUE, 0);

    scrolled = ctk_scrolled_window_new (NULL, NULL);
    ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (scrolled),
                                    CTK_POLICY_AUTOMATIC,
                                    CTK_POLICY_AUTOMATIC);
    ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (scrolled), CTK_SHADOW_IN);
    ctk_box_pack_start (CTK_BOX (vbox), scrolled, TRUE, TRUE, 0);

    model = ctk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);

    treeview = ctk_tree_view_new_with_model (CTK_TREE_MODEL (model));
    ctk_container_add (CTK_CONTAINER (scrolled), treeview);
    g_object_unref (G_OBJECT (model));
    ctk_label_set_mnemonic_widget (CTK_LABEL (label), treeview);

    column = ctk_tree_view_column_new ();

    cell = ctk_cell_renderer_toggle_new ();
    ctk_tree_view_column_pack_start (column, cell, FALSE);
    ctk_tree_view_column_set_attributes (column, cell,
                                         "active", 0,
                                         NULL);
    if (g_strcmp0 (child_schema, "proctree") == 0)
        g_signal_connect (G_OBJECT (cell), "toggled", G_CALLBACK (proc_field_toggled), model);
    else if (g_strcmp0 (child_schema, "disktreenew") == 0)
        g_signal_connect (G_OBJECT (cell), "toggled", G_CALLBACK (disk_field_toggled), model);

    ctk_tree_view_column_set_clickable (column, TRUE);
    ctk_tree_view_append_column (CTK_TREE_VIEW (treeview), column);

    column = ctk_tree_view_column_new ();

    cell = ctk_cell_renderer_text_new ();
    ctk_tree_view_column_pack_start (column, cell, FALSE);
    ctk_tree_view_column_set_attributes (column, cell,
                                         "text", 1,
                                         NULL);

    ctk_tree_view_column_set_title (column, "Not Shown");
    ctk_tree_view_append_column (CTK_TREE_VIEW (treeview), column);
    ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (treeview), FALSE);

    columns = ctk_tree_view_get_columns (CTK_TREE_VIEW (tree));

    for(it = columns; it; it = it->next)
    {
        CtkTreeViewColumn *column = static_cast<CtkTreeViewColumn*>(it->data);
        CtkTreeIter iter;
        const gchar *title;
        gboolean visible;
        gint column_id;

        title = ctk_tree_view_column_get_title (column);
        if (!title)
            title = _("Icon");

        column_id = ctk_tree_view_column_get_sort_column_id(column);
        if ((column_id == COL_CGROUP) && (!cgroups_enabled()))
            continue;

        if ((column_id == COL_UNIT ||
             column_id == COL_SESSION ||
             column_id == COL_SEAT ||
             column_id == COL_OWNER)
#ifdef HAVE_SYSTEMD
            && !LOGIND_RUNNING()
#endif
                )
            continue;

        visible = ctk_tree_view_column_get_visible (column);

        ctk_list_store_append (model, &iter);
        ctk_list_store_set (model, &iter, 0, visible, 1, title, 2, column,-1);
    }

    g_list_free(columns);

    return vbox;
}

void
procdialog_create_preferences_dialog (ProcData *procdata)
{
    static CtkWidget *dialog = NULL;

    typedef SpinButtonUpdater SBU;

    static SBU interval_updater("update-interval");
    static SBU graph_interval_updater("graph-update-interval");
    static SBU disks_interval_updater("disks-interval");

    CtkWidget *notebook;
    CtkWidget *proc_box;
    CtkWidget *sys_box;
    CtkWidget *main_vbox;
    CtkWidget *vbox, *vbox2, *vbox3;
    CtkWidget *hbox, *hbox2, *hbox3;
    CtkWidget *label;
    CtkAdjustment *adjustment;
    CtkWidget *spin_button;
    CtkWidget *check_button;
    CtkWidget *tab_label;
    CtkWidget *smooth_button;
    CtkSizeGroup *size;
    gfloat update;
    gchar *tmp;

    if (prefs_dialog)
        return;

    size = ctk_size_group_new (CTK_SIZE_GROUP_HORIZONTAL);

    dialog = ctk_dialog_new_with_buttons (_("System Monitor Preferences"),
                                          CTK_WINDOW (procdata->app),
                                          CTK_DIALOG_DESTROY_WITH_PARENT,
                                          "ctk-help", CTK_RESPONSE_HELP,
                                          "ctk-close", CTK_RESPONSE_CLOSE,
                                          NULL);
    /* FIXME: we should not declare the window size, but let it's   */
    /* driven by window childs. The problem is that the fields list */
    /* have to show at least 4 items to respect HIG. I don't know   */
    /* any function to set list height by contents/items inside it. */
    ctk_window_set_default_size (CTK_WINDOW (dialog), 400, 500);
    ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);
    prefs_dialog = dialog;

    main_vbox = ctk_dialog_get_content_area (CTK_DIALOG (dialog));
    ctk_box_set_spacing (CTK_BOX (main_vbox), 2);

    notebook = ctk_notebook_new ();
    ctk_container_set_border_width (CTK_CONTAINER (notebook), 5);
    ctk_box_pack_start (CTK_BOX (main_vbox), notebook, TRUE, TRUE, 0);

    proc_box = ctk_box_new (CTK_ORIENTATION_VERTICAL, 18);
    ctk_container_set_border_width (CTK_CONTAINER (proc_box), 12);
    tab_label = ctk_label_new (_("Processes"));
    ctk_widget_show (tab_label);
    ctk_notebook_append_page (CTK_NOTEBOOK (notebook), proc_box, tab_label);

    vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (CTK_BOX (proc_box), vbox, FALSE, FALSE, 0);

    tmp = g_strdup_printf ("<b>%s</b>", _("Behavior"));
    label = ctk_label_new (NULL);
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_label_set_markup (CTK_LABEL (label), tmp);
    g_free (tmp);
    ctk_box_pack_start (CTK_BOX (vbox), label, FALSE, FALSE, 0);

    hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
    ctk_box_pack_start (CTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    label = ctk_label_new ("    ");
    ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox2 = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (CTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

    hbox2 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start (CTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    label = ctk_label_new_with_mnemonic (_("_Update interval in seconds:"));
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_box_pack_start (CTK_BOX (hbox2), label, FALSE, FALSE, 0);

    hbox3 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
    ctk_box_pack_start (CTK_BOX (hbox2), hbox3, TRUE, TRUE, 0);

    update = (gfloat) procdata->config.update_interval;
    adjustment = (CtkAdjustment *) ctk_adjustment_new(update / 1000.0,
                                   MIN_UPDATE_INTERVAL / 1000,
                                   MAX_UPDATE_INTERVAL / 1000,
                                   0.25,
                                   1.0,
                                   0);

    spin_button = ctk_spin_button_new (adjustment, 1.0, 2);
    ctk_box_pack_start (CTK_BOX (hbox3), spin_button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK (SBU::callback), &interval_updater);
    ctk_label_set_mnemonic_widget (CTK_LABEL (label), spin_button);


    hbox2 = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
    ctk_box_pack_start(CTK_BOX(vbox2), hbox2, FALSE, FALSE, 0);

    smooth_button = ctk_check_button_new_with_mnemonic(_("Enable _smooth refresh"));
    ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(smooth_button),
                                 g_settings_get_boolean(procdata->settings,
                                                        SmoothRefresh::KEY.c_str()));
    g_signal_connect(G_OBJECT(smooth_button), "toggled",
                     G_CALLBACK(smooth_refresh_toggled), procdata);
    ctk_box_pack_start(CTK_BOX(hbox2), smooth_button, TRUE, TRUE, 0);



    hbox2 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
    ctk_box_pack_start (CTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    check_button = ctk_check_button_new_with_mnemonic (_("Alert before ending or _killing processes"));
    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (check_button),
                                  procdata->config.show_kill_warning);
    g_signal_connect (G_OBJECT (check_button), "toggled",
                                G_CALLBACK (show_kill_dialog_toggled), procdata);
    ctk_box_pack_start (CTK_BOX (hbox2), check_button, FALSE, FALSE, 0);




    hbox2 = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
    ctk_box_pack_start(CTK_BOX(vbox2), hbox2, FALSE, FALSE, 0);

    CtkWidget *solaris_button = ctk_check_button_new_with_mnemonic(_("Divide CPU usage by CPU count"));
    ctk_widget_set_tooltip_text(solaris_button, _("Solaris mode"));
    ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(solaris_button),
                                 g_settings_get_boolean(procdata->settings,
                                                        procman::settings::solaris_mode.c_str()));
    g_signal_connect(G_OBJECT(solaris_button), "toggled",
                     G_CALLBACK(solaris_mode_toggled), procdata);
    ctk_box_pack_start(CTK_BOX(hbox2), solaris_button, TRUE, TRUE, 0);




    hbox2 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
    ctk_box_pack_start (CTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (CTK_BOX (proc_box), vbox, TRUE, TRUE, 0);

    tmp = g_strdup_printf ("<b>%s</b>", _("Information Fields"));
    label = ctk_label_new (NULL);
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_label_set_markup (CTK_LABEL (label), tmp);
    g_free (tmp);
    ctk_box_pack_start (CTK_BOX (vbox), label, FALSE, FALSE, 0);

    hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
    ctk_box_pack_start (CTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    label = ctk_label_new ("    ");
    ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox2 = create_field_page (procdata->tree, "proctree", _("Process i_nformation shown in list:"));
    ctk_box_pack_start (CTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

    sys_box = ctk_box_new (CTK_ORIENTATION_VERTICAL, 12);
    ctk_container_set_border_width (CTK_CONTAINER (sys_box), 12);
    tab_label = ctk_label_new (_("Resources"));
    ctk_widget_show (tab_label);
    ctk_notebook_append_page (CTK_NOTEBOOK (notebook), sys_box, tab_label);

    vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (CTK_BOX (sys_box), vbox, FALSE, FALSE, 0);

    tmp = g_strdup_printf ("<b>%s</b>", _("Graphs"));
    label = ctk_label_new (NULL);
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_label_set_markup (CTK_LABEL (label), tmp);
    g_free (tmp);
    ctk_box_pack_start (CTK_BOX (vbox), label, TRUE, FALSE, 0);

    hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
    ctk_box_pack_start (CTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    label = ctk_label_new ("    ");
    ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox2 = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (CTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

    hbox2 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start (CTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    label = ctk_label_new_with_mnemonic (_("_Update interval in 1/10 sec:"));
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_box_pack_start (CTK_BOX (hbox2), label, FALSE, FALSE, 0);
    ctk_size_group_add_widget (size, label);

    hbox3 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
    ctk_box_pack_start (CTK_BOX (hbox2), hbox3, TRUE, TRUE, 0);

    update = (gfloat) procdata->config.graph_update_interval;
    adjustment = (CtkAdjustment *) ctk_adjustment_new(update / 1000.0, 0.25,
                                                      100.0, 0.25, 1.0, 0);
    spin_button = ctk_spin_button_new (adjustment, 1.0, 2);
    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK(SBU::callback),
                      &graph_interval_updater);
    ctk_box_pack_start (CTK_BOX (hbox3), spin_button, FALSE, FALSE, 0);
    ctk_label_set_mnemonic_widget (CTK_LABEL (label), spin_button);


    CtkWidget *bits_button;
    bits_button = ctk_check_button_new_with_mnemonic(_("Show network speed in bits"));
    ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(bits_button),
                                 g_settings_get_boolean(procdata->settings,
                                                        procman::settings::network_in_bits.c_str()));

    g_signal_connect(G_OBJECT(bits_button), "toggled",
                     G_CALLBACK(network_in_bits_toggled), procdata);
    ctk_box_pack_start(CTK_BOX(vbox2), bits_button, TRUE, TRUE, 0);



    hbox2 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start (CTK_BOX (vbox2), hbox2, TRUE, TRUE, 0);

    /*
     * Devices
     */
    vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_container_set_border_width (CTK_CONTAINER (vbox), 12);
    tab_label = ctk_label_new (_("File Systems"));
    ctk_widget_show (tab_label);
    ctk_notebook_append_page (CTK_NOTEBOOK (notebook), vbox, tab_label);

    tmp = g_strdup_printf ("<b>%s</b>", _("File Systems"));
    label = ctk_label_new (NULL);
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_label_set_markup (CTK_LABEL (label), tmp);
    g_free (tmp);
    ctk_box_pack_start (CTK_BOX (vbox), label, FALSE, FALSE, 0);

    hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
    ctk_box_pack_start (CTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    label = ctk_label_new ("    ");
    ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox2 = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (CTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

    hbox2 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start (CTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    label = ctk_label_new_with_mnemonic (_("_Update interval in seconds:"));
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_box_pack_start (CTK_BOX (hbox2), label, FALSE, FALSE, 0);

    hbox3 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
    ctk_box_pack_start (CTK_BOX (hbox2), hbox3, TRUE, TRUE, 0);

    update = (gfloat) procdata->config.disks_update_interval;
    adjustment = (CtkAdjustment *) ctk_adjustment_new (update / 1000.0, 1.0,
                                                       100.0, 1.0, 1.0, 0);
    spin_button = ctk_spin_button_new (adjustment, 1.0, 0);
    ctk_box_pack_start (CTK_BOX (hbox3), spin_button, FALSE, FALSE, 0);
    ctk_label_set_mnemonic_widget (CTK_LABEL (label), spin_button);
    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK(SBU::callback),
                      &disks_interval_updater);


    hbox2 = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
    ctk_box_pack_start (CTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);
    check_button = ctk_check_button_new_with_mnemonic (_("Show _all file systems"));
    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (check_button),
                                  procdata->config.show_all_fs);
    g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK (show_all_fs_toggled), procdata);
    ctk_box_pack_start (CTK_BOX (hbox2), check_button, FALSE, FALSE, 0);


    vbox2 = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (CTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

    label = ctk_label_new ("    ");
    ctk_box_pack_start (CTK_BOX (vbox2), label, FALSE, FALSE, 0);

    tmp = g_strdup_printf ("<b>%s</b>", _("Information Fields"));
    label = ctk_label_new (NULL);
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_label_set_markup (CTK_LABEL (label), tmp);
    g_free (tmp);
    ctk_box_pack_start (CTK_BOX (vbox), label, FALSE, FALSE, 0);

    hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
    ctk_box_pack_start (CTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    label = ctk_label_new ("    ");
    ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox3 = create_field_page (procdata->disk_list, "disktreenew", _("File system i_nformation shown in list:"));
    ctk_box_pack_start (CTK_BOX (hbox), vbox3, TRUE, TRUE, 0);

    ctk_widget_show_all (dialog);
    g_signal_connect (G_OBJECT (dialog), "response",
                      G_CALLBACK (prefs_dialog_button_pressed), procdata);

    switch (procdata->config.current_tab) {
    case PROCMAN_TAB_SYSINFO:
    case PROCMAN_TAB_PROCESSES:
        ctk_notebook_set_current_page(CTK_NOTEBOOK(notebook), 0);
        break;
    case PROCMAN_TAB_RESOURCES:
        ctk_notebook_set_current_page(CTK_NOTEBOOK(notebook), 1);
        break;
    case PROCMAN_TAB_DISKS:
        ctk_notebook_set_current_page(CTK_NOTEBOOK(notebook), 2);
        break;

    }
}



static char *
procman_action_to_command(ProcmanActionType type,
                          gint pid,
                          gint extra_value)
{
    switch (type) {
    case PROCMAN_ACTION_KILL:
           return g_strdup_printf("kill -s %d %d", extra_value, pid);
    case PROCMAN_ACTION_RENICE:
        return g_strdup_printf("renice %d %d", extra_value, pid);
    default:
        g_assert_not_reached();
    }
}


/*
 * type determines whether if dialog is for killing process or renice.
 * type == PROCMAN_ACTION_KILL,   extra_value -> signal to send
 * type == PROCMAN_ACTION_RENICE, extra_value -> new priority.
 */
gboolean
procdialog_create_root_password_dialog(ProcmanActionType type,
                                       ProcData *procdata,
                                       gint pid,
                                       gint extra_value)
{
    char * command;
    gboolean ret = FALSE;

    command = procman_action_to_command(type, pid, extra_value);

    procman_debug("Trying to run '%s' as root", command);

    if (procman_has_pkexec())
        ret = procman_pkexec_create_root_password_dialog(command);
    else if (procman_has_gksu())
        ret = procman_gksu_create_root_password_dialog(command);

    g_free(command);
    return ret;
}
