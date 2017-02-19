#ifndef LUAELEMENT
#define LUAELEMENT

#include "bbtypes.hpp"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <memory>
#include <SFGUI/Widgets.hpp>
#include <map>

namespace BlueBear {
  namespace Graphics {
    namespace GUI {
      // Reserve space for additional elements we may need for a LuaElement type.
      class LuaElement {
      public:
        std::shared_ptr< sfg::Widget > widget;

        struct SignalBinding {
          LuaReference reference;
          unsigned int slotHandle;
        };

        /**
         * Much of this has to be static as a LuaElement is a functional wrapper around an sfg::Widget
         * as it relates to the Concordia GUI.
         */
        static std::map< void*, std::map< sfg::Signal::SignalID, SignalBinding > > masterSignalMap;
        static int lua_onEvent( lua_State* L );
        static int lua_offEvent( lua_State* L );
        static int lua_getWidgetByID( lua_State* L );
        static int lua_getWidgetsByClass( lua_State* L );
        static int lua_gc( lua_State* L );
        static int lua_getText( lua_State* L );
        static int lua_setText( lua_State* L );

        static void getUserdataFromWidget( lua_State* L, std::shared_ptr< sfg::Widget > widget );
      };
    }
  }
}


#endif
