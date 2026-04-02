-- Stub to be used when running inside simulator.
--
-- For a few extra bytes in package size, we get a friendlier error message.

local text <const> = "\nThis game only runs on the device.\n\nPlease see sideload instructions:\nhttps://help.play.date/games/sideloading/\n"

-- Log help text to console.  This makes it easier to copy&paste.
print(text)

function playdate.update()
	-- Note that playdate.graphics functions are used here without importing
	-- CoreLib/graphics.  We don't need that import since clear() and
	-- drawText() are available through playdate's environment without any
	-- imports, and we don't need any of the pure Lua functions in CoreLib.
	-- Not having that import reduces main.pdz by a few kilobytes.
	playdate.graphics.clear()
	playdate.graphics.drawText(text, 20, 20)

	-- Stop updating after the first frame.
	playdate.update = function() end
end
