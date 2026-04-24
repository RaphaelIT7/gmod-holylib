concommand.Add( "sv_whereis", function( _, _, _, path )
    local absolutePath = filesystem.RelativePathToFullPath( path, "GAME" )
    if ( !absolutePath or !file.Exists( path, "GAME" ) ) then
        MsgN( "File not found: ", path )
        return
    end

    local relativePath = filesystem.FullPathToRelativePath( absolutePath, "MOD" )
    -- If the relative path is inside the workshop dir, it's part of a workshop addon
    if ( relativePath && relativePath:match( "^workshop[\\/].*" ) ) then
        local addonInfo = util.RelativePathToGMA_Menu( path ) -- We cannot wrap this one

        -- Not here? Maybe somebody just put their own file in ./workshop
        if ( addonInfo ) then
            local addonRelativePath = filesystem.RelativePathToFullPath( addonInfo.File )

            MsgN( "'", addonInfo.Title, "' - ", addonRelativePath or addonInfo.File )
            return
        end
    end

    MsgN( absolutePath )
end, nil, "Searches for the highest priority instance of a file within the GAME mount path." )