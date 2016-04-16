-- Base Types

_classes.object = newclass();

function _classes.object:load( saved )
	local data = saved or {}

	self._cid = data._cid
	self._sys = data._sys
end

function _classes.object:save()
	local out = {
		_sys = self._sys,
		_cid = self._cid
	}

	return out
end

function _classes.object:setup()
	-- abstract - This is the "real" constructor of objects
end

--[[
	Sleep numTicks amount of ticks and then call the function on this object named by functionName
--]]
function _classes.object:sleep( method, numTicks, ... )
	local ticks = numTicks + bluebear.engine.current_tick

	self:register_callback( ticks, method, { ... } )
end

--[[
	Enter in a callback on this object's _sys._sched table for the destination tick
--]]
function _classes.object:register_callback( tick, method, wrapped_arguments )
	local ticks_key = tostring( tick )
	local ticks_table

	if self._sys._sched[ ticks_key ] == nil then
		self._sys._sched[ ticks_key ] = {}
	end

	ticks_table = self._sys._sched[ ticks_key ]

	table.insert( ticks_table, { method = method, arguments = wrapped_arguments } )
end

-- Private methods (do not override these!)
-- Called by the engine, runs all callbacks due for the given ticks
function _classes.object:_run()
	-- Is there a _sys._sched entry for currentTick?
	local currentTick = tostring( bluebear.engine.current_tick )
	local callbackList = self._sys._sched[ currentTick ]

	if type( callbackList ) == "table" then
		-- Clear the callback table from self._sys._sched
		self._sys._sched[ currentTick ] = nil

		-- Fire each callback in the table
		for index, callbackTable in ipairs( callbackList ) do
			self[ callbackTable.method ]( self, table.unpack( callbackTable.arguments ) )
		end
	end

end
-- be careful: if the methods are NOT virtual, "self" will not be the same self when you call it!
-- all methods should be made virtual by default by some type of modification to the yaci lib
_classes.object:virtual( "sleep" )
_classes.object:virtual( "_run" )

-- define promises: there may be several kinds of promises (e.g. timebased promises in pure Lua,
-- or engine-based promises that require the C++ engine to signal for the callback)
_classes.promise = {
	base = newclass()
}

function _classes.promise.base:init( obj_ref, start_ticks )
	self.object = obj_ref
	self.next_tick = start_ticks
end

function _classes.promise.base:then_call( func_name )

end
