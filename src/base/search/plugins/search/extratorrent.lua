-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 extratorrent plugin
-- Copyright (C) Christophe Dumez <chris@qbittorrent.org>
-- Copyright (C) Diego de las Heras <ngosang@hotmail.es>
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met:
--
--    * Redistributions of source code must retain the above copyright notice,
--      this list of conditions and the following disclaimer.
--    * Redistributions in binary form must reproduce the above copyright
--      notice, this list of conditions and the following disclaimer in the
--      documentation and/or other materials provided with the distribution.
--    * Neither the name of the author nor the names of its contributors may be
--      used to endorse or promote products derived from this software without
--      specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
-- POSSIBILITY OF SUCH DAMAGE.

name = "ExtraTorrent"
version = 1.0
url = "http://extratorrent.cc"
supportedCategories = {
    all = 0,
    music = 5,
    movies = 4,
    games = 3,
    software = 7,
    books = 2,
    anime = 1,
    tv = 8,
    pictures = 6
}

function run(what, cat)
    local data = URL.get(url .. "/advanced_search/?" .. URL.urlencode({
        with = what,
        s_cat = supportedCategories[cat:lower()]
    }))
    if data == "" then return end

    listSearches = {}
    currentItem = nil
    currentItemName = nil
    pendingSize = false
    nextQueries = true
    pendingNextQueries = false
    nextQueriesSet = {}
    HTML.parse(data)
    
    for _, searchQuery in ipairs(listSearches) do
        data = URL.get(url .. searchQuery)
        HTML.parse(data)
    end
end

function handleStartTag(tag, attrs)
    if currentItem then
        if tag == "a" then
            local link = attrs["href"]

            if not link:startswith("/torrent") then
                return
            end

            if link:sub(9, 9) == "/" then
                -- description
                currentItem["descrLink"] = url .. link
                -- remove view at the beginning
                currentItem["name"] = attrs["title"]:sub(6, -8)
                pendingSize = true
            elseif link:sub(9, 9) == "_" then
                -- download link
                link = link:gsub("torrent_", "", 1)
                currentItem["link"] = url .. link
            end
        elseif tag == "td" then
            if pendingSize then
                currentItemName = "size"
                currentItem["size"] = ""
                pendingSize = false
            end

            if attrs["class"] then
                if attrs["class"]:startswith("s") then
                    currentItemName = "seeds"
                    currentItem["seeds"] = ""
                elseif attrs["class"]:startswith("l") then
                    currentItemName = "leeches"
                    currentItem["leeches"] = ""
                end
            end
        end
    elseif tag == "tr" then
        if attrs["class"] and attrs["class"]:startswith("tl") then
            currentItem = {}
            currentItem["siteUrl"] = url
        end
    elseif pendingNextQueries then
        if tag == "a" then
            if nextQueriesSet[attrs["title"]] then
                return
            end
            
            listSearches[#listSearches + 1] = attrs["href"]
            nextQueriesSet[attrs["title"]] = 1
            if attrs["title"] == "10" then
                pendingNextQueries = false
            end
        else
            pendingNextQueries = false
        end
    elseif nextQueries then
        if tag == "b" and attrs["class"] == "pager_no_link" then
            nextQueries = false
            pendingNextQueries = true
        end
    end
end

function handleData(data)
    if currentItemName then
        currentItem[currentItemName] = currentItem[currentItemName] .. " " .. data
        -- Due to utf-8 we need to handle data two times if there is space
        if currentItemName ~= "size" then
            currentItemName = nil
        end
    end
end

function handleEndTag(tag)
    if currentItem then
        if tag == "tr" then
            newSearchResult(currentItem)
            currentItem = nil
        end
    end
end
