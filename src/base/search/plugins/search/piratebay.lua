-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 piratebay plugin
-- Copyright (C) Fabien Devaux <fab@gnux.info>
-- Copyright (C) Christophe Dumez <chris@qbittorrent.org>
-- Copyright (C) Arthur <custparasite@gmx.se>
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

name = "The Pirate Bay"
version = 1.0
url = "https://thepiratebay.se"
supportedCategories = {
    all = 0,
    music = 100,
    movies = 200,
    games = 400,
    software = 300
}

function run(what, cat)
    cat = supportedCategories[cat:lower()]
    
    -- 7 is filtering by seeders
    local data = URL.get(string.format("%s/search/%s/%d/%d/%s", url, what, 0, 7, cat))
    if data == "" then return; end

    listSearches = {}
    saveItem = nil
    resultTable = false -- table with results is found
    resultTbody = false
    resultQuery = false
    currentItem = nil
    nextQueries = true
    HTML.parse(data)

    nextQueries = false
    for _, searchQuery in ipairs(listSearches) do
       data = URL.get(url .. searchQuery)
       HTML.parse(data)
    end
end

function handleStartTag_a(attrs)
    -- Handler for start tag a
    local link = attrs["href"]
    if link:startswith("/torrent") then
        currentItem["descrLink"] = url .. link
        saveItem = "name"
    elseif link:startswith("magnet") then
        currentItem["link"] = link
        -- end of the "name" item
        currentItem["name"] = currentItem["name"]:strip()
        saveItem = nil
    elseif link:startswith("/user") and saveItem == "size" then
        -- end of the "size" item
        local i, _, size = currentItem["size"]:find("Size%s+([%d%.]+[^%a]+%a+)")
        if i then
            currentItem["size"] = size
        end
        saveItem = nil
    end
end

function handleStartTag_font(attrs)
    -- Handler for start tag font
    if attrs["class"] == "detDesc" then
        saveItem = "size"
    end
end

function handleStartTag_td(attrs)
    -- Handler for start tag td
    if attrs["align"] == "right" then
        if currentItem["seeds"] then
            saveItem = "leeches"
        else
            saveItem = "seeds"
        end
    end
end

function handleStartTag(tag, attrs)
    if currentItem then
        local dispatcher = _G["handleStartTag_" .. tag]
        if dispatcher then
            dispatcher(attrs)
        end
    elseif resultTbody then
        if tag == "tr" then
            currentItem = { siteUrl = url }
        end
    elseif tag == "table" then
        resultTable = (attrs["id"] == "searchResult")
    elseif nextQueries then
        if resultQuery and tag == "a" then
            if #listSearches < 10 then
                listSearches[#listSearches + 1] = attrs["href"]
            else
                nextQueries = false
                resultQuery = false
            end
        elseif tag == "div" then
            resultQuery = (attrs["align"] == "center")
        end
    end
end

function handleData(data)
    if not saveItem then return; end
    
    if saveItem == "size" or saveItem == "name" then
        -- names with special characters like "&" are splitted in several pieces
        if not currentItem[saveItem] then
            currentItem[saveItem] = ""
        end
        currentItem[saveItem] = currentItem[saveItem] .. data
    else
        currentItem[saveItem] = data
        saveItem = nil
    end
end

function handleEndTag(tag)
    if resultTbody then
        if tag == "tr" then
            newSearchResult(currentItem)
            currentItem = nil
        elseif tag == "font" then
            saveItem = nil
        elseif tag == "table" then
            resultTable = false
            resultTbody = false
        end
    elseif resultTable then
        if tag == "thead" then
            resultTbody = true
        elseif tag == "table" then
            resultTable = false
            resultTbody = false
        end
    elseif nextQueries and resultQuery then
        if tag == "div" then
            nextQueries = false
            resultQuery = false
        end
    end
end