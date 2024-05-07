/* Procman - main window
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
#include <ctk/ctk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>

#include "procman.h"
#include "callbacks.h"
#include "interface.h"
#include "proctable.h"
#include "procactions.h"
#include "load-graph.h"
#include "util.h"
#include "disks.h"
#include "sysinfo.h"
#include "gsm_color_button.h"

static void    cb_toggle_tree (GtkAction *action, gpointer data);
static void    cb_proc_goto_tab (gint tab);

static const GtkActionEntry menu_entries[] =
{
    // xgettext: noun, top level menu.
    // "File" did not make sense for system-monitor
    { "Monitor", NULL, N_("_Monitor") },
    { "Edit", NULL, N_("_Edit") },
    { "View", NULL, N_("_View") },
    { "Help", NULL, N_("_Help") },

    { "Lsof", "edit-find", N_("Search for _Open Files"), "<control>O",
      N_("Search for open files"), G_CALLBACK(cb_show_lsof) },
    { "Quit", "application-exit", N_("_Quit"), "<control>Q",
      N_("Quit the program"), G_CALLBACK (cb_app_exit) },


    { "StopProcess", NULL, N_("_Stop Process"), "<control>S",
      N_("Stop process"), G_CALLBACK(cb_kill_sigstop) },
    { "ContProcess", NULL, N_("_Continue Process"), "<control>C",
      N_("Continue process if stopped"), G_CALLBACK(cb_kill_sigcont) },

    { "EndProcess", NULL, N_("_End Process"), "<control>E",
      N_("Force process to finish normally"), G_CALLBACK (cb_end_process) },
    { "KillProcess", NULL, N_("_Kill Process"), "<control>K",
      N_("Force process to finish immediately"), G_CALLBACK (cb_kill_process) },
    { "ChangePriority", NULL, N_("_Change Priority"), NULL,
      N_("Change the order of priority of process"), NULL },
    { "Preferences", "preferences-desktop", N_("_Preferences"), NULL,
      N_("Configure the application"), G_CALLBACK (cb_edit_preferences) },

    { "Refresh", "view-refresh", N_("_Refresh"), "<control>R",
      N_("Refresh the process list"), G_CALLBACK(cb_user_refresh) },

    { "MemoryMaps", NULL, N_("_Memory Maps"), "<control>M",
      N_("Open the memory maps associated with a process"), G_CALLBACK (cb_show_memory_maps) },
    // Translators: this means 'Files that are open' (open is no verb here
    { "OpenFiles", NULL, N_("Open _Files"), "<control>F",
      N_("View the files opened by a process"), G_CALLBACK (cb_show_open_files) },
    { "ProcessProperties", NULL, N_("_Properties"), NULL,
      N_("View additional information about a process"), G_CALLBACK (cb_show_process_properties) },


    { "HelpContents", "help-browser", N_("_Contents"), "F1",
      N_("Open the manual"), G_CALLBACK (cb_help_contents) },
    { "About", "help-about", N_("_About"), NULL,
      N_("About this application"), G_CALLBACK (cb_about) }
};

static const GtkToggleActionEntry toggle_menu_entries[] =
{
    { "ShowDependencies", NULL, N_("_Dependencies"), "<control>D",
      N_("Show parent/child relationship between processes"),
      G_CALLBACK (cb_toggle_tree), TRUE },
};


static const GtkRadioActionEntry radio_menu_entries[] =
{
  { "ShowActiveProcesses", NULL, N_("_Active Processes"), NULL,
    N_("Show active processes"), ACTIVE_PROCESSES },
  { "ShowAllProcesses", NULL, N_("A_ll Processes"), NULL,
    N_("Show all processes"), ALL_PROCESSES },
  { "ShowMyProcesses", NULL, N_("M_y Processes"), NULL,
    N_("Show only user-owned processes"), MY_PROCESSES }
};

static const GtkRadioActionEntry priority_menu_entries[] =
{
    { "VeryHigh", NULL, N_("Very High"), NULL,
      N_("Set process priority to very high"), VERY_HIGH_PRIORITY },
    { "High", NULL, N_("High"), NULL,
      N_("Set process priority to high"), HIGH_PRIORITY },
    { "Normal", NULL, N_("Normal"), NULL,
      N_("Set process priority to normal"), NORMAL_PRIORITY },
    { "Low", NULL, N_("Low"), NULL,
      N_("Set process priority to low"), LOW_PRIORITY },
    { "VeryLow", NULL, N_("Very Low"), NULL,
      N_("Set process priority to very low"), VERY_LOW_PRIORITY },
    { "Custom", NULL, N_("Custom"), NULL,
      N_("Set process priority manually"), CUSTOM_PRIORITY }
};


static const char ui_info[] =
    "  <menubar name=\"MenuBar\">"
    "    <menu name=\"MonitorMenu\" action=\"Monitor\">"
    "      <menuitem name=\"MonitorLsofMenu\" action=\"Lsof\" />"
    "      <menuitem name=\"MonitorQuitMenu\" action=\"Quit\" />"
    "    </menu>"
    "    <menu name=\"EditMenu\" action=\"Edit\">"
    "      <menuitem name=\"EditStopProcessMenu\" action=\"StopProcess\" />"
    "      <menuitem name=\"EditContProcessMenu\" action=\"ContProcess\" />"
    "      <separator />"
    "      <menuitem name=\"EditEndProcessMenu\" action=\"EndProcess\" />"
    "      <menuitem name=\"EditKillProcessMenu\" action=\"KillProcess\" />"
    "      <separator />"
    "      <menu name=\"EditChangePriorityMenu\" action=\"ChangePriority\" >"
    "        <menuitem action=\"VeryHigh\" />"
    "        <menuitem action=\"High\" />"
    "        <menuitem action=\"Normal\" />"
    "        <menuitem action=\"Low\" />"
    "        <menuitem action=\"VeryLow\" />"
    "        <separator />"
    "        <menuitem action=\"Custom\"/>"
    "      </menu>"
    "      <separator />"
    "      <menuitem name=\"EditPreferencesMenu\" action=\"Preferences\" />"
    "    </menu>"
    "    <menu name=\"ViewMenu\" action=\"View\">"
    "      <menuitem name=\"ViewActiveProcesses\" action=\"ShowActiveProcesses\" />"
    "      <menuitem name=\"ViewAllProcesses\" action=\"ShowAllProcesses\" />"
    "      <menuitem name=\"ViewMyProcesses\" action=\"ShowMyProcesses\" />"
    "      <separator />"
    "      <menuitem name=\"ViewDependenciesMenu\" action=\"ShowDependencies\" />"
    "      <separator />"
    "      <menuitem name=\"ViewMemoryMapsMenu\" action=\"MemoryMaps\" />"
    "      <menuitem name=\"ViewOpenFilesMenu\" action=\"OpenFiles\" />"
    "      <separator />"
    "      <menuitem name=\"ViewProcessPropertiesMenu\" action=\"ProcessProperties\" />"
    "      <separator />"
    "      <menuitem name=\"ViewRefresh\" action=\"Refresh\" />"
    "    </menu>"
    "    <menu name=\"HelpMenu\" action=\"Help\">"
    "      <menuitem name=\"HelpContentsMenu\" action=\"HelpContents\" />"
    "      <menuitem name=\"HelpAboutMenu\" action=\"About\" />"
    "    </menu>"
    "  </menubar>"
    "  <popup name=\"PopupMenu\" action=\"Popup\">"
    "    <menuitem action=\"StopProcess\" />"
    "    <menuitem action=\"ContProcess\" />"
    "    <separator />"
    "    <menuitem action=\"EndProcess\" />"
    "    <menuitem action=\"KillProcess\" />"
    "    <separator />"
    "    <menu name=\"ChangePriorityMenu\" action=\"ChangePriority\" >"
    "      <menuitem action=\"VeryHigh\" />"
    "      <menuitem action=\"High\" />"
    "      <menuitem action=\"Normal\" />"
    "      <menuitem action=\"Low\" />"
    "      <menuitem action=\"VeryLow\" />"
    "      <separator />"
    "      <menuitem action=\"Custom\"/>"
    "    </menu>"
    "    <separator />"
    "    <menuitem action=\"MemoryMaps\" />"
    "    <menuitem action=\"OpenFiles\" />"
    "    <separator />"
    "    <menuitem action=\"ProcessProperties\" />"

    "  </popup>";


static GtkWidget *
create_proc_view (ProcData *procdata)
{
    GtkWidget *vbox1;
    GtkWidget *hbox1;
    GtkWidget *scrolled;
    GtkWidget *hbox2;
    char* string;

    vbox1 = ctk_box_new (GTK_ORIENTATION_VERTICAL, 18);
    ctk_container_set_border_width (GTK_CONTAINER (vbox1), 12);

    hbox1 = ctk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

    string = make_loadavg_string ();
    procdata->loadavg = ctk_label_new (string);
    g_free (string);
    ctk_box_pack_start (GTK_BOX (hbox1), procdata->loadavg, FALSE, FALSE, 0);


    scrolled = proctable_new (procdata);
    if (!scrolled)
        return NULL;
    ctk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);

    ctk_box_pack_start (GTK_BOX (vbox1), scrolled, TRUE, TRUE, 0);


    hbox2 = ctk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    ctk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, FALSE, 0);

    procdata->endprocessbutton = ctk_button_new_with_mnemonic (_("End _Process"));
    ctk_box_pack_end (GTK_BOX (hbox2), procdata->endprocessbutton, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (procdata->endprocessbutton), "clicked",
                      G_CALLBACK (cb_end_process_button_pressed), procdata);


    /* create popup_menu */
     procdata->popup_menu = ctk_ui_manager_get_widget (procdata->uimanager, "/PopupMenu");

    return vbox1;
}


GtkWidget *
make_title_label (const char *text)
{
    GtkWidget *label;
    char *full;

    full = g_strdup_printf ("<span weight=\"bold\">%s</span>", text);
    label = ctk_label_new (full);
    g_free (full);

    ctk_label_set_xalign (GTK_LABEL (label), 0.0);
    ctk_label_set_use_markup (GTK_LABEL (label), TRUE);

    return label;
}


static GtkWidget *
create_sys_view (ProcData *procdata)
{
    GtkWidget *vbox, *hbox;
    GtkWidget *cpu_box, *mem_box, *net_box;
    GtkWidget *cpu_graph_box, *mem_graph_box, *net_graph_box;
    GtkWidget *label,*cpu_label, *spacer;
    GtkWidget *grid;
    GtkWidget *color_picker;
    GtkWidget *mem_legend_box, *net_legend_box;
    LoadGraph *cpu_graph, *mem_graph, *net_graph;

    gint i;
    gchar *title_text;
    gchar *label_text;
    gchar *title_template;

    // Translators: color picker title, %s is CPU, Memory, Swap, Receiving, Sending
    title_template = g_strdup(_("Pick a Color for '%s'"));

    vbox = ctk_box_new (GTK_ORIENTATION_VERTICAL, 18);

    ctk_container_set_border_width (GTK_CONTAINER (vbox), 12);

    /* The CPU BOX */

    cpu_box = ctk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (GTK_BOX (vbox), cpu_box, TRUE, TRUE, 0);

    label = make_title_label (_("CPU History"));
    ctk_box_pack_start (GTK_BOX (cpu_box), label, FALSE, FALSE, 0);

    cpu_graph_box = ctk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (GTK_BOX (cpu_box), cpu_graph_box, TRUE, TRUE, 0);

    cpu_graph = new LoadGraph(LOAD_GRAPH_CPU);
    ctk_box_pack_start (GTK_BOX (cpu_graph_box),
                        load_graph_get_widget(cpu_graph),
                        TRUE,
                        TRUE,
                         0);

    hbox = ctk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    spacer = ctk_label_new ("");
    ctk_widget_set_size_request(GTK_WIDGET(spacer), 57, -1);
    ctk_box_pack_start (GTK_BOX (hbox), spacer,
                        FALSE, FALSE, 0);


    ctk_box_pack_start (GTK_BOX (cpu_graph_box), hbox,
                        FALSE, FALSE, 0);

    GtkWidget* cpu_grid = ctk_grid_new();
    ctk_grid_set_row_spacing(GTK_GRID(cpu_grid), 6);
    ctk_grid_set_column_spacing(GTK_GRID(cpu_grid), 6);
    ctk_grid_set_column_homogeneous(GTK_GRID(cpu_grid), TRUE);
    ctk_box_pack_start(GTK_BOX(hbox), cpu_grid, TRUE, TRUE, 0);

    for (i=0;i<procdata->config.num_cpus; i++) {
        GtkWidget *temp_hbox;

        temp_hbox = ctk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        ctk_widget_set_hexpand (temp_hbox, TRUE);
        ctk_grid_attach(GTK_GRID(cpu_grid), temp_hbox,
                        i % 4, i / 4, 1, 1);

        color_picker = gsm_color_button_new (&cpu_graph->colors.at(i), GSMCP_TYPE_CPU);
        g_signal_connect (G_OBJECT (color_picker), "color_set",
                          G_CALLBACK (cb_cpu_color_changed), GINT_TO_POINTER (i));
        ctk_box_pack_start (GTK_BOX (temp_hbox), color_picker, FALSE, TRUE, 0);
        ctk_widget_set_size_request(GTK_WIDGET(color_picker), 32, -1);
        if(procdata->config.num_cpus == 1) {
            label_text = g_strdup (_("CPU"));
        } else {
            label_text = g_strdup_printf (_("CPU%d"), i+1);
        }
        title_text = g_strdup_printf(title_template, label_text);
        label = ctk_label_new (label_text);
        gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
        g_free(title_text);
        ctk_box_pack_start (GTK_BOX (temp_hbox), label, FALSE, FALSE, 6);
        g_free (label_text);

        cpu_label = ctk_label_new (NULL);
        ctk_label_set_xalign (GTK_LABEL (cpu_label), 0.0);

        ctk_box_pack_start (GTK_BOX (temp_hbox), cpu_label, TRUE, TRUE, 0);
        load_graph_get_labels(cpu_graph)->cpu[i] = cpu_label;

    }

    procdata->cpu_graph = cpu_graph;

    mem_box = ctk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (GTK_BOX (vbox), mem_box, TRUE, TRUE, 0);

    label = make_title_label (_("Memory and Swap History"));
    ctk_box_pack_start (GTK_BOX (mem_box), label, FALSE, FALSE, 0);

    mem_graph_box = ctk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (GTK_BOX (mem_box), mem_graph_box, TRUE, TRUE, 0);


    mem_graph = new LoadGraph(LOAD_GRAPH_MEM);
    ctk_box_pack_start (GTK_BOX (mem_graph_box),
                        load_graph_get_widget(mem_graph),
                        TRUE,
                        TRUE,
                        0);

    hbox = ctk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    spacer = ctk_label_new ("");
    ctk_widget_set_size_request(GTK_WIDGET(spacer), 54, -1);
    ctk_box_pack_start (GTK_BOX (hbox), spacer,
                        FALSE, FALSE, 0);


    ctk_box_pack_start (GTK_BOX (mem_graph_box), hbox,
                        FALSE, FALSE, 0);

    mem_legend_box = ctk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    ctk_box_pack_start (GTK_BOX (hbox), mem_legend_box,
                        TRUE, TRUE, 0);

    grid = ctk_grid_new ();
    ctk_grid_set_row_spacing (GTK_GRID (grid), 6);
    ctk_grid_set_column_spacing (GTK_GRID (grid), 6);
    ctk_box_pack_start (GTK_BOX (mem_legend_box), grid,
                        TRUE, TRUE, 0);

    label_text = g_strdup(_("Memory"));
    color_picker = load_graph_get_mem_color_picker(mem_graph);
    g_signal_connect (G_OBJECT (color_picker), "color_set",
                      G_CALLBACK (cb_mem_color_changed), procdata);
    title_text = g_strdup_printf(title_template, label_text);
    gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
    g_free(title_text);
    ctk_grid_attach (GTK_GRID (grid), color_picker, 0, 0, 1, 2);

    label = ctk_label_new (label_text);
    g_free(label_text);
    ctk_label_set_xalign (GTK_LABEL (label), 0.0);
    ctk_grid_attach (GTK_GRID (grid), label, 1, 0, 7, 1);

    ctk_grid_attach (GTK_GRID (grid),
                      load_graph_get_labels(mem_graph)->memory,
                      1, 1, 1, 1);

    grid = ctk_grid_new ();
    ctk_grid_set_row_spacing (GTK_GRID (grid), 6);
    ctk_grid_set_column_spacing (GTK_GRID (grid), 6);
    ctk_box_pack_start (GTK_BOX (mem_legend_box), grid,
                        TRUE, TRUE, 0);

    label_text = g_strdup(_("Swap"));
    color_picker = load_graph_get_swap_color_picker(mem_graph);
    g_signal_connect (G_OBJECT (color_picker), "color_set",
                      G_CALLBACK (cb_swap_color_changed), procdata);
    title_text = g_strdup_printf(title_template, label_text);
    gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
    g_free(title_text);
    ctk_grid_attach (GTK_GRID (grid), color_picker, 0, 0, 1, 2);

    label = ctk_label_new (label_text);
    g_free(label_text);
    ctk_label_set_xalign (GTK_LABEL (label), 0.0);
    ctk_grid_attach (GTK_GRID (grid), label, 1, 0, 7, 1);

    ctk_grid_attach (GTK_GRID (grid),
                      load_graph_get_labels(mem_graph)->swap,
                      1, 1, 1, 1);

    procdata->mem_graph = mem_graph;

    /* The net box */
    net_box = ctk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (GTK_BOX (vbox), net_box, TRUE, TRUE, 0);

    label = make_title_label (_("Network History"));
    ctk_box_pack_start (GTK_BOX (net_box), label, FALSE, FALSE, 0);

    net_graph_box = ctk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    ctk_box_pack_start (GTK_BOX (net_box), net_graph_box, TRUE, TRUE, 0);

    net_graph = new LoadGraph(LOAD_GRAPH_NET);
    ctk_box_pack_start (GTK_BOX (net_graph_box),
                        load_graph_get_widget(net_graph),
                        TRUE,
                        TRUE,
                        0);

    hbox = ctk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    spacer = ctk_label_new ("");
    ctk_widget_set_size_request(GTK_WIDGET(spacer), 54, -1);
    ctk_box_pack_start (GTK_BOX (hbox), spacer,
                        FALSE, FALSE, 0);


    ctk_box_pack_start (GTK_BOX (net_graph_box), hbox,
                        FALSE, FALSE, 0);

    net_legend_box = ctk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    ctk_box_pack_start (GTK_BOX (hbox), net_legend_box,
                        TRUE, TRUE, 0);

    grid = ctk_grid_new ();
    ctk_grid_set_row_spacing (GTK_GRID (grid), 6);
    ctk_grid_set_column_spacing (GTK_GRID (grid), 6);
    ctk_box_pack_start (GTK_BOX (net_legend_box), grid,
                        TRUE, TRUE, 0);

    label_text = g_strdup(_("Receiving"));

    color_picker = gsm_color_button_new (
        &net_graph->colors.at(0), GSMCP_TYPE_NETWORK_IN);
    g_signal_connect (G_OBJECT (color_picker), "color_set",
                G_CALLBACK (cb_net_in_color_changed), procdata);
    title_text = g_strdup_printf(title_template, label_text);
    gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
    g_free(title_text);
    ctk_grid_attach (GTK_GRID (grid), color_picker, 0, 0, 1, 2);

    label = ctk_label_new (label_text);
    g_free(label_text);
    ctk_label_set_xalign (GTK_LABEL (label), 0.0);
    ctk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);

    ctk_label_set_xalign (GTK_LABEL (load_graph_get_labels(net_graph)->net_in), 1.0);

    ctk_widget_set_size_request(GTK_WIDGET(load_graph_get_labels(net_graph)->net_in), 100, -1);
    ctk_widget_set_hexpand (load_graph_get_labels(net_graph)->net_in, TRUE);
    ctk_grid_attach (GTK_GRID (grid), load_graph_get_labels(net_graph)->net_in, 2, 0, 1, 1);

    label = ctk_label_new (_("Total Received"));
    ctk_label_set_xalign (GTK_LABEL (label), 0.0);
    ctk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

    ctk_label_set_xalign (GTK_LABEL (load_graph_get_labels(net_graph)->net_in_total), 1.0);
    ctk_grid_attach (GTK_GRID (grid),
                     load_graph_get_labels(net_graph)->net_in_total,
                     2, 1, 1, 1);

    spacer = ctk_label_new ("");
    ctk_widget_set_size_request(GTK_WIDGET(spacer), 38, -1);
    ctk_grid_attach (GTK_GRID (grid), spacer, 3, 0, 1, 1);

    grid = ctk_grid_new ();
    ctk_grid_set_row_spacing (GTK_GRID (grid), 6);
    ctk_grid_set_column_spacing (GTK_GRID (grid), 6);
    ctk_box_pack_start (GTK_BOX (net_legend_box), grid,
                        TRUE, TRUE, 0);

    label_text = g_strdup(_("Sending"));

    color_picker = gsm_color_button_new (
        &net_graph->colors.at(1), GSMCP_TYPE_NETWORK_OUT);
    g_signal_connect (G_OBJECT (color_picker), "color_set",
                G_CALLBACK (cb_net_out_color_changed), procdata);
    title_text = g_strdup_printf(title_template, label_text);
    gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
    g_free(title_text);
    ctk_grid_attach (GTK_GRID (grid), color_picker, 0, 0, 1, 2);

    label = ctk_label_new (label_text);
    g_free(label_text);
    ctk_label_set_xalign (GTK_LABEL (label), 0.0);
    ctk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);

    ctk_label_set_xalign (GTK_LABEL (load_graph_get_labels(net_graph)->net_out), 1.0);

    ctk_widget_set_size_request(GTK_WIDGET(load_graph_get_labels(net_graph)->net_out), 100, -1);
    ctk_widget_set_hexpand (load_graph_get_labels(net_graph)->net_out, TRUE);
    ctk_grid_attach (GTK_GRID (grid), load_graph_get_labels(net_graph)->net_out, 2, 0, 1, 1);

    label = ctk_label_new (_("Total Sent"));
    ctk_label_set_xalign (GTK_LABEL (label), 0.0);
    ctk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

    ctk_label_set_xalign (GTK_LABEL (load_graph_get_labels(net_graph)->net_out_total), 1.0);
    ctk_grid_attach (GTK_GRID (grid),
                      load_graph_get_labels(net_graph)->net_out_total,
                      2, 1, 1, 1);

    spacer = ctk_label_new ("");
    ctk_widget_set_size_request(GTK_WIDGET(spacer), 38, -1);
    ctk_grid_attach (GTK_GRID (grid), spacer, 3, 0, 1, 1);

    procdata->net_graph = net_graph;
    g_free(title_template);

    return vbox;
}

static void
menu_item_select_cb (GtkMenuItem *proxy,
                     ProcData *procdata)
{
    GtkAction *action;
    char *message;

    action = ctk_activatable_get_related_action (GTK_ACTIVATABLE(proxy));
    g_assert(action);

    g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
    if (message)
    {
        ctk_statusbar_push (GTK_STATUSBAR (procdata->statusbar),
                    procdata->tip_message_cid, message);
        g_free (message);
    }
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
                       ProcData *procdata)
{
    ctk_statusbar_pop (GTK_STATUSBAR (procdata->statusbar),
               procdata->tip_message_cid);
}

static void
connect_proxy_cb (GtkUIManager *manager,
                  GtkAction *action,
                  GtkWidget *proxy,
                  ProcData *procdata)
{
    if (GTK_IS_MENU_ITEM (proxy)) {
        g_signal_connect (proxy, "select",
                          G_CALLBACK (menu_item_select_cb), procdata);
        g_signal_connect (proxy, "deselect",
                          G_CALLBACK (menu_item_deselect_cb), procdata);
    }
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
                     GtkAction *action,
                     GtkWidget *proxy,
                     ProcData *procdata)
{
    if (GTK_IS_MENU_ITEM (proxy)) {
        g_signal_handlers_disconnect_by_func
            (proxy, (void*)(G_CALLBACK(menu_item_select_cb)), procdata);
        g_signal_handlers_disconnect_by_func
            (proxy, (void*)(G_CALLBACK(menu_item_deselect_cb)), procdata);
    }
}

void
create_main_window (ProcData *procdata)
{
    gint i;
    gint width, height, xpos, ypos;
    GtkWidget *app;
    GtkAction *action;
    GtkWidget *menubar;
    GtkWidget *main_box;
    GtkWidget *notebook;
    GtkWidget *tab_label1, *tab_label2, *tab_label3;
    GtkWidget *vbox1;
    GtkWidget *sys_box, *devices_box;
    GtkWidget *sysinfo_box, *sysinfo_label;

    app = ctk_window_new(GTK_WINDOW_TOPLEVEL);
    ctk_window_set_title(GTK_WINDOW(app), _("System Monitor"));

    GdkScreen* screen = ctk_widget_get_screen(app);
    /* use visual, if available */
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual)
        ctk_widget_set_visual(app, visual);

    main_box = ctk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    ctk_container_add(GTK_CONTAINER(app), main_box);

    width = procdata->config.width;
    height = procdata->config.height;
    xpos = procdata->config.xpos;
    ypos = procdata->config.ypos;
    ctk_window_set_default_size (GTK_WINDOW (app), width, height);
    ctk_window_move(GTK_WINDOW (app), xpos, ypos);
    ctk_window_set_resizable (GTK_WINDOW (app), TRUE);
    if (procdata->config.maximized) {
        ctk_window_maximize(GTK_WINDOW(app));
    }

    /* create the menubar */
    procdata->uimanager = ctk_ui_manager_new ();

    /* show tooltips in the statusbar */
    g_signal_connect (procdata->uimanager, "connect_proxy",
                      G_CALLBACK (connect_proxy_cb), procdata);
    g_signal_connect (procdata->uimanager, "disconnect_proxy",
                      G_CALLBACK (disconnect_proxy_cb), procdata);

    ctk_window_add_accel_group (GTK_WINDOW (app),
                                ctk_ui_manager_get_accel_group (procdata->uimanager));

    if (!ctk_ui_manager_add_ui_from_string (procdata->uimanager,
                                            ui_info,
                                            -1,
                                            NULL)) {
        g_error("building menus failed");
    }

    procdata->action_group = ctk_action_group_new ("ProcmanActions");
    ctk_action_group_set_translation_domain (procdata->action_group, NULL);
    ctk_action_group_add_actions (procdata->action_group,
                                  menu_entries,
                                  G_N_ELEMENTS (menu_entries),
                                  procdata);
    ctk_action_group_add_toggle_actions (procdata->action_group,
                                         toggle_menu_entries,
                                         G_N_ELEMENTS (toggle_menu_entries),
                                         procdata);

    ctk_action_group_add_radio_actions (procdata->action_group,
                        radio_menu_entries,
                        G_N_ELEMENTS (radio_menu_entries),
                        procdata->config.whose_process,
                        G_CALLBACK(cb_radio_processes),
                        procdata);

    ctk_action_group_add_radio_actions (procdata->action_group,
                                        priority_menu_entries,
                                        G_N_ELEMENTS (priority_menu_entries),
                                        NORMAL_PRIORITY,
                                        G_CALLBACK(cb_renice),
                                        procdata);

    ctk_ui_manager_insert_action_group (procdata->uimanager,
                                        procdata->action_group,
                                        0);

    menubar = ctk_ui_manager_get_widget (procdata->uimanager, "/MenuBar");
    ctk_box_pack_start (GTK_BOX (main_box), menubar, FALSE, FALSE, 0);


    /* create the main notebook */
    procdata->notebook = notebook = ctk_notebook_new ();
    ctk_box_pack_start (GTK_BOX (main_box), notebook, TRUE, TRUE, 0);
    ctk_container_set_border_width (GTK_CONTAINER (notebook), 12);

    sysinfo_box = ctk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); // procman_create_sysinfo_view();
    sysinfo_label = ctk_label_new(_("System"));
    ctk_notebook_append_page(GTK_NOTEBOOK(notebook), sysinfo_box, sysinfo_label);

    vbox1 = create_proc_view (procdata);
    tab_label1 = ctk_label_new (_("Processes"));
    ctk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox1, tab_label1);

    sys_box = create_sys_view (procdata);
    tab_label2 = ctk_label_new (_("Resources"));
    ctk_notebook_append_page (GTK_NOTEBOOK (notebook), sys_box, tab_label2);

    devices_box = create_disk_view (procdata);
    tab_label3 = ctk_label_new (_("File Systems"));
    ctk_notebook_append_page (GTK_NOTEBOOK (notebook), devices_box, tab_label3);

    g_signal_connect (G_OBJECT (notebook), "switch-page",
              G_CALLBACK (cb_switch_page), procdata);
    g_signal_connect (G_OBJECT (notebook), "change-current-page",
              G_CALLBACK (cb_change_current_page), procdata);

    ctk_widget_show_all(notebook); // need to make page switch work
    ctk_notebook_set_current_page (GTK_NOTEBOOK (notebook), procdata->config.current_tab);
    cb_change_current_page (GTK_NOTEBOOK (notebook), procdata->config.current_tab, procdata);
    g_signal_connect (G_OBJECT (app), "delete_event",
                      G_CALLBACK (cb_app_delete),
                      procdata);

    GtkAccelGroup *accel_group;
    GClosure *goto_tab_closure[4];
    accel_group = ctk_accel_group_new ();
    ctk_window_add_accel_group (GTK_WINDOW(app), accel_group);
    for (i = 0; i < 4; ++i) {
        goto_tab_closure[i] = g_cclosure_new_swap (G_CALLBACK (cb_proc_goto_tab),
                                                   GINT_TO_POINTER (i), NULL);
        ctk_accel_group_connect (accel_group, '0'+(i+1),
                                 GDK_MOD1_MASK, GTK_ACCEL_VISIBLE,
                                 goto_tab_closure[i]);
    }

    /* create the statusbar */
    procdata->statusbar = ctk_statusbar_new();
    ctk_box_pack_start(GTK_BOX(main_box), procdata->statusbar, FALSE, FALSE, 0);
    procdata->tip_message_cid = ctk_statusbar_get_context_id
        (GTK_STATUSBAR (procdata->statusbar), "tip_message");

    action = ctk_action_group_get_action (procdata->action_group, "ShowDependencies");
    ctk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                      procdata->config.show_tree);

    ctk_widget_show_all(app);
    procdata->app = app;
}

void
do_popup_menu (ProcData *procdata, GdkEventButton *event)
{
    ctk_menu_popup_at_pointer (GTK_MENU (procdata->popup_menu), NULL);
}

void
update_sensitivity(ProcData *data)
{
    const char * const selected_actions[] = { "StopProcess",
                                              "ContProcess",
                                              "EndProcess",
                                              "KillProcess",
                                              "ChangePriority",
                                              "MemoryMaps",
                                              "OpenFiles",
                                              "ProcessProperties" };

    const char * const processes_actions[] = { "ShowActiveProcesses",
                                               "ShowAllProcesses",
                                               "ShowMyProcesses",
                                               "ShowDependencies",
                                               "Refresh"
    };

    size_t i;
    gboolean processes_sensitivity, selected_sensitivity;
    GtkAction *action;

    processes_sensitivity = (data->config.current_tab == PROCMAN_TAB_PROCESSES);
    selected_sensitivity = (processes_sensitivity && data->selected_process != NULL);

    if(data->endprocessbutton) {
        /* avoid error on startup if endprocessbutton
           has not been built yet */
        ctk_widget_set_sensitive(data->endprocessbutton, selected_sensitivity);
    }

    for (i = 0; i != G_N_ELEMENTS(processes_actions); ++i) {
        action = ctk_action_group_get_action(data->action_group,
                                             processes_actions[i]);
        ctk_action_set_sensitive(action, processes_sensitivity);
    }

    for (i = 0; i != G_N_ELEMENTS(selected_actions); ++i) {
        action = ctk_action_group_get_action(data->action_group,
                                             selected_actions[i]);
        ctk_action_set_sensitive(action, selected_sensitivity);
    }
}

void
block_priority_changed_handlers(ProcData *data, bool block)
{
    gint i;
    if (block) {
        for (i = 0; i != G_N_ELEMENTS(priority_menu_entries); ++i) {
            GtkRadioAction *action = GTK_RADIO_ACTION(ctk_action_group_get_action(data->action_group,
                                             priority_menu_entries[i].name));
            g_signal_handlers_block_by_func(action, (gpointer)cb_renice, data);
        }
    } else {
        for (i = 0; i != G_N_ELEMENTS(priority_menu_entries); ++i) {
            GtkRadioAction *action = GTK_RADIO_ACTION(ctk_action_group_get_action(data->action_group,
                                             priority_menu_entries[i].name));
            g_signal_handlers_unblock_by_func(action, (gpointer)cb_renice, data);
        }
    }
}

static void
cb_toggle_tree (GtkAction *action, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;
    gboolean show;

    show = ctk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
    if (show == procdata->config.show_tree)
        return;

    g_settings_set_boolean (settings, "show-tree", show);
}

static void
cb_proc_goto_tab (gint tab)
{
    ProcData *data = ProcData::get_instance ();
    ctk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), tab);
}
