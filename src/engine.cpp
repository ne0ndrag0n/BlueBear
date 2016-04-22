#include "bbtypes.hpp"
#include "utility.hpp"
#include "lotentity.hpp"
#include "lot.hpp"
#include "engine.hpp"
#include "json.hpp"
#include <iterator>
#include <string>
#include <vector>
#include <iostream>

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
		Utility::doDirectories( L, BLUEBEAR_OBJECTS_DIRECTORY );

		return true;
	}

	/**
	 * Load a lot
	 */
	bool Engine::loadLot( const char* lotPath ) {
		std::ifstream lot( lotPath );

		if( lot.is_open() && lot.good() ) {

			// Shitty library using exceptions!!
			try {
				json lotJSON( lot );

				std::cout << "[" << lotPath << "] " << "Lot revision: " << lotJSON[ "rev" ] << std::endl;

				// Instantiate the lot
				int terrain = lotJSON[ "terrain" ];
				this->currentLot.reset(
					new BlueBear::Lot(
						lotJSON[ "floorx" ],
						lotJSON[ "floory" ],
						lotJSON[ "stories" ],
						lotJSON[ "subtr" ],
						BlueBear::TerrainType( terrain )
					)
				);

				// Create one lot table for the Luasphere - contains functions that we call on this->currentLot to do things like get other objects on the lot and trigger events
				this->createLotTable();

				this->worldTicks = lotJSON[ "ticks" ];

				this->currentLot->objects.clear();

				// Iterate through the "entities" array: each object within is a serialised LotEntity
				json entities = lotJSON[ "entities" ];
				for( json& element : entities ) {
					std::string classID = element[ "classID" ];
					std::string instance = element[ "instance" ].dump();
					std::string cid = element[ "instance" ][ "_cid" ];

					this->currentLot->objects.emplace( cid, BlueBear::LotEntity( this->L, classID.c_str(), instance.c_str() ) );
				}
			} catch( ... ) {
				std::cout <<  "Failed to load lot: Library threw exception for lot " << lotPath <<  std::endl;
				return false;
			}

			// After the lot has all its LotEntities loaded, let's fix those serialized function references
			this->deserializeFunctionRefs();
		}

		return true;
	}

	/**
	 * For all serialized curried functions in _sys._sched, ask each object to deserialize the reference to another LotEntity
	 */
	void Engine::deserializeFunctionRefs() {

		// Get fresh Lua stack
		Utility::clearLuaStack( this->L );

		for( auto& keyValuePair : this->currentLot->objects ) {
			BlueBear::LotEntity& currentEntity = keyValuePair.second;

			// Push this object's table onto the API stack
			lua_rawgeti( this->L, LUA_REGISTRYINDEX, currentEntity.luaVMInstance );
			// Get the reserved function "_deserialize_function_refs"
			Utility::getTableValue( this->L, "_deserialize_function_refs" );
			// First argument is that same table (the context)
			lua_pushvalue( this->L, -2 );

			// Call _deserialize_function_refs: The rest is done in Lua
			if( lua_pcall( this->L, 1, 0, 0 ) != 0 ) {
				std::cout << lua_tostring( L, -1 ) << std::endl;
			}
		}

		// Clear stack when done
		Utility::clearLuaStack( this->L );
	}

	/**
	 * We do this once; create the lot table and assign it a lot instance
	 */
	void Engine::createLotTable() {

		// Get "dumb pointer" from smart pointer
		Lot* lot = this->currentLot.get();

		// Push the "bluebear" global onto the stack, then push the "lot" identifier
		// We will set this at the very end of the function
		lua_getglobal( L, "bluebear" );
		lua_pushstring( L, "lot" );

		// Push new, blank table
		lua_createtable( this->L, 0, 1 );

		// get_all_objects retrieves all objects
		lua_pushstring( L, "get_all_objects" );
		lua_pushlightuserdata( L, lot );
		lua_pushcclosure( L, &Lot::lua_getLotObjects, 1 );
		lua_settable( L, -3 );

		// get_objects_by_type gets all objects on the lot of a specific type
		lua_pushstring( L, "get_objects_by_type" );
		lua_pushlightuserdata( L, lot );
		lua_pushcclosure( L, &Lot::lua_getLotObjectsByType, 1 );
		lua_settable( L, -3 );

		// get_object_by_cid retrieves a specific object by its cid
		lua_pushstring( L, "get_object_by_cid" );
		lua_pushlightuserdata( L, lot );
		lua_pushcclosure( L, &Lot::lua_getLotObjectByCid, 1 );
		lua_settable( L, -3 );

		// Remember pushing the bluebear table, then lot? Stack should now have the lot table,
		// the "lot" identifier, then the bluebear global. Go ahead and set "lot" to this table.
		lua_settable( L, -3 );
	}

	/**
	 * Where the magic happens
	 */
	void Engine::objectLoop() {
		std::cout << "Starting world engine with a tick count of " << this->worldTicks << "\n";

		for( ; this->worldTicks != 500000; this->worldTicks++ ) {

			// Clear the API stack of the Luasphere
			Utility::clearLuaStack( this->L );

			// Make note of current tick for all objects on the bluebear.engine status table
			lua_getglobal( L, "bluebear" );
			Utility::getTableValue( L, "engine" );
			lua_pushstring( L, "current_tick" );
			lua_pushnumber( L, this->worldTicks );
			lua_settable( L, -3 );

			for( auto& keyValuePair : this->currentLot->objects ) {
				BlueBear::LotEntity& currentEntity = keyValuePair.second;

				currentEntity.execute();
			}
		}

		std::cout << "Finished!" << std::endl;
	}

	/**
	 * Given the cid of a player, the cid of an object, and its desired action, call the action method on the LotEntity
	 * by passing the player identified by playerId as the "player" argument.
	 */
	void Engine::callActionOnObject( const char* playerId, const char* obejctId, const char* method ) {

	}

}
