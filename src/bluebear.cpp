#include "bluebear.hpp"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdint>

// Not X-Platform
#include <dirent.h>

extern "C" {
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
}

namespace BlueBear {
	
	Engine::Engine() {
		L = luaL_newstate();
		luaL_openlibs( L );
	}
	
	Engine::~Engine() {
		lua_close( L );
	}
	
	/**
	 * Setup the global environment all Engine mods will run within. This method sets up required global objects used by each mod.
	 */
	bool Engine::setupRootEnvironment() {
		// TODO: The root script should not be a user-modifiable file, but rather a hardcoded minified string. This script sets up
		// the root Lua environment all mods (including pack-ins) run from, and is NOT to be changed by the user.
		if ( luaL_dofile( L, "system/root.lua" ) ) {
			printf( "Failed to set up BlueBear root environment: %s\n", lua_tostring( L, -1 ) );
			return false;
		}
		
		// Setup the root environment by loading in and "class-ifying" all objects used by the game
		
		// Gets a list of all directories in the "assets/objects" folder. Each folder holds everything required for a BlueBear object.
		std::vector< std::string > directories = Utility::getSubdirectoryList( BLUEBEAR_OBJECTS_DIRECTORY );
		
		// For each of these subdirectories, do the obj.lua file within our lua scope
		for ( std::vector< std::string >::iterator directory = directories.begin(); directory != directories.end(); directory++ ) {
			std::string scriptPath = BLUEBEAR_OBJECTS_DIRECTORY + *directory + "/obj.lua";
			if( luaL_dofile( L, scriptPath.c_str() ) ) {
				printf( "Error in BlueBear object: %s\n", lua_tostring( L, -1 ) );
			}
		}
		
		return true;
	}
	
	bool Engine::loadLot( const char* lotPath ) {
		std::ifstream lot( lotPath, std::ifstream::binary );
		
		if( lot.is_open() && lot.good() ) {
			// Read in unsigned int and verify magic ID
			uint32_t magicID;
			lot.read( ( char* )&magicID, 4 );
			
			if( Utility::swap_uint32( magicID ) == BLUEBEAR_LOT_MAGIC_ID ) {
				std::cout << "Valid BlueBear lot.\n";
				return true;
			} else {
				std::cout << "This doesn't appear to be a valid BlueBear lot.\n";
			}
		}
		
		std::cerr << "Couldn't load lot!\n";
		return false;
	}
	
	/**
	 * Where the magic happens 
	 */
	void Engine::objectLoop() {
		std::cout << "Starting object loop...\n";
	}
	
	Lot::Lot( int floorX, int floorY, int stories, int undergroundStories, BlueBear::TerrainType terrainType ) {
		this->floorX = floorX;
		this->floorY = floorY;
		
		this->stories = stories;
		this->undergroundStories = undergroundStories;
		
		this->terrainType = terrainType;
	}
	
	namespace Utility {
		
		uint16_t swap_uint16( uint16_t val ) {
			return (val << 8) | (val >> 8 );
		}

		int16_t swap_int16( int16_t val ) {
			return (val << 8) | ((val >> 8) & 0xFF);
		}

		uint32_t swap_uint32( uint32_t val ) {
			val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF ); 
			return (val << 16) | (val >> 16);
		}

		int32_t swap_int32( int32_t val ) {
			val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF ); 
			return (val << 16) | ((val >> 16) & 0xFFFF);
		}
		
		/**
		 * Dump the Lua stack out to terminal
		 */
		static void stackDump( lua_State* L ) {
			  int i;
			  int top = lua_gettop(L);
			  for (i = 1; i <= top; i++) {  /* repeat for each level */
				int t = lua_type(L, i);
				switch (t) {
			
				  case LUA_TSTRING:  /* strings */
					printf("`%s'", lua_tostring(L, i));
					break;
			
				  case LUA_TBOOLEAN:  /* booleans */
					printf(lua_toboolean(L, i) ? "true" : "false");
					break;
			
				  case LUA_TNUMBER:  /* numbers */
					printf("%g", lua_tonumber(L, i));
					break;
			
				  default:  /* other values */
					printf("%s", lua_typename(L, t));
					break;
			
				}
				printf("  ");  /* put a separator */
			  }
			  printf("\n");  /* end the listing */
		}
		
		/**
		 * @noxplatform
		 * 
		 * Gets a collection of subdirectories for the given directory
		 */
		std::vector< std::string > getSubdirectoryList( const char* rootSubDirectory ) {
			std::vector< std::string > directories;
			
			DIR *dir = opendir( rootSubDirectory );

			struct dirent* entry = readdir( dir );

			while ( entry != NULL ) {
				if ( entry->d_type == DT_DIR && strcmp( entry->d_name, "." ) != 0 && strcmp( entry->d_name, ".." ) != 0 )
					directories.push_back( entry->d_name );
					
				entry = readdir( dir );
			}

			
			return directories;
		}
		
		/**
		 * Used for testing purposes 
		 */
		void simpleStringPrinter( std::string string ) {
			std::cerr << string << std::endl; 
		}
	}
}
