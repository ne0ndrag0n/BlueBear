#include "lotentity.hpp"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "utility.hpp"
#include "json/json.h"
#include <iostream>
#include <cstring>
#include <string>

namespace BlueBear {

	Json::FastWriter LotEntity::writer;

	LotEntity::LotEntity( lua_State* L, std::string& classID )
		: L( L ), classID( classID ) {
			createEntityTable();
	}

	LotEntity::LotEntity( lua_State* L, Json::Value& serialEntity )
		: L( L ), classID( serialEntity[ "classID" ].asString() ) {
		createEntityTable();
		if( ok ) {
			ok = false;

			deserializeEntity( serialEntity );
		}
	}

	void LotEntity::createEntityTable() {
		// Get bluebear.classes
		// bluebear
		lua_getglobal( L, "bluebear" );

		// bluebear.classes bluebear
		Utility::getTableValue( L, "classes" );

		// Get the actual class referred to by this classID
		// Class bluebear
		Utility::getTableTreeValue( L, classID );

		// If "nil bluebear": Cannot continue: the class was not found
		if( lua_istable( L, -1 ) ) {
			// Class is at the top of the stack
			// Let's start by creating a new instance

			// <new> Class bluebear
			Utility::getTableValue( L, "new" );

			// Class <new> Class bluebear
			lua_pushvalue( L, -2 );

			// instance Class bluebear
			if( lua_pcall( L, 1, 1, 0 ) == 0 ) {
				// Instance is created!
				ok = true;

				// this->luaVMInstance holds a Lua registry index to the table returned by this function
				// Class bluebear
				luaVMInstance = luaL_ref( L, LUA_REGISTRYINDEX );

				// EMPTY
				lua_pop( L, 2 );
			} else {
				// error instance Class bluebear
				std::cout << lua_tostring( L, -1 ) << std::endl;
				lua_pop( L, 4 );
			}
		} else {
			std::cout << "Could not find class " << classID << std::endl;
			lua_pop( L, 2 );
		}
	}

	/**
	 * Load from JSON or other saved format.
	 */
	void LotEntity::deserializeEntity( Json::Value& serialEntity ) {

		// Grab a reference to the instance table
		// instance
		lua_rawgeti( L, LUA_REGISTRYINDEX, luaVMInstance );

		// Use the load method to deserialise
		// <load> instance
		Utility::getTableValue( L, "load" );

		// instance <load> instance
		lua_pushvalue( L, -2 );

		// Use JSON.decode( JSON, "serialisedInstance" ) to transform the serialised instance to a table
		// JSON instance <load> instance
		lua_getglobal( L, "JSON" );

		// <decode> JSON instance <load> instance
		Utility::getTableValue( L, "decode" );

		// JSON <decode> JSON instance <load> instance
		lua_pushvalue( L, -2 );

		// "serialisedInstance" JSON <decode> JSON instance <load> instance
		lua_pushstring( L, writer.write( serialEntity[ "instance" ] ).c_str() );

		// deserialisedTable JSON instance <load> instance
		if( lua_pcall( L, 2, 1, 0 ) == 0 ) {
			// Prepare for the pcall that loads
			// deserialisedTable instance <load> instance
			lua_remove( L, -2 );
		} else {
			// error JSON instance <load> instance
			std::cout << "Problem deserialising using Lua JSON lib on " << classID << std::endl;
			lua_pop( L, 5 );
			return;
		}

		// instance
		if( lua_pcall( L, 2, 0, 0 ) == 0 ) {
			// Everything's ok!
			ok = true;

			// Grab the _cid of the LotEntity and set the public "cid" property to this value
			// cid instance
			Utility::getTableValue( L, "_cid" );
			cid = lua_tostring( L, -1 );
			// Then, clear the value off the stack, ensuring the instance is at the top of the stack
			// EMPTY
			lua_pop( L, 2 );
		} else {
			// error instance
			std::cout << lua_tostring( L, -1 ) << std::endl;
			lua_pop( L, 2 );
		}
	}

	void LotEntity::execute( unsigned int currentTick ) {
		// Push this object's table onto the API stack
		lua_rawgeti( L, LUA_REGISTRYINDEX, luaVMInstance );

		// Get the object's _sys table
		Utility::getTableValue( L, "_sys" );
		// Get the nested _sched table
		Utility::getTableValue( L, "_sched" );

		// Create the key we will need from worldTicks
		std::string tickKey = std::to_string( currentTick ) + ".0";

		// Check if tickKey has an associated value in _sched
		Utility::getTableValue( L, tickKey.c_str() );

		if( lua_istable( L, -1 ) ) {
			// There's functions that need to be executed
			// Use lua_next to get the objects that describe how to call these functions
			lua_pushnil( L );
			while( lua_next( L, -2 ) != 0 ) {
				// The Serialised Function Table (SFT) is the value, available on -1
				// The key is the index position, available on -2

				// STEP 1: Get the named function and push a closure with "self"
				// a. Push the name of the function onto the stack ( SFT[ "method" ] )
				Utility::getTableValue( L, "method" );
				// b. Save this result, then pop it
				std::string functionName = lua_tostring( L, -1 );
				lua_pop( L, 1 );
				// c. Re-push object (it is currently at -6)
				lua_pushvalue( L, -6 );
				// d. Get the function out of the object
				Utility::getTableValue( L, functionName.c_str() );
				// e. Remember the object that we just used? Re-push it as the first object argument (self)
				lua_pushvalue( L, -2 );

				// STEP 2: Push all the function's arguments and call!
				// a. Re-grab the SFT: it should be at position -4
				lua_pushvalue( L, -4 );
				// b. Grab the "arguments" array-table
				Utility::getTableValue( L, "arguments" );
				// c. Now, time to start keeping the Lua stack cleaned up a bit; REMOVE the SFT now at position -2
				lua_remove( L, -2 );
				// d. The arguments array is now at the top of the stack. How many are in it?
				// This will always be at least 1 (because of self)
				int totalArguments = lua_rawlen( L, -1 ) + 1;

				// Without accounting for self, count the number of arguments that need to be unrolled
				if( totalArguments - 1 > 0 ) {
					// e. Use this lovely loop to spit everything in the array out onto the stack
					for( int i = 1; i != totalArguments; i++ ) {
						lua_rawgeti( L, -i, i );
					};
				}

				// f. Remove the actual array, which should be at -totalArguments
				// In the case of empty array - totalArguments should be 1 (0+1, the "self" arg)
				// In other cases, it should just be the negative of the length of the array
				lua_remove( L, -totalArguments );

				// g. Call that sumbitch!
				if( lua_pcall( L, totalArguments, 0, 0 ) != 0 ) {
					// We only get here if the function bombs out
					std::cout << "Error in lot entity: " << lua_tostring( L, -1 ) << std::endl;
					lua_pop( L, 1 );

					ok = false;
				}

				// h. There's two junk items remaining on the stack; clean 'em up!
				// These two items should be the object table, followed by the SFT
				// leaving the key at -1 and the table at -2 (see how the loop restarts?)
				lua_pop( L, 2 );
			}
		}

		// Start popping all this crap off the stack
		// The remaining items should always be the tick array table (or nil), _sys._sched, _sys, and the object table
		lua_pop( L, 4 );
	}
}
