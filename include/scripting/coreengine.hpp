#ifndef NEW_ENGINE
#define NEW_ENGINE

#include "containers/reusableobjectvector.hpp"
#include "state/substate.hpp"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <sol.hpp>
#include <vector>

namespace BlueBear::Scripting {

  class CoreEngine : public State::Substate {
    static constexpr const char* USER_MODPACK_DIRECTORY = "modpacks/user/";
    static constexpr const char* SYSTEM_MODPACK_DIRECTORY = "modpacks/system/";

    sol::state lua;
    Containers::ReusableObjectVector< std::pair< int, sol::function > > queuedCallbacks;

    CoreEngine( State::State& state );

    void setupCoreEnvironment();
    int setTimeout( int interval, sol::function f );
    
    static sol::function bind( sol::function f, sol::variadic_args args );

  public:
    bool update() override;
  };

}

#endif
