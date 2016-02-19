-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 legittorrents plugin
-- Copyright (C) Christophe Dumez <chris@qbittorrent.org>
-- Copyright (C) Douman <custparasite@gmx.se>
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

name = "Legit Torrents"
version = 1.0
url = "http://www.legittorrents.info"
supportedCategories = {
    all = 0,
    music = 2,
    movies = 1,
    games = 3,
    books = 6,
    anime = 5,
    tv = 13
}

function run(what, cat)
    local tablePattern = "<table%sclass=\"lista\".*>(.*)</table>"
    
    local data = URL.get(url .. "/index.php?" .. URL.urlencode({
        page = "torrents",
        search = what,
        category = supportedCategories[cat:lower()],
        active = 1
    }))
    if data == "" then return end
    
    local i, j = data:find(tablePattern)
    if not i then return end
    
    currentItem = nil
    saveItemKey = nil
    
    data = data:sub(i, j)
    HTML.parse(data)
    
    -- extract first ten pages of next results
    local k = 10
    local nextPages = {}
    for nextPage in data:gmatch("<option value=\"([^\n]*)\">[0-9]+</option>") do
        nextPage = nextPage:gsub("&amp;", "&")
        if not nextPages[nextPage] then
            nextPages[nextPage] = true
            
            data = URL.get(url .. nextPage)
            if data == "" then return end
            
            i, j = data:find(tablePattern)
            if not i then break end
            
            data = data:sub(i, j)
            HTML.parse(data)
            
            k = k - 1
            if k == 0 then break end
        end
    end
end

function handleStartTag(tag, attrs)
    if currentItem then
        if tag == "a" then
            local link = attrs["href"]
            if link:startswith("index") and attrs["title"] then
                -- description link
                currentItem["name"] = attrs["title"]:sub(15)
                currentItem["descrLink"] = url .. "/" .. link
            elseif link:startswith("download") then
                currentItem["link"] = url .. "/" .. link
            end
        elseif tag == "td" then
            if attrs["class"] and attrs["class"]:startswith("#FF") then
                if currentItem["seeds"] then
                    saveItemKey = "leeches"
                else
                    saveItemKey = "seeds"
                end
            end
        end
    elseif tag == "tr" then
        currentItem = {}
        currentItem["size"] = ""
        currentItem["siteUrl"] = url
    end
end

function handleData(data)
    if saveItemKey then
        currentItem[saveItemKey] = data:strip()
        saveItemKey = nil
    end
end

function handleEndTag(tag)
    if currentItem and tag == "tr" then
        local count = 0
        for _ in pairs(currentItem) do count = count + 1 end
        if count > 4 then
            newSearchResult(currentItem)
        end
        currentItem = nil
    end
end
