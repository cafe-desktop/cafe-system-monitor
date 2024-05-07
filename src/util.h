// -*- mode: c++ -*-

#ifndef H_CAFE_SYSTEM_MONITOR_UTIL_1123178725
#define H_CAFE_SYSTEM_MONITOR_UTIL_1123178725

#include <glib.h>
#include <ctk/ctk.h>
#include <time.h>
#include <string>

using std::string;

/* check if logind is running */
#define LOGIND_RUNNING() (access("/run/systemd/seats/", F_OK) >= 0)

CtkWidget*
procman_make_label_for_mmaps_or_ofiles(const char *format,
                                       const char *process_name,
                                       unsigned pid);

gboolean
load_symbols(const char *module, ...) G_GNUC_NULL_TERMINATED;

gchar *
procman_format_date_for_display(time_t time_raw);

const char*
format_process_state(guint state);

void
procman_debug_real(const char *file, int line, const char *func,
                   const char *format, ...) G_GNUC_PRINTF(4, 5);

#define procman_debug(FMT, ...) procman_debug_real(__FILE__, __LINE__, __func__, FMT, ##__VA_ARGS__)

inline string make_string(char *c_str)
{
    if (!c_str) {
        procman_debug("NULL string");
        return string();
    }

    string s(c_str);
    g_free(c_str);
    return s;
}


namespace procman
{
    gchar* format_duration_for_display(unsigned centiseconds);

    void memory_size_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                                    CtkTreeModel *model, CtkTreeIter *iter,
                                    gpointer user_data);

    void io_rate_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                                CtkTreeModel *model, CtkTreeIter *iter,
                                gpointer user_data);

    void memory_size_na_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                                       CtkTreeModel *model, CtkTreeIter *iter,
                                       gpointer user_data);

    void storage_size_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                                     CtkTreeModel *model, CtkTreeIter *iter,
                                     gpointer user_data);

    void storage_size_na_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                                        CtkTreeModel *model, CtkTreeIter *iter,
                                        gpointer user_data);

    void duration_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                   CtkTreeModel *model, CtkTreeIter *iter,
                   gpointer user_data);

    void time_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                             CtkTreeModel *model, CtkTreeIter *iter,
                             gpointer user_data);

    void status_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                               CtkTreeModel *model, CtkTreeIter *iter,
                               gpointer user_data);
    void priority_cell_data_func(CtkTreeViewColumn *col, CtkCellRenderer *renderer,
                               CtkTreeModel *model, CtkTreeIter *iter,
                               gpointer user_data);
    gint priority_compare_func(CtkTreeModel* model, CtkTreeIter* first,
                            CtkTreeIter* second, gpointer user_data);
    gint number_compare_func(CtkTreeModel* model, CtkTreeIter* first,
                            CtkTreeIter* second, gpointer user_data);


    template<typename T>
    void poison(T &t, char c)
    {
        memset(&t, c, sizeof t);
    }



    //
    // Stuff to update a tree_store in a smart way
    //

    template<typename T>
    void tree_store_update(CtkTreeModel* model, CtkTreeIter* iter, int column, const T& new_value)
    {
        T current_value;

        ctk_tree_model_get(model, iter, column, &current_value, -1);

        if (current_value != new_value)
            ctk_tree_store_set(CTK_TREE_STORE(model), iter, column, new_value, -1);
    }

    // undefined
    // catch every thing about pointers
    // just to make sure i'm not doing anything wrong
    template<typename T>
    void tree_store_update(CtkTreeModel* model, CtkTreeIter* iter, int column, T* new_value);

    // specialized versions for strings
    template<>
    void tree_store_update<const char>(CtkTreeModel* model, CtkTreeIter* iter, int column, const char* new_value);

    template<>
    inline void tree_store_update<char>(CtkTreeModel* model, CtkTreeIter* iter, int column, char* new_value)
    {
         tree_store_update<const char>(model, iter, column, new_value);
    }

    gchar* get_nice_level (gint nice);
}


#endif /* H_CAFE_SYSTEM_MONITOR_UTIL_1123178725 */
