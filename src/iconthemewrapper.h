#ifndef H_PROCMAN_ICONTHEMEWRAPPER_H_1185707711
#define H_PROCMAN_ICONTHEMEWRAPPER_H_1185707711

#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <ctkmm/icontheme.h>
#include <cdkmm/pixbuf.h>

namespace procman
{
    class IconThemeWrapper
    {
      public:
        // returns 0 instead of raising an exception
        Glib::RefPtr<Cdk::Pixbuf>
            load_icon(const Glib::ustring& icon_name, int size) const;

        const IconThemeWrapper* operator->() const
        { return this; }
    };
}

#endif // H_PROCMAN_ICONTHEMEWRAPPER_H_1185707711
