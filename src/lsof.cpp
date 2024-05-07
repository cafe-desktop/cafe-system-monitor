#include <config.h>

#include <ctkmm/messagedialog.h>
#include <glibmm/regex.h>
#include <glib/gi18n.h>
#include <glibtop/procopenfiles.h>

#include <sys/wait.h>

#include <set>
#include <string>
#include <sstream>
#include <iterator>

#include <glibmm/regex.h>

#include "procman.h"
#include "lsof.h"
#include "util.h"


using std::string;


namespace
{

    class Lsof
    {
        Glib::RefPtr<Glib::Regex> re;

        bool matches(const string &filename) const
        {
            return this->re->match(filename);
        }

    public:

        Lsof(const string &pattern, bool caseless)
        {
            Glib::RegexCompileFlags flags = static_cast<Glib::RegexCompileFlags>(0);

            if (caseless)
                flags |= Glib::REGEX_CASELESS;

            this->re = Glib::Regex::create(pattern, flags);
        }


        template<typename OutputIterator>
        void search(const ProcInfo &info, OutputIterator out) const
        {
            glibtop_open_files_entry *entries;
            glibtop_proc_open_files buf;

            entries = glibtop_get_proc_open_files(&buf, info.pid);

            for (unsigned i = 0; i != buf.number; ++i) {
                if (entries[i].type & GLIBTOP_FILE_TYPE_FILE) {
                    const string filename(entries[i].info.file.name);
                    if (this->matches(filename))
                        *out++ = filename;
                }
            }

            g_free(entries);
        }
    };



    // GUI Stuff


    enum ProcmanLsof {
        PROCMAN_LSOF_COL_SURFACE,
        PROCMAN_LSOF_COL_PROCESS,
        PROCMAN_LSOF_COL_PID,
        PROCMAN_LSOF_COL_FILENAME,
        PROCMAN_LSOF_NCOLS
    };


    struct GUI {

        GtkListStore *model;
        GtkEntry *entry;
        GtkWindow *window;
        GtkLabel *count;
        ProcData *procdata;
        bool case_insensitive;


        GUI()
        {
            procman_debug("New Lsof GUI %p", this);
        }


        ~GUI()
        {
            procman_debug("Destroying Lsof GUI %p", this);
        }


        void clear_results()
        {
            ctk_list_store_clear(this->model);
            ctk_label_set_text(this->count, "");
        }


        void clear()
        {
            this->clear_results();
            ctk_entry_set_text(this->entry, "");
        }


        void display_regex_error(const Glib::RegexError& error)
        {
            char * msg = g_strdup_printf ("<b>%s</b>\n%s\n%s",
                                          _("Error"),
                                          _("'%s' is not a valid Perl regular expression."),
                                          "%s");
            std::string message = make_string(g_strdup_printf(msg, this->pattern().c_str(), error.what().c_str()));
            g_free(msg);

            Gtk::MessageDialog dialog(message,
                                      true, // use markup
                                      Gtk::MESSAGE_ERROR,
                                      Gtk::BUTTONS_OK,
                                      true); // modal
            dialog.run();
        }


        void update_count(unsigned count)
        {
            std::ostringstream ss;
            ss << count;
            string s = ss.str();
            ctk_label_set_text(this->count, s.c_str());
        }


        string pattern() const
        {
            return ctk_entry_get_text(this->entry);
        }


        void search()
        {
            typedef std::set<string> MatchSet;
            typedef MatchSet::const_iterator iterator;

            this->clear_results();


            try {
                Lsof lsof(this->pattern(), this->case_insensitive);

                unsigned count = 0;

                for (ProcInfo::Iterator it(ProcInfo::begin()); it != ProcInfo::end(); ++it) {
                    const ProcInfo &info(*it->second);

                    MatchSet matches;
                    lsof.search(info, std::inserter(matches, matches.begin()));
                    count += matches.size();

                    for (iterator it(matches.begin()), end(matches.end()); it != end; ++it) {
                        GtkTreeIter file;
                        ctk_list_store_append(this->model, &file);
                        ctk_list_store_set(this->model, &file,
                                           PROCMAN_LSOF_COL_SURFACE, info.surface,
                                           PROCMAN_LSOF_COL_PROCESS, info.name,
                                           PROCMAN_LSOF_COL_PID, info.pid,
                                           PROCMAN_LSOF_COL_FILENAME, it->c_str(),
                                           -1);
                    }
                }

                this->update_count(count);
            }
            catch (Glib::RegexError& error) {
                this->display_regex_error(error);
            }
        }


        static void search_button_clicked(GtkButton *, gpointer data)
        {
            static_cast<GUI*>(data)->search();
        }


        static void search_entry_activate(GtkEntry *, gpointer data)
        {
            static_cast<GUI*>(data)->search();
        }


        static void clear_button_clicked(GtkButton *, gpointer data)
        {
            static_cast<GUI*>(data)->clear();
        }


        static void close_button_clicked(GtkButton *, gpointer data)
        {
            GUI *gui = static_cast<GUI*>(data);
            ctk_widget_destroy(CTK_WIDGET(gui->window));
            delete gui;
        }


        static void case_button_toggled(GtkToggleButton *button, gpointer data)
        {
            bool state = ctk_toggle_button_get_active(button);
            static_cast<GUI*>(data)->case_insensitive = state;
        }


        static gboolean window_delete_event(GtkWidget *, GdkEvent *, gpointer data)
        {
            delete static_cast<GUI*>(data);
            return FALSE;
        }

    };
}




void procman_lsof(ProcData *procdata)
{
    GtkListStore *model = \
        ctk_list_store_new(PROCMAN_LSOF_NCOLS,
                           CAIRO_GOBJECT_TYPE_SURFACE, // PROCMAN_LSOF_COL_SURFACE
                           G_TYPE_STRING,              // PROCMAN_LSOF_COL_PROCESS
                           G_TYPE_UINT,                // PROCMAN_LSOF_COL_PID
                           G_TYPE_STRING               // PROCMAN_LSOF_COL_FILENAME
        );

    GtkWidget *tree = ctk_tree_view_new_with_model(CTK_TREE_MODEL(model));
    g_object_unref(model);

    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    // SURFACE / PROCESS

    column = ctk_tree_view_column_new();

    renderer = ctk_cell_renderer_pixbuf_new();
    ctk_tree_view_column_pack_start(column, renderer, FALSE);
    ctk_tree_view_column_set_attributes(column, renderer,
                                        "surface", PROCMAN_LSOF_COL_SURFACE,
                                        NULL);

    renderer = ctk_cell_renderer_text_new();
    ctk_tree_view_column_pack_start(column, renderer, FALSE);
    ctk_tree_view_column_set_attributes(column, renderer,
                                        "text", PROCMAN_LSOF_COL_PROCESS,
                                        NULL);

    ctk_tree_view_column_set_title(column, _("Process"));
    ctk_tree_view_column_set_sort_column_id(column, PROCMAN_LSOF_COL_PROCESS);
    ctk_tree_view_column_set_resizable(column, TRUE);
    ctk_tree_view_column_set_sizing(column, CTK_TREE_VIEW_COLUMN_GROW_ONLY);
    ctk_tree_view_column_set_min_width(column, 10);
    ctk_tree_view_append_column(CTK_TREE_VIEW(tree), column);
    ctk_tree_sortable_set_sort_column_id(CTK_TREE_SORTABLE(model), PROCMAN_LSOF_COL_PROCESS,
                                         CTK_SORT_ASCENDING);


    // PID
    renderer = ctk_cell_renderer_text_new();
    column = ctk_tree_view_column_new_with_attributes(_("PID"), renderer,
                                                      "text", PROCMAN_LSOF_COL_PID,
                                                      NULL);
    ctk_tree_view_column_set_sort_column_id(column, PROCMAN_LSOF_COL_PID);
    ctk_tree_view_column_set_sizing(column, CTK_TREE_VIEW_COLUMN_GROW_ONLY);
    ctk_tree_view_append_column(CTK_TREE_VIEW(tree), column);


    // FILENAME
    renderer = ctk_cell_renderer_text_new();
    column = ctk_tree_view_column_new_with_attributes(_("Filename"), renderer,
                                                      "text", PROCMAN_LSOF_COL_FILENAME,
                                                      NULL);
    ctk_tree_view_column_set_sort_column_id(column, PROCMAN_LSOF_COL_FILENAME);
    ctk_tree_view_column_set_resizable(column, TRUE);
    ctk_tree_view_column_set_sizing(column, CTK_TREE_VIEW_COLUMN_AUTOSIZE);
    ctk_tree_view_append_column(CTK_TREE_VIEW(tree), column);


    GtkWidget *dialog; /* = ctk_dialog_new_with_buttons(_("Search for Open Files"), NULL,
                                                        CTK_DIALOG_DESTROY_WITH_PARENT,
                                                        CTK_STOCK_CLOSE, CTK_RESPONSE_CLOSE,
                                                        NULL); */
    dialog = ctk_window_new(CTK_WINDOW_TOPLEVEL);
    ctk_window_set_transient_for(CTK_WINDOW(dialog), CTK_WINDOW(procdata->app));
    ctk_window_set_destroy_with_parent(CTK_WINDOW(dialog), TRUE);
    // ctk_window_set_modal(CTK_WINDOW(dialog), TRUE);
    ctk_window_set_title(CTK_WINDOW(dialog), _("Search for Open Files"));

    // g_signal_connect(G_OBJECT(dialog), "response",
    //                           G_CALLBACK(close_dialog), NULL);
    ctk_window_set_resizable(CTK_WINDOW(dialog), TRUE);
    ctk_window_set_default_size(CTK_WINDOW(dialog), 575, 400);
    ctk_container_set_border_width(CTK_CONTAINER(dialog), 12);
    GtkWidget *mainbox = ctk_box_new(CTK_ORIENTATION_VERTICAL, 12);
    ctk_container_add(CTK_CONTAINER(dialog), mainbox);
    ctk_box_set_spacing(CTK_BOX(mainbox), 6);


    // Label, entry and search button

    GtkWidget *hbox1 = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start(CTK_BOX(mainbox), hbox1, FALSE, FALSE, 0);

    GtkWidget *image = ctk_image_new_from_icon_name("edit-find", CTK_ICON_SIZE_DIALOG);
    ctk_box_pack_start(CTK_BOX(hbox1), image, FALSE, FALSE, 0);


    GtkWidget *vbox2 = ctk_box_new(CTK_ORIENTATION_VERTICAL, 12);
    ctk_box_pack_start(CTK_BOX(hbox1), vbox2, TRUE, TRUE, 0);


    GtkWidget *hbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start(CTK_BOX(vbox2), hbox, TRUE, TRUE, 0);
    GtkWidget *label = ctk_label_new_with_mnemonic(_("_Name contains:"));
    ctk_box_pack_start(CTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget *entry = ctk_entry_new();

    ctk_box_pack_start(CTK_BOX(hbox), entry, TRUE, TRUE, 0);

    GtkWidget *search_button = CTK_WIDGET (g_object_new (CTK_TYPE_BUTTON,
                                                         "label", "ctk-find",
                                                         "use-stock", TRUE,
                                                         "use-underline", TRUE,
                                                         NULL));

    ctk_box_pack_start(CTK_BOX(hbox), search_button, FALSE, FALSE, 0);

    GtkWidget *clear_button = CTK_WIDGET (g_object_new (CTK_TYPE_BUTTON,
                                                        "label", "ctk-clear",
                                                        "use-stock", TRUE,
                                                        "use-underline", TRUE,
                                                        NULL));

    /* The default accelerator collides with the default close accelerator. */
    ctk_button_set_label(CTK_BUTTON(clear_button), _("C_lear"));
    ctk_box_pack_start(CTK_BOX(hbox), clear_button, FALSE, FALSE, 0);


    GtkWidget *case_button = ctk_check_button_new_with_mnemonic(_("Case insensitive matching"));
    GtkWidget *hbox3 = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start(CTK_BOX(hbox3), case_button, FALSE, FALSE, 0);
    ctk_box_pack_start(CTK_BOX(vbox2), hbox3, FALSE, FALSE, 0);


    GtkWidget *results_box = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start(CTK_BOX(mainbox), results_box, FALSE, FALSE, 0);
    GtkWidget *results_label = ctk_label_new_with_mnemonic(_("S_earch results:"));
    ctk_box_pack_start(CTK_BOX(results_box), results_label, FALSE, FALSE, 0);
    GtkWidget *count_label = ctk_label_new(NULL);
    ctk_box_pack_end(CTK_BOX(results_box), count_label, FALSE, FALSE, 0);

    // Scrolled TreeView
    GtkWidget *scrolled = ctk_scrolled_window_new(NULL, NULL);
    ctk_scrolled_window_set_policy(CTK_SCROLLED_WINDOW(scrolled),
                                   CTK_POLICY_AUTOMATIC,
                                   CTK_POLICY_AUTOMATIC);
    ctk_scrolled_window_set_shadow_type(CTK_SCROLLED_WINDOW(scrolled),
                                        CTK_SHADOW_IN);
    ctk_container_add(CTK_CONTAINER(scrolled), tree);
    ctk_box_pack_start(CTK_BOX(mainbox), scrolled, TRUE, TRUE, 0);

    GtkWidget *bottom_box = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 12);

    GtkWidget *close_button = CTK_WIDGET (g_object_new (CTK_TYPE_BUTTON,
                                                        "label", "ctk-close",
                                                        "use-stock", TRUE,
                                                        "use-underline", TRUE,
                                                        NULL));

    ctk_box_pack_start(CTK_BOX(mainbox), bottom_box, FALSE, FALSE, 0);
    ctk_box_pack_end(CTK_BOX(bottom_box), close_button, FALSE, FALSE, 0);


    GUI *gui = new GUI; // wil be deleted by the close button or delete-event
    gui->procdata = procdata;
    gui->model = model;
    gui->window = CTK_WINDOW(dialog);
    gui->entry = CTK_ENTRY(entry);
    gui->count = CTK_LABEL(count_label);

    g_signal_connect(G_OBJECT(entry), "activate",
                     G_CALLBACK(GUI::search_entry_activate), gui);
    g_signal_connect(G_OBJECT(clear_button), "clicked",
                     G_CALLBACK(GUI::clear_button_clicked), gui);
    g_signal_connect(G_OBJECT(search_button), "clicked",
                     G_CALLBACK(GUI::search_button_clicked), gui);
    g_signal_connect(G_OBJECT(close_button), "clicked",
                     G_CALLBACK(GUI::close_button_clicked), gui);
    g_signal_connect(G_OBJECT(case_button), "toggled",
                     G_CALLBACK(GUI::case_button_toggled), gui);
    g_signal_connect(G_OBJECT(dialog), "delete-event",
                     G_CALLBACK(GUI::window_delete_event), gui);


    ctk_widget_show_all(dialog);
}

