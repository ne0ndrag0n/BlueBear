--[[
  Applies methods to the "bluebear" global that concern the registration of motives, and defines a few
  basic motives.
--]]

bluebear.engine.require_modpack( "yaci" )
bluebear.engine.require_modpack( "class" )

local Motive = newclass()

Motive.motive_name = "Root Motive"
Motive.motive_group = "Motive Group"

function Motive:init( doll )
  self.doll = doll
  self.value = 50
end

function Motive:decay()
  if self.value > 0 then
    self.value = self.value - 1
  end
end

function Motive:increment_by( value )
  self:set_value( self.value + value )
end

function Motive:decrement_by( value )
  self:set_value( self.value - value )
end

function Motive:set_value( value )
  if value > 100 then
    self.value = 100
  elseif value < 0 then
    self.value = 0
  else
    self.value = value
  end
end

bluebear.register_class( "system.motive.base", Motive )

-- Convience methods that are set up on the BlueBear object
bluebear.motives = {}
bluebear.register_motive = function( class_name, Motive )
  -- Register the motive as an ordinary class
  bluebear.register_class( class_name, Motive )

  -- Push the class onto the table
  table.insert( bluebear.motives, Motive )
end
