local Class = bluebear.extend( 'object', {

	catalog = {
		name = "bluebear trashpile",
		description = "static tile of a trashpile",
		price = 0
	},

	actions = {

	},

	-- This function will run when the object is scheduled to update its status.
	main = function( self )
		print( "Hello from Lua! I am object instance ("..self._cid..")" )

		self:sleep( 'main', 86400 )
	end,

	load = function( self, saved )
		self.super:load( saved )

		self.stink = saved.stink
	end
} )


bluebear.register_class( "game.household.trashpile.base", Class )
