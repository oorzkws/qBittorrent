-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 torlock plugin
-- Copyright (C) Douman <custparasite@gmx.se>
-- Copyright (C) Diego de las Heras <ngosang@hotmail.es>
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met then
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

name = "TorLock"
version = 1.0
url = "https://www.torlock.com"
supportedCategories = {
    all = "all",
    anime = "anime",
    software = "software",
    games = "game",
    movies = "movie",
    music = "music",
    tv  = "television",
    books = "ebooks"
}

function run(what, cat)
    what = what:gsub("%%20", "-")
    cat = supportedCategories[cat:lower()]
    
    local data = URL.get(string.format(
        "%s/%s/torrents/%s.html?sort=seeds&page=1",
        url,
        cat,
        what
    ))
    if data == "" then return end

    articleFound = false -- true when <article> with results is found
    itemFound = false
    itemBad = false -- set to True for malicious links
    currentItem = nil -- dict for found item
    itemName = nil -- key's name in current_item dict
    parserClass = { ts = "size", tul = "seeds", tdl = "leeches" }
    HTML.parse(data)
    
    local counter = 1
    local nextPagesPattern = string.format("/%s/torrents/%s.html?sort=seeds&page=[0-9]+", cat, what)
    local listSearches = {}
    for page in data:gmatch(nextPagesPattern) do
        if not listSearches[page] then
            listSearches[page] = true
            data = URL.get(url .. page)
            HTML.parse(data)
            counter = counter + 1
            if counter > 3 then break; end
        end
    end
end

function handleStartTag(tag, attrs)
    if itemFound and tag == "td" then
        if attrs["class"] then
            itemName = parserClass[attrs["class"]]
            if itemName then
                currentItem[itemName] = ""
            end
        end
    elseif articleFound and tag == "a" then
        if attrs["href"] then
            link = attrs["href"]
            if link:startswith("/torrent") then
                currentItem["descrLink"] = url .. link
                currentItem["link"] = url .. "/tor/" .. link:split("/")[3] .. ".torrent"
                currentItem["siteUrl"] = url
                itemFound = true
                itemName = "name"
                currentItem["name"] = ""
                itemBad = (attrs["rel"] == "nofollow")
            end
        end
    elseif tag == "article" then
        articleFound = true
        currentItem = {}
    end
end

function handleData(data)
    if itemName then
        currentItem[itemName] = currentItem[itemName] .. data
    end
end

function handleEndTag(tag)
    if tag == "article" then
        articleFound = false
    elseif itemName and (tag == "a" or tag == "td") then
        itemName = nil
    elseif itemFound and tag == "tr" then
        itemFound = false
        if not itemBad then
            newSearchResult(currentItem)
        end
        currentItem = {}
    end
end
