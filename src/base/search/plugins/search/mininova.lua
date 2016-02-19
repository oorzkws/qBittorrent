-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 mininova plugin
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

name = "Mininova"
version = 1.0
url = "http://www.mininova.org"
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
    local data = URL.get(string.format(
        "%s/search/%s/%s/seeds", 
        url, 
        what, 
        supportedCategories[cat:lower()]
    ))
    if data == "" then return end

    listSearches = {}
    currentItem = nil
    currentItemName = nil
    tableResults = false
    nextQueries = true
    HTML.parse(data)

    nextQueries = false
    for _, searchQuery in ipairs(listSearches) do
       data = URL.get(url .. searchQuery)
       HTML.parse(data)
    end
end

function handleStartTag_tr()
    currentItem = {}
end

function handleStartTag_a(attrs)
    link = attrs["href"]

    if link:startswith("/tor/") then
        -- description
        currentItem["descrLink"] = url .. link
        -- get download link from description by id
        currentItem["link"] = url .. "/get/" .. link:sub(6, -2)
        currentItemName = "name"
        currentItem["name"] = ""
    elseif nextQueries and link:startswith("/search") then
        if attrs["title"]:startswith("Page") then
            listSearches[#listSearches + 1] = link
        end
    end
end

function handleStartTag_td(attrs)
    if attrs["align"] == "right" then
        if not currentItem["size"] then
            currentItemName = "size"
            currentItem["size"] = ""
        end
    end
end

function handleStartTag_span(attrs)
    if attrs["class"] == "g" then
        currentItemName = "seeds"
        currentItem["seeds"] = ""
    elseif attrs["class"] == "b" then
        currentItemName = "leeches"
        currentItem["leeches"] = ""
    end
end

function handleStartTag(tag, attrs)
    if tableResults then
        local dispatcher = _G["handleStartTag_" .. tag]
        if dispatcher then
            dispatcher(attrs)
        end
    elseif tag == "table" then
        tableResults = (attrs["class"] == "maintable")
    end
end

function handleData(data)
    if currentItemName then
        currentItem[currentItemName] = currentItem[currentItemName] .. " " .. data
    end
end

function handleEndTag(tag)
    if tag == "tr" and currentItem then
        currentItem["siteUrl"] = url
        local count = 0
        for _ in pairs(currentItem) do count = count + 1 end
        if count > 4 then
            newSearchResult(currentItem)
        end
        currentItem = nil
    elseif currentItemName then
        if tag == "a" or tag == "td" then
            currentItemName = nil
        end
    end
end