#ifndef UTILITY
#define UTILITY

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <vector>
#include <string>

namespace BlueBear {
	class Utility {
		public:
			static void stackDump( lua_State* L );

			static void stackDumpAt( lua_State* L, int pos );

			static std::vector< std::string > getSubdirectoryList( const char* rootSubDirectory );

			static void clearLuaStack( lua_State* L );

			static void getTableValue( lua_State* L, const char* key );

			static void setTableIntValue( lua_State* L, const char* key, int value );

			static void setTableStringValue( lua_State* L, const char* key, const char* value );

			static void setTableFunctionValue( lua_State* L, const char* key, lua_CFunction value );

			static std::vector<std::string> split(const std::string &text, char sep);

			static void getTableTreeValue( lua_State* L, const std::string& treeValue );
	};
}


#endif
