tell application "Transmit"
	make new document at before front document
	tell current session of document 1
	    if (connect to favorite with name "amnoid.de/tmp") then	
			if (set your stuff to "/Users/nico/src/clangtut/clangtut/") then
				if (set their stuff to "clangtut") then
					synchronize method update direction upload files with time offset 0
				else
					display dialog ("An error occured, could not change remote folder to clangtut")
				end if
			else
				display dialog ("An error occured, could not change local folder to clangtut")
			end if
		end if
	end tell
end tell

