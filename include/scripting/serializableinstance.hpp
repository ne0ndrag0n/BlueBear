#ifndef BBOBJECT
#define BBOBJECT

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <jsoncpp/json/json.h>
#include "bbtypes.hpp"
#include <string>

namespace BlueBear {
	namespace Scripting {
		class SerializableInstance {

			private:
				lua_State* L;
				static Json::FastWriter writer;
				// Don't you EVER modify this value this outside of Engine! EVER!!
				// Read-only!! Do not TOUCH!
				const Tick& currentTickReference;

			public:
				std::string cid;
				bool ok = false;
				int luaVMInstance;
				std::string classID;
				SerializableInstance( lua_State* L, const Tick& currentTickReference, const Json::Value& serialEntity );
				SerializableInstance( lua_State* L, const Tick& currentTickReference, const std::string& classID );

				void createEntityTable();
				void createEntityTable( const Json::Value& serialEntity );
				void registerCallback( const std::string& callback, Tick tick );
				void deferCallback( const std::string& callback );
				void onCreate();
				void execute();
				char* save();
				void load( char* pickledObject );
				bool good();
		};
	}
}

#endif