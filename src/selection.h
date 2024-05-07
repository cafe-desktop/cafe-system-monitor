#ifndef H_CAFE_SYSTEM_MONITOR_SELECTION_H_1183113337
#define H_CAFE_SYSTEM_MONITOR_SELECTION_H_1183113337

#include <sys/types.h>
#include <ctk/ctk.h>
#include <vector>

namespace procman
{
    class SelectionMemento
    {
        std::vector<pid_t> pids;
        static void add_to_selected(CtkTreeModel* model, CtkTreePath* path, CtkTreeIter* iter, gpointer data);

    public:
        void save(CtkWidget* tree);
        void restore(CtkWidget* tree);
    };
}

#endif /* H_CAFE_SYSTEM_MONITOR_SELECTION_H_1183113337 */
