-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 demonoid plugin
-- Copyright (C) Douman <custparasite@gmx.se>
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

name = "Demonoid"
version = 1.0
url = "https://www.demonoid.pw"
supportedCategories = {
    all = 0,
    music = 2,
    movies = 1,
    games = 4,
    software = 5,
    books = 11,
    anime = 9,
    tv = 3
}

function run(what, cat)
    for page = 1, 5 do
        local data = URL.get(url .. "/files/?" .. URL.urlencode({
            category = supportedCategories[cat:lower()],
            subcategory = "All",
            quality = "All",
            seeded = 2,
            external = 2,
            query = what,
            to = 1,
            uid = 0,
            sort = "S",
            page = page
        }))
        if data == "" then return end

        local i, j, response = data:find("start torrent list %-%->(.*)<!%-%- end torrent")
        if not response then break end

        currentItem = nil
        saveData = nil
        seedsLeechs = false
        HTML.parse(response)

        if not response:find("/files.-page=" .. (page + 1)) then
            -- no more results
            break
        end
    end
end

function handleStartTag(tag, attrs)
    if tag == "a" then
        if attrs["href"] then
            local link = attrs["href"]
            if link:startswith("/files/details") then
                currentItem = {}
                currentItem["descrLink"] = url .. link
                currentItem["siteUrl"] = url
                saveData = "name"
            elseif link:startswith("/files/download") then
                currentItem["link"] = url .. link
            end
        end
    elseif currentItem then
        if tag == "td" then
            if attrs["class"] and attrs["align"] then
                if attrs["class"]:startswith("tone") then
                    if attrs["align"] == "right" then
                        saveData = "size"
                    elseif attrs["align"] == "center" then
                        seedsLeechs = true
                    end
                end
            end
        elseif seedsLeechs == true and tag == "font" then
            if attrs["class"] == "green" then
                saveData = "seeds"
            elseif attrs["class"] == "red" then
                saveData = "leeches"
            end

            seedsLeechs = false
        end
    end
end

function handleData(data)
    if saveData then
        if saveData == "name" then
            -- names with special characters like '&' are splitted in several pieces
            if not currentItem["name"] then
                currentItem["name"] = ""
            end
            currentItem["name"] = currentItem["name"] .. data
        else
            currentItem[saveData] = data
            saveData = nil
        end

        local count = 0
        for _ in pairs(currentItem) do count = count + 1 end
        if count == 7 then
            currentItem["size"]:gsub(",", "")
            newSearchResult(currentItem)
            currentItem = nil
        end
    end
end

function handleEndTag(tag)
    if saveData == "name" then
        saveData = nil
    end
end
