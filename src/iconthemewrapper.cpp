#include <config.h>

#include <ctkmm/icontheme.h>
#include <giomm/error.h>

#include "iconthemewrapper.h"


Glib::RefPtr<Cdk::Pixbuf>
procman::IconThemeWrapper::load_icon(const Glib::ustring& icon_name, int size) const
{
    gint scale = cdk_window_get_scale_factor (cdk_get_default_root_window ());
    try
    {
      return Ctk::IconTheme::get_default()->load_icon(icon_name, size, scale, Ctk::ICON_LOOKUP_USE_BUILTIN | Ctk::ICON_LOOKUP_FORCE_SIZE);
    }
    catch (Ctk::IconThemeError &error)
    {
        if (error.code() != Ctk::IconThemeError::ICON_THEME_NOT_FOUND)
            g_error("Cannot load icon '%s' from theme: %s", icon_name.c_str(), error.what().c_str());
        return Glib::RefPtr<Cdk::Pixbuf>();
    }
    catch (Gio::Error &error)
    {
        g_debug("Could not load icon '%s' : %s", icon_name.c_str(), error.what().c_str());
        return Glib::RefPtr<Cdk::Pixbuf>();
    }
}

